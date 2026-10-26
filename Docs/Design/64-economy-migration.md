# 64. Bringing the model into the game

The user, 2026-10-14: translate doc 63's simulation into the game itself; there's a lot of work to tweak the game to
match it.

Doc 63 is the specification. `tools/econ_model/model.py` is the reference behaviour: a rule is "done" in the game when
the game, measured the same way, behaves as the model does. This document maps each of the model's rules onto the
code that does that job today, sets out where the game has to differ from the model, and orders the work into phases.

Read from the code 2026-10-14. Paths are relative to the repository. Line numbers are as of 9901668.

## Ground rules

1. **The model is the spec.** Every phase is measured with the same measures as `model.py`'s `measure()` (Phase 0),
   and compared side by side with a model run.
2. **Re-found, don't migrate.** It's a test world, and the user's standing rule is no data migrations. The economy's
   saved state (`SocietyState`: accounts, tills, Houses, careers, the ledger, the orchestrator) is founded afresh on
   DEV where a phase needs it. The world itself, its residents and their homes and posts, stays as it is. PROD pushes
   stay the user's.
3. **Money is conserved at every step.** `conserved()` stays true in every test and every run. The only coin struck or
   melted after the founding is the mint's (Phase 11), and it is logged.
4. **One rule at a time; the orchestrator retires piece by piece.** Each part goes only once its replacement is
   measured to work: prices, the wage table, channels, funds, banks, bands.
5. **Tick cost stays low.** The model's daily, weekly and monthly reviews run staggered (a business on its own day of
   the week). The perf gate is unchanged (`world_check --players 20`, doc 31).
6. **Where the game can't do what the model does, adapt and write it down here.** The model is a sketch of 890 people
   and 17 goods with no bodies. The game has about 1,080 residents who walk to work, about 430 items, seven towns,
   roads, caravans and players.

## Where the game stands

| Area | Today | The model | Size of the gap |
|---|---|---|---|
| **Money** | 500,000p (`WorldMoney`, for ≥500 residents) spread evenly. Mid-game mints: a newly sorted House's 3,000p, late town and church accounts, newcomers' and births' purses (`adoptResident`), `restore`. Banks (`bank:<town>`). | 1,000,000p founded once, 40% households (uneven), 15% businesses, 30% Houses, 5% church, 10% treasury. No mints, no banks. | Small |
| **Prices** | One price per good per town from the orchestrator (catalogue × shelf × `priceLift`, 0.6–1.6×, support for staples). Producers paid price × margin. Caravans ×1.4. No record of what stock cost. | Each seller prices at normal cost × (1 + its margin). The margin follows its shelf and wears back toward 15%; −30%/+60% (+100% for staples). | **Large**: needs a cost basis |
| **Production** | A batch every `seconds/86400` days, made by the keeper alone. Help and hands add nothing. Output capped by stock targets (`GoodsKept`, `ProducerKept`). | Every hand's hours make goods; output bounded by hours and inputs. Fields sown and harvested; grain stored for the year. | **Large** |
| **Businesses** | Keeper's wage plus a third of weekly profit. Houses sweep their tills nightly, prop them up and sell failing ones. Rent a flat 20p a month. Rescue loans from orchestrator funds. | Wages daily, a month of working money kept, the rest to the owner monthly. Owner tops up (capped). A week of trade credit. Rent a share of takings. | Medium |
| **Wages and hiring** | A town wage table by kind (orchestrator). Posts are authored positions. The jobless take outwork or the Works. Hires and odd jobs pay by the job. | Each employer's own wage, moved by its open posts. Hires by its sales. Workers judge posts by actual pay. Poaching, the harvest call, masters to their trade. | **Large** |
| **Households** | About eight overlapping rules (food, needs, wants, stipends, savers, deposits, feasts, the plate). Purses per resident, evened daily. | One rule: needs, then a share of what's held over k × the household's income, into a want fund and home help. Servants for the very rich. | Medium |
| **Institutions** | Tax and tithe on weekly *profit* (about 10%, banded), a wealth tithe and a levy. One shared church. Town buyers. Orchestrator bands and channels spend the rest. | Tax 8% and tithe 6% of *income*, weekly. Each institution: its income + a2 × (held − k × income). Church: relief, clergy, masons. Town: watch, works as employer of last resort. Houses: retainers, finery, works, estate crew. | Medium |
| **Lives** | Ages advance on World entities. Wages stop at 65. Death only from 70, by bands. Births only while a town is under its founding number. Marriage, newcomers. Estates of cash, meal and herbs to family. Succession: apprentice, family, a local. | Retire at 58–66. Death by age (Gompertz). Births follow deaths into young families. Households split above six. Kin inherit everything. Succession to the best hand. | Medium: adapt |
| **Skills** | Skill per resident and post. Practice at the post, ×3 beside a master. One apprentice per post, unpaid, done at 70. Skill doesn't affect pay or who may work a trade. | Trades need skill. Learning beside a better hand, up to the trade's level. Apprentices on half pay, bound. Masters' trades seek masters. The user's succession rule. | Medium |
| **Mending** | Players only (`RatwDurability.cpp`). NPCs rebuy household needs and tools on a timer. | NPCs own pieces that wear, and choose to mend or replace. Mending costs the maker hours and materials. | Medium |
| **Carriage** | Caravans with escorts and bandits sell at ×1.4. Standing orders and porters. | A carter by weight. A business hauls its own when the carts are full. | Small–medium |
| **Hunting** | NPC outwork "hunting" from the same finite game players use. | The idle hunt; hides to tanneries; sheep runs give hides too. | Small |
| **Players** | Stalls (3p), town projects hiring residents, Chapter Halls and Holds with sworn residents but no payroll. No mint. | The mint (a thermostat on players' dormant money). Keeps hiring NPCs, drawing on the towns. | Medium |
| **Measures** | `econ_watch` writes money by holder, Gini, purses, hunger, flows, wages, prices, larders and trade. | `measure()`: prices in labourer's day-wages, the market wage, masters, apprentices, elders, births, deaths, hunting, a monthly flows table by holder. | Small: mostly new columns |

**Size:**

- Economy core: about 9,350 lines.
- The orchestrator: about 3,470 lines (`RatwOrchestrator.cpp/.h`, `RatwOrchestrate.cpp`, `RatwChannels.cpp`).
- `RatwSociety.h`: 1,221 lines.

**Tests that pin today's behaviour:**

- `roads_tests.cpp`: about 40 economy cases.
- `orchestrator_tests.cpp`: 45 references.
- Also `crafting_tests`, `careers_tests`, `society_tests` and others.

## How the game must differ from the model

These are the adaptations. Each is decided here, or put to the user at the end.

### A. The penny

At 1,000,000p the model prices a loaf at about 0.7p. The game rounds to whole pennies (`pennies()`), and catalogue
bread is 1p.

**Proposal: count in farthings.** The ledger keeps farthings (a quarter penny), and prices show as pennies and
farthings ("2p 1f"). The money supply becomes 4,000,000 farthings.

- A loaf is about 3f, a labourer's day about 48f (12p), a bronze sword about 36p, a rapier about 300p.
- It's medieval; it keeps every staple above one coin; and nothing about the economy changes, only the unit.

The other option is a bigger money supply (about 4,000,000p, a labourer's day about 48p). It's the same thing without
the coin, but it doubles every price players see against the catalogue. Question 1.

### B. Posts and hands

The model's businesses hire and lay off freely. The game's posts are authored positions with work spots and routes,
and NPCs walk to them.

**Adaptation:**

- **A business's hands are its filled posts plus its hires.**
- **Its "posts offered" are its empty authored posts, plus hires** for anything beyond them (the existing hires
  machinery in `RatwOddJobs.cpp`).
- **Laying off leaves a post empty.** Its holder joins the labour market and keeps its home.
- **Every hand at the station adds its hours to the business's output**, not only the keeper. This is the change that
  makes output follow labour.

Question 3.

### C. A cost basis on fungible stock

Stock is a count per account per item. Building prices from cost needs **a parallel map of average cost per account
per item**, updated whenever goods are made (the hours at the maker's wage, plus inputs at their average cost, plus
tool wear) or bought (the price paid, plus carriage). It's saved with the account. Re-founding (rule 2) sets it from
the ladder at the start.

### D. 430 items, not 17

**Every item gets a worth**, from the ladder (`tools/econ_model/ladder.json`), placed or by default from its catalogue
price.

**Every NPC-made item gets calibrated work hours** (`proposed_rates.json`). The calibration's conflict list (110
recipes whose inputs cost more than the product) is resolved by raising those items' worth to their built cost.

**Goods a shop buys and resells** (a general store's) are priced at their purchase cost plus its margin, like the
model's.

### E. Hourly spells, daily rules

The model decides by the day. The game pays and produces by the spell (a game hour) and runs its society once a day.

- **Making** stays per spell. The model's day maps onto the game's daily pass, in the model's order: lives, firms'
  making, households, relief, leftovers, help, hunting, services' wages, the labour market.
- **Weekly reviews** run on each business's own day (its offset).
- **Monthly payouts** run on each business's own day of the month.

### F. Towns and roads

The model has one city and one countryside joined by a carter. The game has seven towns, roads, caravans, porters and
standing orders.

**Adaptation:**

- **Carriage between towns is priced by weight and distance**, from the carrier's hours. This replaces the caravans'
  flat ×1.4 markup.
- **A business short of carriage sends its own hands** (the model's self-haul).
- **Caravans stay physical**, with bandits; what they carry costs its carriage.
- **Each town has its own labour market, wages and prices.** Wolves may move between towns for work (doc 61's porters
  already travel).

### G. People

- **Ages already live on World entities** (`RatwAging.cpp`), and marriage, births and newcomers exist
  (`RatwCareers.cpp`, `RatwWorld.cpp`). So the model's lives are an adaptation, not new work:
  - Gompertz deaths replace the bands from 70;
  - retirement at 58–66 replaces 65;
  - births follow deaths into young families, alongside marriage;
  - estates carry everything to kin.
- **Households splitting above six need a home to go to:** an empty house, a room let, or a bunkhouse bed. Where none
  is free, the household stays together. Question 4.
- **Purses stay per resident.** The household rule applies to the household's pooled money, as
  `RatwHouseholds.cpp` already evens it daily.

### H. Keeps are Holds

The game's Chapter Halls and Holds (`RatwGameHolds.cpp`) already have sworn and settled residents, but no payroll.
They become doc 63's keeps:

- up to three workshops and a farm, with NPC hands paid by the Hold's players;
- houses filled by wolves moving from the towns (the user's "draw" rule).

## Phases

Each phase lists what changes, what it retires, its tests, and when it's done. **"Measured"** means: an `econ_watch`
run of the stated length on DEV's export, compared with a model run of the same length by `tools/econ_model`'s
comparison script (Phase 0), every listed measure within its band (see "Done when" at the end).

### Phase 0: the measures and the comparison

- `econ_watch` gains the model's measures:
  - prices in labourer's day-wages for bread, wheat, a sword, a rapier and each rung's sample item;
  - the market wage;
  - masters, apprentices, elders, births, deaths and households;
  - hunted beasts and hides;
  - idle wolves (no post, no help that day);
  - days with more than 50 and more than 300 hungry;
  - **a monthly flows table by kind of holder,** whose columns must sum to zero: the audit.
- **`tools/econ_model/compare.py`** runs the model and reads an `econ_watch` run, then prints both side by side, decade
  or month by month.
- **A long-run harness:** `econ_watch` for a year or more on the deterministic export, run in the background.
- **Done when** today's game is measured and compared, as the "before".

### Phase 1: the books

- **Re-found the economy.**
  - 1,000,000p (or 4,000,000f, Question 1) for the realm, split as the model does: households 40% (uneven), businesses
    15%, Houses 30% (50/30/20), church 5%, treasuries 10%.
  - Houses get their fortunes at the founding.
- **No mid-game mints:**
  - `sortHouses` gives no money (a newly sorted House starts with what its businesses earn);
  - late town and church accounts are opened from the treasury;
  - newcomers arrive with nothing, and the town's relief takes them in;
  - a birth's purse comes from its parents;
  - `restore` mints nothing.
- **Banks removed.** Savers are repaid at re-founding (moot on a fresh founding).
- **The carter's goods from nothing** stop. Those goods come from producers.
- **Retires:** `worldMoney`'s top-up, `HouseFortune` mid-game, `bankReckoning`, `tendBank`, `bankWorths`, `payDue`.
- **Tests:**
  - `conserved()` across a founding;
  - no `minted` change after day 0 (except the mint, later);
  - `roads_tests` bank cases removed.
- **Done when** a 28-day run mints nothing after the founding.

### Phase 2: the worth ladder in the catalogue

- **`items.json` gains `worth`** (`rung`, `at`) for placed items, and **`playerSeconds`: 0** for every item (the
  user's choice: player craft time set per item, later, in Atlas and the DM).
- **`ladder.json` moves to `Data/Economy/ladder.json`** and `ladder.py` into `tools/item_catalog.py`'s checks.
- **The calibrated NPC work hours** go into `crafts.json` (producers' `seconds`, crafts' `seconds`). They're kept
  apart from player crafting time in `recipes.json`.
- **The 110 conflicts:** each item's worth is raised to its built cost, or its recipe changed. They're listed in the
  phase's notes for the user to glance over.
- **Labour budget check:** a head's daily needs at about 1.7 labourer's hours (doc 63).
- **Done when** `item_catalog.py` passes, and `calibrate.py` run on the game's data reports no unplaced conflict.

### Phase 3: production by hours

- **Every hand's hours make goods.** A post's spell at the station adds its hours to its business's work. Batches come
  out when the hours for one are done, as far as inputs allow. Producers' yields come from hours too.
- **Output follows sales**, with the model's shelf target (`keep` days; staples 20% spare). It replaces `GoodsKept`,
  `ProducerKept` and `SuppliesKept` as the bound.
- **Tool wear** counts into a batch's cost. Tools are bought by the business, not out of the worker's purse.
- **Seasonal fields:**
  - farms sow in spring and harvest at the end of summer, by hours per field;
  - the store lasts the year, spoiling a little;
  - acres are planned from last year's sales or the last month's rate, with a tenth over, and half again after a dry
    year;
  - sowing and harvest call day labour (Phase 6).
- **Wood laid in for winter** a month ahead.
- **Retires:** stock-target production, `SeasonalStore`, the producers' `ProducerKept` cap.
- **Tests:** `crafting_tests` rewritten around hours; `makersKeepWhatIsOrdered` and its kin re-pinned.

### Phase 4: what things cost, and prices

- **Average cost** per account per item (adaptation C), saved.
- **Each seller's price** is normal cost (hours at its wage, inputs, wear, rent) × (1 + its margin).
  - The margin moves weekly by shelf days against a band and wears back toward 15% between.
  - Caps: −30%/+60%, +100% for staples.
- **Producers are paid the price they set, not town price × margin.** Players sell to a shop at what the shop would
  pay: its price less its margin.
- **Caravans and porters** price by cost plus carriage (adaptation F).
- **Retires:** `townPrice`, `buyingPrice`, `applyPrices`, `priceLift`, the orchestrator's price and margin, price
  support, `storePriced`.
- **Tests:** `oneTownPrice`, `tradeAndPrices` and `priceSupport` replaced by per-seller cost-price cases.

### Phase 5: businesses and owners

- **Working money:** a business keeps a month of its staffing (its hands and at most two posts) and its inputs.
- **The rest goes to its owner** on the business's own day of the month (the keeper's household, or its House).
- **The owner tops up** a business short of a week's working money, out of what it holds above its own needs. It
  stops after two months' working money unreturned in a quarter.
- **A week of turnover in trade credit**, repaid first from takings, written off after 8 weeks.
- **Rent** is a share of takings (15%) on a House's land, and it counts in the cost.
- **Retires:** the keeper's third of profit, the Houses' nightly sweep, props and forced sales, rescue loans, the flat
  20p rent.
- **Tests:** `greatHouses`, `theOwnersShare`, `housesKeepToTheirTrades`, `aHousesTillKeepsItsOrders` and
  `shopkeepersDontHoard` rewritten.

### Phase 6: the labour market

- **Each employer has its own wage and its posts** (adaptation B). They're reviewed weekly from its sales, with
  staples keeping 20% spare and a short shelf adding hands. A business short of materials hires only the hands its
  materials keep busy.
- **Wages:**
  - up when a post stands open most of the week (to 10% a week when short; at most 2× the going rate for the
    skill);
  - down slowly after calm weeks while hands stand idle;
  - never under 0.85× the going rate for the skill;
  - sliding back when more than 30% over it with no post open.
- **A post left is open again at once.**
- **Wolves judge a post by what it paid last week.** Under 60% of the going rate, they look daily. Under 40%, they
  leave. A new post starts a fresh record.
- **Poaching:** 4% a day of wolves move to a post paying 10% more, but never from an employer that is itself short,
  and nobody leaves a post it holds alone.
- **The harvest call:** sowing and harvest pay 1.25× the going rate and draw the idle and non-staple, non-master
  trades. The going rate leaves the farms' harvest pay out.
- **Artisans' pay floor** is 60% of a day when paid by the hours worked.
- **Carriers take on hands the day they turn loads away.**
- **Retires:** the town wage table, `dayWage` by kind, wage support, `HireRaise` and the weekly wage moves in `plan()`.
- **Tests:** `whoPaysWages`, `aLivingForEveryone`, `residentsTakeWork`, `workingOutOfTown` and `oddJobs` reworked.

### Phase 7: households and institutions

- **The household rule:**
  - needs first (food, fuel, clothes);
  - then the household's pooled money over (1 month's income + 10 days' needs) goes, at its a2 a month, half into a
    want fund and half to help about the home.
- **Wants** are owned pieces: a sword for a household that works, a rapier for the top sixth, finery to a piece a
  head. Building work is bought by the amount.
- **Servants** for a household holding over six months' income.
- **Daily relief** from the church, then the town, out of what each sets aside weekly (last week's relief and a third
  over).
- **Bakers' spoiling bread** goes to the hungry, then the poor, each evening, into the household's larder.
- **Tax 8% and tithe 6% of income, weekly.**
- **Institutions spend their month's income + a2 × (held − k × income):**
  - Houses (k 4, a2 0.3): retainers, finery, works, an estate crew;
  - the church (k 2, a2 0.4): relief, clergy, works, its own masons;
  - the town (k 2, a2 0.4): relief, the watch, its swords, works that hire whoever the budget pays.
- **Retires:**
  - wants (the tenth a day), savers' channels, feasts, the levy, the wealth tithe, the 2p dole;
  - the orchestrator's bands, pressure, grants, channels and funds, and the reserve rule;
  - `spendSurpluses`' hand-offs.
- **Tests:** `childrensStipends`, `grownWolvesBuyWhatTheyWant`, `householdsShareAndKeepHouse`, `theDole`,
  `theChurchCares`, `townPursesAndTheReckoning`, `noHoarding` and `theChannelsSpend` rewritten.

### Phase 8: lives, skills and masters

- **Lives** (adaptation G):
  - Gompertz deaths;
  - retirement at 58–66;
  - births following deaths into young families;
  - households splitting above six (into a free home);
  - kin inheriting money and goods;
  - the realm's number held.
- **Skill gates the trade:** a post needs its trade's skill; apprentices excepted.
- **Learning:** fast beside a better hand, up to the trade's level and a little; slowly alone; 4 a year for a shop's
  head with no better hand.
- **Apprentices:**
  - on half pay and bound to their master;
  - one per qualified hand (two while short; at most a third of a shop's hands);
  - only masters' trades (skill 50+) take the young from a post;
  - apprentices are hands over and above, so a master's trade keeps a post open for a master.
- **Masters' trades seek masters** (idle, or working below their level).
- **The user's succession rule:** the best apprentice steps up, else the best-trained wolf not in a master's place or
  an important position. They inherit the workshop (a House keeps its own) and work toward mastership. A masters'
  trade with no one at its level and no head appoints one weekly.
- **Re-founding** sets masters at about one wolf in fifty.
- **Tests:** `careers_tests` (apprentices, succession), `aging_tests` and `newcomer_tests` reworked; new succession
  cases.

### Phase 9: wear and mending for NPCs

- **NPCs own what they wear and carry:** a household's clothes (a set a head), swords, rapiers and finery, the
  watch's swords, the Houses' blades. All wear by days of use.
- **At 0.35 the owner sees to it:**
  - heirlooms are mended;
  - the rich may move up;
  - "fashion" sometimes buys new (clothes 15%, finery 35%);
  - otherwise mend if the maker's quote is under 60% of new.
- **The maker spends hours and its own materials** on a mend (blades 30% of the hours and 10% of the materials;
  clothes 50/30; finery 60/40). Mending counts toward its hands. Mending never shortens a piece's life (doc 35).
- **Founded pieces start at random condition.**
- **Retires:** household needs bought on a timer for clothes; NPC tool purchases out of the worker's purse.

### Phase 10: hunting and hides

- **The idle hunt** (the existing outwork, from the same finite game). Their catch feeds their household's larder, and
  hides go to the tanneries at the tannery's offer.
- **Sheep runs give hides too.** Leather goes into the blades and finery as the recipes have it.

### Phase 11: players

- **The mint** (`players.py`'s `Mint`): a weekly thermostat on money in use.
  - Players' and Holds' dormant money is what they hold beyond two months of their spending.
  - **It strikes** into the treasury only when that pulls money in use 5% under the founding *and* there's harm (wages
    down 3% in 8 weeks, 10% idle, 1% hungry).
  - **It melts** evenly from every purse (households under a week's food spared), only what it struck, when money in
    use is 5% over.
  - Logged as "the mint strikes" and "the mint melts" (`record(...)`, `minted`/`sunk`).
- **Holds as keeps** (adaptation H): workshops (up to three) and a farm with NPC hands paid by the Hold's players, and
  houses filled from the towns.
- **Players' goods:** stalls sell at the player's price, so a player can dump or corner. The market rebounds by the
  rules above (doc 63's crash tests).
- **Tests:** mint strike and melt cases; a Hold's payroll; a stall dump and recovery.

### Phase 12: the orchestrator steps back

This is done along the way, as each phase retires its part. What's left at the end:

- **Kept as a monitor:** the 23:00 snapshot and its measures (distress, hungry, short, poor, idle, Gini, the poorer
  half) and the DM's Money tab and brief viewer.
- **Removed:** `plan()`'s prices, margins, wages, bands, tax levels, channels, funds, grants, rescue and supports;
  `orchestrator.json`'s dials; `scenarios.json`; the steer routes. The DM's steering is Question 5.
- **Tests:** `orchestrator_tests` cut down to the monitor.

### Phase 13: the long runs

- **1-year, 5-year and 20-year `econ_watch` runs** on DEV's export, each compared with the model (Phase 0's
  `compare.py`). From the sixth pass on, doc 63's long runs found faults no short run showed.
- **A century run if the game's tick allows it.** Otherwise the 20-year run stands, with the model's century as the
  reference.
- **Players:** a 5-year run with the mint on and simulated hoarders, and one with Holds (staffed, drawing).

## Order

Phase 0 first: nothing can be judged without it.

Then:

1. **Phases 1 and 2:** the foundation, both small.
2. **Phases 3–6:** the core of the market, built in order. Prices need costs; costs need hours; owners need prices;
   wages need posts.
3. **Phase 7.**
4. **Phase 8:** it can begin alongside Phase 7, since lives touch careers, not money.
5. **Phases 9–11.**
6. **Phase 13's long runs**, after Phases 7, 8 and 11.

Each phase is committed and pushed when measured. The perf gate is checked at Phases 3, 6, 7 and 8.

## Done when (from the model)

| Measure | Band (years 3+) |
|---|---|
| Gini among households | 0.33–0.45 (20 years); no runaway over longer runs |
| The poorer half's share | 0.18–0.28 |
| Hungry | no day over 300 after year 1; days over 50 rare (a few a decade) |
| Bread / a bronze sword / a rapier | 0.05–0.09 / 1.5–4 / 15–40 LD |
| A labourer's day | steady within ±30% year to year; no drift beyond 1% a year |
| Money by kind of holder | each steady within about 20% of its year-3 share |
| Masters | never none for long; succession by apprentice or best hand |
| People and households | within 2% of the founding |
| Money | minted only at the founding (and by the mint, logged) |
| Every start | normal, one House holding all, empty treasury and equal purses converge (as doc 63) |

## Questions for the user

1. **The penny.** Count in farthings (a quarter penny; prices shown as pennies and farthings) so bread stays above one
   coin at 1,000,000p? Or found with about 4,000,000p and keep whole pennies?
   - *Recommended:* farthings.
2. **Re-founding.** Found DEV's economy afresh: accounts, tills, Houses, careers' money and the orchestrator's state.
   The world, its residents, homes and posts stay as they are.
   - *Recommended:* yes, at Phase 1. PROD when you choose.
3. **Posts.** A business may leave an authored post empty (laid off) and take hires beyond its posts, so its hands
   follow its sales. Should the empty post's holder keep its home and look for other work?
   - *Recommended:* yes.
4. **Splitting households.** A household above six splits only when there's a free home (an empty house, a room to
   let, a bunkhouse bed).
   - *Recommended:* yes, with the generator leaving a few homes free in each town.
5. **The DM's steering.** The orchestrator's pressure and scenarios go with it. Keep a few levers for the DM: the tax
   rate, the mint on or off, a one-off relief grant from the treasury?
   - *Recommended:* those three, logged.

## Decisions (the user, 2026-10-15)

1. **Whole pennies, and found with 5,000,000p.** A labourer's day comes to about 60p, bread about 3–4p, a bronze sword
   about 180p, a rapier about 1,500p. The penny stays the unit; there are no farthings. Adaptation A is settled this
   way.
2. **Reset DEV's economy** at Phase 1 (re-founding, rule 2).
3. **Posts: as recommended.** A business may leave an authored post empty and take hires beyond its posts. A laid-off
   holder keeps its home and looks for other work.
4. **Splitting households: yes.** The generator leaves a few homes free in each city and town.
5. **The DM keeps three levers:** the tax rate, the mint on or off, and a one-off relief grant from the treasury.
   Each is logged.

## Progress

### Phase 0: done (2026-10-15)

**econ_watch's new measures.** `days.jsonl` gains a `"model"` block each day:

- people, households (by home), and the households' Gini, poorer half's share and median;
- idle wolves (grown, under retiring age, no post, not apprenticed);
- masters (best skill ≥ 60), apprentices, elders (65+), and births and deaths since the day before;
- the going labourer's day wage in the capital (`dayWage(capital, "labour")`);
- prices in labourer's day-wages for wheat, flour, bread, firewood, timber, iron ore, iron bars, the three swords, the
  rapier, a wool scarf and a mining pick.

The measures cost nothing measurable per tick.

**`tools/econ_model/compare.py OUT [--every 7]`** runs the model for as many days and prints both, in measures that
don't depend on the money supply: shares by kind of holder, the Gini, the poorer half, prices in LD, people,
households, idle, hungry. It also lists money minted day by day.

**The "before"** (DEV build 34, 28 days, deterministic, `artifacts/economy/2026-10-15-before/days.jsonl` and
`-compare.txt`):

| Day 28 | Game | Model |
|---|---|---|
| Household Gini / poorer half | 0.48 / 0.17 (from 0.38 / 0.25 on day 7: widening) | 0.43 / 0.20 |
| Bread / wheat / bronze sword / rapier (LD) | 0.049 / 0.058 / 1.73 / 5.2 (falling) | 0.067 / 0.037 / 2.86 / 22 |
| Money: homes / businesses / Houses / church / town / other | 34% / 33% / 8% / 2% / 15% / 8% | 45% / 10% / 30% / 6% / 8% / 0 |
| Minted | days 1–2 (the founding, staged: +26,001p, +329,894p); none after | at the founding |

**Notes:**

- The game founds in stages over its first two days: starting purses, then the top-up to `WorldMoney`.
- Its businesses hold a third of all money, its Houses almost none: the reverse of the model.
- Its swords and rapiers sit at the catalogue's ladder (2.5 and 7.5 LD at the start), well under the rapier's Rare
  rung.
- **The game's skill scale isn't the model's** (700–900 "masters" at 60+), so masters aren't compared until Phase 8.
- **A run costs** about 9 minutes for 28 days at 0.067 ms a tick, so a year is about 2 hours, in the background.

### The user, 2026-10-15: the Houses and newcomers

- **The Houses aren't "founded".** They're old families of the realm; their money is part of the realm as it stands
  on its first day, like every household's purse and every shop's till. No coin is struck for them, then or later: a
  House the game sorts out later (`sortHouses`) already had what it holds, and the 3,000p for a newly sorted House goes.
  (So "the founding" in this document means only the realm's opening state, 5,000,000p, not an event in its story.)
- **Newcomers arrive with two days' wages** in their pockets. That is new money; the user accepts it, and leaves it
  to the mint. Since the mint only ever melts what it struck, **a newcomer's purse is counted as struck coin**: the
  mint has it out, and melts it back (evenly, from every purse) once the money in use runs over the realm's opening
  5,000,000p.

### Phase 1: built (2026-10-15); the DEV reset waits

**The realm's money as it stands** (`Society::worldMoney`, `RatwFounding.cpp`). Once the purses are made, what the
realm holds is made up to `WorldMoney` = 5,000,000p, each kind of holder to its share:

| Holder | Share | Spread |
|---|---|---|
| Households | 40% | Among grown wolves, unevenly: an exponential lot by each one's id, the same every run |
| Businesses | 15% | Tills, and keepers whose purse is their till, by their floats |
| The great houses | 30% | By the businesses each owns, plus one |
| The church | 5% | |
| The treasuries | 10% | By their residents |

The great houses' share is theirs from before the story starts: they're not founded (the user).

**After that, nothing is made:**

- A purse for something new (a town's buyers or church opened late) comes from its treasury (`startingMoney`).
- A house the rules newly count gets no coin (`sortHouses`).
- No deposits: the banks go. Savings already held are drawn back as before.
- The capital's daily carter of meals and herbs from nothing is gone.
- **The one exception: a newcomer** arrives with `NewcomerWageDays` (2) of its town's labourer's wage
  (`Game::newcomerPurse`). That's the user's choice, and the mint's to answer later. A child is born with nothing.

**`--refound-economy`** (`Options::refoundEconomy`, `Game::load`): the saved society and the roads' trade are set
aside on start, and the realm's money stands afresh from the world. Players' purses, which are in the society, start
afresh too. Tested in `game_tests` (`theEconomyRefounds`): without the option the saved economy loads; with it, the
money stands as at the start.

**Tests:** society 140, careers 64, newcomers 2,646, roads 515 (the bank case now expects no saving), orchestrator 126,
crafting 89,400, town 38,722, game 3,570: all pass.

**Measured** (28 days, `artifacts/economy/2026-10-15-phase1`):

- **From day 2, the opening matches the model:** Gini 0.48 / 0.48, poorer half 0.17 / 0.17, money 40/15/30/5/10%
  against the model's 40/15/31/5/10%. Nothing is minted after day 2.
- **Then the old rules pull it apart, as expected before Phases 4–7:**
  - the Houses' share drains (31% → 16%);
  - the church's grows (5% → 16%: the old wealth tithe on so many fuller purses);
  - the Gini widens to 0.51;
  - swords and rapiers sink in LD, while wages stay at the old 12p a day and prices at the old catalogue's.
- **The tick** is 0.082 ms (0.067 before), with more money moving. It's to be watched at the perf gate.

**The DEV reset waits.** With 10× the money but wages and prices at their old 12p level, DEV would play for weeks with
every household holding four months' wages. The reset (`--refound-economy` on DEV's server) is to be done once Phases
2, 4 and 6 build prices and wages at the 5,000,000p level (a labourer's day about 60p).

### Phase 2: done (2026-10-15)

**The ladder is in the catalogue.**

- **`Data/Economy/ladder.json`** holds the rungs and the calibration's settings: `catalogueDay` 12, `markup` 0.15,
  `skillWage` 150, `carryPerWeight` 0.004. Its `anchors` are the placings the ladder began from (the user's targets).
- **Every item in `Data/Items/items.json`** carries its own place, `worth` (`rung`, `at`), and **`playerSeconds`: 0**.
  That's how long a player takes to make it; it's not read by the game yet, and is set per item later in Atlas and the
  DM. An item's `price` is unchanged: doc 47's gear prices stand until Phase 4 prices from cost.
- **Every craft and producer in `crafts.json`** carries its NPC work, **`hours`** a batch or spell, calibrated from the
  ladder. It sits beside `seconds`, which still paces making until Phase 3. A bakery's batch of six loaves is 1.7
  hours of work, for example.

**`tools/econ_model/apply_ladder.py`** writes both, re-runnably.

- It changes only those fields, one object to a line, as it was. It's checked: no line changes beyond them, and lines
  written with `\u` escapes keep them.
- **The 110 conflicts:** a recipe whose inputs alone cost more than its goods were worth. Those goods' worth is raised
  to what they cost to make, and the ladder settled again until none are left. The biggest rises are silver goods,
  blankets, chain collars and crossbow prods. Calibration on the game's data now reports **0 conflicts of 361 ways**.
- **The labour budget:** a head's day of bread and household needs is 1.68 labourer's hours.

**`tools/item_catalog.py`** knows the ladder:

- every item needs a rung and an `at` from 0 to 1, and a whole `playerSeconds`;
- every craft and producer needs its `hours`;
- a recipe "loses" by worth, no longer by catalogue price;
- crafts.json's crafts and producers count as makers. Before, the checker failed on smoked mutton, smoked rabbit and
  farm gear for "nothing makes it", and on the brass bracelet's loss at catalogue prices.

A losing recipe is unsound only if it's the only way to make its goods. Four player recipes are dearer routes to goods
made cheaper another way (rye flour, a copper bar, plaster, an ear ring), and are listed as notes. The check passes.

**Tests:** crafting, society, roads and town suites pass with the new fields; the loaders ignore keys they don't know.

### Phase 3: built behind a switch (2026-10-15); calibrated hours wait for Phase 6

**Goods come from a pool of work hours.**

- Each business (its till) and each producer keeps **hours banked**, up to a day's 24.
- **Who adds to the pool:**
  - a keeper at the station adds the time since they last worked there, faster with skill and the shop's
    improvements;
  - a resident's spell for "the shop" or "the house" adds its hour;
  - an odd-job hand adds the spells they're paid for.
- **What the pool pays for:**
  - a batch comes out when the bank holds its hours;
  - a producer brings in a yield for each spell of hours banked, still under its `ProducerKept` cap.
- A keeper who isn't yet able to make a batch looks again when the hours should be there (between a minute and an hour
  of game time), not every quarter-hour.

**The switch.** `Society::setWorkByHours` picks what a batch or spell costs:

- **Off (the game's default):** the old pace, a craft's `seconds` as hours. A producer still brings in a yield a spell
  at most, as it did.
- **On:** the calibrated `hours` from Phase 2. `econ_watch --hours` turns it on.

**Why it's off.** Calibrated hours are the model's, and the model gets its hands from the labour market (Phase 6).
The game's shops are still kept by one keeper and a few residents. With the hours on, 9 days from DEV:

| | old pace | calibrated hours |
|---|---|---|
| starving on day 9 | 0 | 651 |
| maker-hours short | 6,178 | 7,646 |
| wages paid on day 9 | 9,917p | 4,720p |
| ms a tick | 0.064 | 0.133 (errands) |

At the old pace the run matches Phase 1's (maker-hours short ~6,000, nobody starving). Hours are turned on with
Phase 6, when businesses can hire.

**Left for later (Phase 3b):** output by sales targets in place of the stock caps, seasonal fields, wood laid in for
winter, and tool wear in a batch's cost. They need Phase 4's cost basis and Phase 6's day labour.

**Tests:** all suites pass. The crafting tests pin the old pace, a yield a spell.

### Phase 4: built (2026-10-15)

**Each seller prices from its own costs** (`Core/RatwPrices.cpp`).

- **The cost basis** (adaptation C): every seller's account (a till, a House's till, a caravan, a fund, a town's
  stores) keeps the average a piece of what each good cost it.
  - Bought: what it paid.
  - Made: the batch's hours at the town's labourer's wage (more for a skilled trade, ladder.json `skillWage`) and its
    materials at what they cost it, shared among the batch by worth.
  - Brought in: the producer's normal cost.
  - Moved unpaid (a caravan loaded from the stores, an estate): what it cost the giver.
  - It's kept in the society's memory once a day, so it's saved. An older save has none: sellers price from the
    ladder until they've bought or made.
- **A seller's price** is its normal cost × (1 + its margin), in whole pennies.
  - A good it makes: a batch's calibrated hours (Phase 2's, whatever the Phase 3 switch), its materials at what they
    cost it (or the going price, with no record) carted to the bench, and for a producer its tools' wear.
  - A good it buys and sells on: what it paid.
  - A quality its share of the plain good's, by worth.
- **The margin** starts at the ladder's 15% and is reviewed weekly, on each seller's own day, three weeks in four:
  - up by up to 4% when its shelf holds under 0.3 of the days of sales it keeps;
  - down when it holds over 1.7 of them and more than a maker keeps (`GoodsKept`). A maker's few on the shelf don't
    count as a glut until Phase 3b makes output follow sales;
  - otherwise a quarter of the way back toward 15%, at random;
  - within −30% and +60%, +100% for staples (the ladder's lowest rung).
  - Days kept: food by how long it keeps (half, 1 to 6 days), staples 6, else 10, 12 or 20 by worth.
- **The land is paid its own price.** Producers sell to the food shops, the granary and the trade orders at their
  price. A shop buys from a player or a wolf at its normal cost: its price less its margin.
- **Carriage by weight** (adaptation F): ladder.json's `carryPerWeight` labourer's hours a unit of weight a step, 10
  steps between towns.
  - Materials carted from another town cost the seller's price plus carriage, in place of half as much again.
  - A caravan sells at what its load cost it plus the carriage with its margin, in place of the road's 1.4×, and
    brings back the unsold at what it paid.
  - A standing order's price is the going price plus the carriage with its margin.
- **The going price** (`townPrice`) is now a good's worth at the town's labourer's wage: a yardstick, what a seller
  asks at the usual margin when its costs are the ladder's. Stalls, contracts, town projects and the measures use it.
  Contracts now pay it, not the catalogue's price.

**Retired:**

- the orchestrator's prices, its margin and its price support: it still works them out and shows them, but nothing
  applies them;
- `oneTownPrice` and `priceSupport` (roads tests). `sellersPriceFromCost` takes their place.

**Not yet:**

- meals, herbs and swords keep their old store pricing (`storePriced`). They're the oldest goods, and the next
  catalogue pass brings them onto the ladder;
- rent in the cost waits for Phase 5;
- `priceLift` stays in the orchestrator until Phase 12 strips it.

**Tests:** `sellersPriceFromCost` checks the going price, a baker's loaf from its hours and flour within 5% of its
worth, a fine loaf's share, the land paid its normal cost, and each bakery's own margin. The carted-in flour test is
re-pinned to the mill's price plus carriage.

**Measured** (30 days from DEV, prices in labourer's day-wages, sellers' mean asking price):

| | day 5 | day 30 | the model, day 30 |
|---|---|---|---|
| bread | 0.074 | 0.059 | 0.067 |
| wheat | 0.029 | 0.012 | 0.037 |
| iron bar | 1.00 | 0.54 | — |
| steel sword | 10.1 | 9.2 | — |
| rapier | 25.0 | 25.0 | 21.8 |
| margins (mean) | 0.153 | 0.165 | — |

- **Prices sit on the ladder.** In Phase 1's run a rapier had fallen to 3.8 LD by day 28; now it holds at 25.
- **Margins move but stay near 15%:** by day 30, 1,781 margins, none over 30%, under half a percent under 5%.
- **Prices lag wages.** The old orchestrator's wage table raises a labourer's day by a quarter a week (12p to 29p by
  day 30, as in Phase 1's run). Stock bought at the old prices sells at what it cost, so prices in LD fall while
  wages climb. Wheat falls furthest: at 12p a day it is worth 0.3p, and the mills paid a whole penny for it. Phase 6
  replaces the wage table.
- **As in Phase 1's run:** the Houses' share drains (31% to 13%), the church's grows (5% to 20%), and the household
  Gini widens (0.43 to 0.54). These are the old rules' (tithes, the Houses' sweeps and props), for Phases 5 and 7.
- Nobody starving; 2 hungry on day 30. 0.067 ms a tick.
- Tests: all suites pass. Two re-pinned: the contract reward (now the going price and the premium) and the carted-in
  flour.

### The orchestrator switched off (the user, 2026-10-15)

> "If orchestrator is getting in the way of implementing our working model, then perhaps we disable orchestrator and
> re-evaluate whether we need it later or not."

**`Data/Economy/orchestrator.json` `mode` is now `off`.** The code stays, and `RATW_ORCHESTRATOR=on|shadow` brings it
back. Off, these stop:

- its wage table. Wages hold at its starting table (a labourer's day 12p) until Phase 6's labour market;
- its channels and funds: the towns' odd jobs, commissions, granaries and trade orders;
- the bank's grants, the levy on savings, and the savers' rule.

**Measured** (30 days from DEV, the same run with it on and off):

| day 30 | on | off | the model |
|---|---|---|---|
| prices (LD) | falling behind wages (wheat 0.012, iron bar 0.54) | steady on the ladder (wheat 0.029, iron bar 1.03) | wheat 0.037 |
| household Gini | 0.54 | 0.53 | 0.43 |
| the Houses' share | 13% | 25% | 30% |
| the church's share | 20% | 20% | 6% |
| hungry | 2 | 0 | 0 |
| median purse | 986p | 1,087p | — |
| ms a tick | 0.067 | 0.072 | — |

Off is as good or better on every measure, and nobody goes hungry without the towns' odd jobs. The church still
gathers a fifth of the money by day 30: that's the old tithes, for Phase 7.

**Tests.** The game's tests now run with it off. Tests of what it does turn it on for their own world (`orchestrate`
in `roads_tests`: odd jobs, its channels, its watching in shadow; the projects test's town measure in shadow).
`orchestrator_tests` sets its own dials. Phase 12 decides what, if anything, it keeps.

### Phase 5: built (2026-10-15)

**A business keeps its working money, and the rest is its owner's** (`Core/RatwTills.cpp`, `tendOwners`).

- **Working money:** a month of the business's staffing and of the materials its sales use.
  - Staffing: its keeper, its help, and up to two of its help posts standing open, at the town's wages.
  - Materials: its daily sales of each good, at what one piece's materials cost it. Goods it sells on count at what
    they cost it, and a producer's tools at their wear.
  - Also a week of its standing orders on the roads.
- **The owner** is its House, or its keeper.
- **On the business's own day of the month**, all it holds above its working money (and its debts) goes to its owner
  as "the owner's profit".
- **On its own day of the week:**
  - a business on a House's land pays it **15% of the week's takings** as rent. That's the land's: a producer in a
    town with great houses, always the same House. Rent counts in its price, as a share of it;
  - a business short of a week's working money is **topped up by its owner** from what it can spare. A keeper keeps
    `KeeperReserve`; a House gives what it has. An owner stops after putting in two months' working money that hasn't
    come back: what's put in wears away by a thirteenth a week, and profits paid out count against it.
- **Trade credit:** a business short of money buys its materials on credit, up to a week of its turnover at cost.
  - Debts are repaid first each day, from whatever is in the till.
  - The seller writes a debt off after 56 days.
  - Debts and what owners have put in are saved in the society's memory.
- **The keeper's wage** comes out of the till as far as it can pay. It no longer waits for the till to hold half a
  float.
- **Market dues** are now a twentieth of what a till holds above its working money (it was above its float).

**Retired:** the keeper's third of the week's profit, the Houses' nightly sweep, a House propping up its own trade's
businesses, the forced sale of a failing business, and the flat 20p monthly rent. Rescue loans went with the
orchestrator.

**Measured** (30 days from DEV, the orchestrator off):

- **Money moved** over the month:
  - farms paid their farmers 128,000p in profit;
  - farms paid the Houses 17,100p in rent;
  - House businesses paid their Houses 11,200p in profit;
  - shopkeepers took 10,200p in profit and put 35,800p in.
- **Shops are kept going by their owners**, because of what the old institutions take:
  - in the first run (before the market dues change), shop tills paid 103,000p in market dues, 82,000p in wealth
    tithe and 70,000p in weekly tax and tithe;
  - **the wealth tithe** takes about 700,000p from everyone (14% of all money) to the church in 30 days. That's why the
    church's share climbs from 5% to 20%, and households' falls from 41% to 28%. **It is the largest drain left, and
    Phase 7 replaces it** with the model's tithe of 6% of income.
- **Against Phase 4 with the orchestrator off** (day 30), every share is within a point:
  - households 28%, businesses 17%, the Houses 24%, the church 20%, the towns 11%;
  - household Gini 0.53;
  - nobody hungry; 0.068 ms a tick.

**Tests:**

- `theOwnersShare` became `theOwnersProfit`: the month's profit to the keeper; a dry till topped up.
- `greatHouses`' second half is rewritten: the manager's wage, the month's profit to the House, its top-up, and no
  rent on its own business.
- New: `rentOnHouseLand`, a farm paying 15% of its takings.
- `residentsFillContractsForGoods` checks the till's contract payment, since the keeper's purse now tops up its till.

### Phase 6: built (2026-10-15); calibrated hours still wait, on the realm's diet

**Every employer pays its own wage** (`Core/RatwLabour.cpp`).

- **Who's an employer:**
  - a business, by its till;
  - the town's watch, works and other posts, and the church (`"<account>|<kind>"`).
- **Its book** (saved in the society's memory) holds:
  - its day's wage;
  - the days this week a post of its stood open;
  - its calm weeks in a row;
  - whether it fell short paying.
- **A new employer** starts at the going rate for its skill:
  - **the going rate** is its town's labourer's wage: the middle of what its unskilled businesses pay, counted by
    their hands;
  - times 1 + skill / `skillWage` (the most any of its crafts needs);
  - the Town Works at 0.85 of it, the clergy 1.1.
- **The going rate is now `labourDay`.** Prices follow it, and `dayWage` by kind becomes a share of it. The
  orchestrator's wage table applies only when it's on.
- **Weekly, on its own day**, an employer's wage:
  - **rises** if a post stood open four days or more: 3% to 10%, the more of its posts open the more, never past
    twice the going rate for its skill;
  - **eases a little** after six calm weeks while more than 3% of grown wolves are idle;
  - **slides back** 5% a week while over 1.3× the going rate with nothing open;
  - **falls a little** after a week it fell short paying;
  - **never drops** under 0.85× the going rate.
- **Everyone is paid their employer's wage:** help by the spell, the keeper and the manager by the day.
- **Wolves judge a post by what it paid of what it owed them**, a running week of working days:
  - under 60%, a wolf looks each morning for a hire paying better than it's been getting;
  - under 40%, it leaves, unless it's the only hand (the watch keeps its posts);
  - a new post starts a fresh record.
  - Not by pay itself: a messenger or a House's kinsman works fewer spells at its post, and isn't badly paid for it.
    The first build judged by pay, and 90 wolves left good posts in 30 days.
- **Poaching:** a wolf paid well enough takes a hire paying a tenth over its post's wage on 4% of mornings. Never from
  an employer short of hands, nor as its only hand.
- **A post left is open again at once**, and succession fills it as before.

**Hands by sales.** Only with work by the hours (Phase 3's switch): at the seconds pace a business's sales take almost
no hours, and it would let everyone go. At its weekly review, a business works out its hands wanted:

- **The hours its sales take a day:**
  - its crafts' or yields' hours for what it sells;
  - a fifth more for staples (`StapleSlack`);
  - half again when its shelf is nearly bare, a fifth less when it's glutted;
  - in hands of 8 hours, its keeper at least.
- **Short of hands,** it opens the posts it let go, then hires for a week at its own wage, from the idle and from other
  trades. A farm pays a quarter over at sowing and harvest (the harvest call).
- **Over,** it lets its hires run out, then lets help go: half the excess, one at least. Their posts are kept empty
  (`memory.closed`, which succession skips).
- **Never more than doubling** its hands (and two) in a week.
- A hire's hand now works a full day's 8 spells, not 4. A day's pay for half a day's work made hired hands cost twice
  their hours.
- With the hours on, farms hire through this review, and the old farm hires stand down.

**Measured, the game's default (the seconds pace), 30 days from DEV:**

- the going rate holds at 12p;
- 267 employers have books; their wages average 1.06× the going rate, and 7% are over 1.3×;
- 2 wolves left a post that didn't pay;
- nobody idle, hungry or starving; shares of money as in Phase 5; 0.063 ms a tick.

**With the calibrated hours on, the realm still starves** (642 starving by day 9, as in Phase 3). The labour market
isn't why: the realm's diet takes more work than its hands have.

| | hours of work a day |
|---|---|
| food alone, at calibrated hours (the wolves' actual diet, days 8–14) | about 3,750 |
| everything a household needs, in doc 63's calibration (1.68 h a head) | about 1,820 |
| the realm's producers (235) and businesses (148) at 8 hours | about 3,060 |

**Why the diet costs so much:**

- A wolf eats about 47 nourishment a day. The model fed each head two loaves (30).
- Most of it comes from the costlier foods:
  - smoked fish, smoked mutton, cheese and jerky take 4 to 7 hours of work per meal's worth (50 nourishment);
  - bread, porridge and rabbit take about 1.4.
- Those foods took their ladder places from their catalogue prices, so they are dear for what they feed.

**This is for the user to decide.** Calibrated hours wait for it.

**Retired:** wage support (the orchestrator's) and `HireRaise` (a hire must now pay a tenth more). `dayWage` by kind
is now a share of the going rate. The orchestrator's table and weekly moves apply only when it's on.

**Tests:**

- `greatHouses`: the manager is paid its business's wage.
- `householdsShareAndKeepHouse`: a working morning, not a Restday.
- `theDole`: a whole household penniless, since the dole counts a household's purse.

### The realm's diet, and the pace (the user, 2026-10-15: "go with your recommendation")

**Foods placed by how much they feed** (`tools/econ_model/food_worth.py`):

- a food's worth is its nourishment at bread's rate a point (4 mLD), times a premium for its kind:
  - staple 1.0: bread, porridge, rabbit, offal;
  - fresh 1.2 to 1.3: fish, meat, mutton, eggs, milk;
  - preserved 1.6: smoked, salted, cheese, butter, sausage;
  - cooked 1.6: a meal, a pie, a stew, a roast;
  - fine 3.0: spiced roasts, pâtés, honey cakes;
- drinks keep their places;
- then `apply_ladder.py` recalibrates every craft's and producer's hours. It raised smoked fish to 0.17 LD and a meal
  to 0.43 LD, to cover their materials. 0 conflicts of 361.
- **The wolves' actual diet now takes about 2,700 hours a day, down from 3,750.**

**Thrift:** wolves short of a week's food money already buy by nourishment a penny. Now only the well off (a month's
food money) follow their taste as far as it goes. The merely comfortable follow it half as far.

**Calibrated hours still starved the realm, for want of time, not hands:**

- the realm credits about 7,000 work hours a day, enough;
- but they're in the authored trades, not on the land. In the first runs 1,500 to 2,500 hours a day were lost to full
  banks (shelves at their caps, materials short);
- the founding's food spoils on day 5, and hungry wolves don't work, so production fell into a spiral.

**Three fixes:**

- **Farms review their hands too.** A producer's post had no employer key, so farms never hired.
- **Hiring for need, every day:**
  - a business short of hands hires the day it finds itself short; it lets hands go only at its weekly review;
  - hires aren't capped at 3 hands;
  - a hand whose business has more work banked than it can use may take a hire elsewhere (the harvest call).
- **The realm's pace** (`memory.pace`, saved): with work by the hours, each craft's and yield's hours go from the old
  pace toward the calibrated:
  - a 56th of the way a day while fewer than 2% of the realm are hungry;
  - back a 14th a day while more are;
  - businesses don't lay hands off until the pace reaches 1.

**Measured, 60 days with the hours on:**

| day | 6 | 12 | 18 | 24 | 30 | 36 | 44 | 52 | 60 |
|---|---|---|---|---|---|---|---|---|---|
| pace | 0.09 | 0.20 | 0.30 | 0.41 | 0.43 | 0.27 | 0.43 | 0.39 | 0.54 |

- Two hungry spells (44 wolves on day 32, 15 on day 52); each ended within days.
- Prices on the ladder throughout; the going wage at 12p.
- **Where it settles is what the realm's hands can carry.** By day 60 that's still rising past half. A longer run will
  tell (Phase 13).
- The game's default stays the old pace (the switch off) until then.

### Phase 7: built (2026-10-15)

**Income** (`Core/RatwInstitutions.cpp`): what an account earns, tallied as it's paid.

- **A resident:** everything but gifts, estates, relief, the household purse and an owner's investment.
- **A House:** its profit and rent.
- **A town:** its tax and the capital's share.
- **The church:** its tithes and the plate.
- **Kept in the society's memory:** the week's income (taxed) and a month's running income (a 28th off a day).

**Tax 8% and tithe 6% of each resident's and House's week's income**, at the reckoning, as far as its purse goes:

- a business isn't taxed: its profit is its owner's, and taxed as theirs;
- an inheritance or a gift isn't income;
- **gone:** the profit tax with its band, the poverty line's exemption, the wealth tithe and market dues;
- the capital's tenth of each town's tax stays.

**Institutions spend a weekly budget**: a quarter of a month's income plus a2 × (held − k × that income). In the
realm's first month, before there's a month's income, they reckon a k+1-th of what they hold.

- **A town** (k 2, a2 0.4):
  - first its week's relief, set aside: last week's and a third over, plus a twentieth of the budget, at most 30%;
  - then its own wages (the watch, the Town Works, its posts);
  - then 80% of the rest on a works crew hired for the week, the rest on building materials.
- **The church** (k 2, a2 0.4):
  - its relief (at least a tenth of the budget);
  - its clergy's wages;
  - half the rest on materials for its works, the rest on its own masons for the week.
- **A House** (k 4, a2 0.3):
  - retainers for the week (30%, up to 12);
  - finery (30%, kept, up to 12 pieces);
  - works (40% of what's left);
  - an estate crew with the rest.

**Daily relief** replaces the dole:

- a household short of a day's food money, or hungry with no food in, is given what it lacks;
- by the church, then its town, out of what each set aside (before its first reckoning, the share its first week's
  budget would give);
- relief isn't income.

**The household rule** (daily, in `tendHouseholds`):

- what the household's purse holds over a month's income and 10 days' needs goes, at its own rate (0.3 to 0.7 a
  month), half into a want fund and half to help about the home;
- **wants** come out of the want fund now, each grown member its share. The tenth-a-day rule is gone;
- **help about the home** is a hand for the week at 0.8 of the going rate, once the help budget runs to a week of it;
- **servants:** a household holding over six months of its income takes servants for the week, as many as a tenth of
  the excess pays for a month (up to 3).

**Bakers' leftovers:** each evening, bread that wouldn't keep another day goes from a baker's shelves to the larders
of the hungry, then the poor (under three days' food money). It isn't sold.

**Hires and keepers:** a wolf keeping the house doesn't take hires (it counted as jobless).

**Not yet:** owned pieces (a sword for a working household, a rapier for the rich) wait for Phase 9's mending.
Works on the home come out of the want fund only as wants for now. The savers' rule and the levy stay inert with the
orchestrator.

**Measured** (30 days from DEV, the default pace):

| day 30 | Phase 6 | Phase 7 | the model |
|---|---|---|---|
| the church's share | 20% | 5% | 6% |
| the Houses' | 25% | 30% | 30% |
| the towns' | 11% | 9% | 8% |
| households' | 28% | 35% | 46% |
| businesses' | 16% | 20% | 10% |
| household Gini | 0.53 | 0.52 | 0.43 |
| median purse | 1,076p | 1,310p | — |
| hungry | 0 | 0 | 0 |
| ms a tick | 0.063 | 0.081 | — |

- The wealth tithe's drain is gone: the church, the Houses and the towns hold the model's shares.
- **Money now moves:**
  - wants about 36,000p a month;
  - tax about 33,000p and tithes about 21,000p;
  - towns' works crews about 9,000p;
  - help about the home about 13,000p.
- **Left:**
  - businesses hold twice the model's share: working money and the month's profit not yet paid out;
  - households' share is short by as much;
  - the Gini is 0.1 wider than the model's.
  These are for the long runs (Phase 13), once lives (Phase 8) and mending (Phase 9) are in.
- **The tick** is 28% dearer, in errands: more wolves walking to hires and help. Help about the home is hired by the
  week (it was by the day: 0.039 ms of errands, now 0.034).

**Tests:**

- `townPursesAndTheReckoning`: 8% and 6% of income, nothing on an inheritance, nothing from an empty purse, no wealth
  tithe or market dues.
- `theDole` became `theRelief`.
- `shopkeepersDontHoard`: a till pays no dues or tax of its own.
- `grownWolvesBuyWhatTheyWant`: a windfall spent from the want fund over a week.
- A test's own transfers ("test: …") aren't income.
- All suites pass.

### Phase 8: built (2026-10-15)

**Lives:**

- **Deaths by age** (`World::tendOldAge`): a yearly chance of 0.0004 × e^(0.085 × age), and 3% more under five. That's
  about 1% at forty, 7% at sixty, 15% at seventy, a third at eighty. It replaces the bands from seventy.
  Protected residents still don't die of it.
- **Retirement:** each wolf has its own age, 58 to 66 (a lot by its id).
  - In paid work (a trade, the watch), it leaves its post then **once someone can take it on**: its apprentice, family
    or a local out of work, as skilled as the trade needs. With nobody, it works on.
  - The model retires everyone and fills posts from its idle. The authored realm has nobody idle and 76 wolves past
    their age in work: retiring them all at once left about 70 posts empty and the realm hungry.
  - The clergy, a great House's head and the unpaid (a storyteller, a beggar) keep theirs: the realm's elders stay as
    they were written. Greyfen's 72-year-old Maple still preaches.
  - Out of work past its age, a wolf is "retired".
- **Births and newcomers in a world from files:** `World::makeRequestedResidents` makes them from their templates, as
  the database host does. A child has nothing; a newcomer has two days' wages.
  - The game does this when it has no database, and `econ_watch` every game minute.
  - Before, such a world logged the requests and dropped them, so its population could only fall.
- **The realm's number holds:**
  - births still come while a town is under its founding number (married couples, about a month apart): deaths
    bring them;
  - a stranger is now sent for a post only while its town is under its number too. Otherwise its posts fill from its
    own. Before, every post empty eight days brought a stranger: 60 newcomers in a fortnight once retirements and
    apprenticeships emptied posts;
  - a hand lodged at a farm's bunkhouse counts at its own home. City wolves hired out to the farms made the city look
    short, and births were asked for.
- **Kin inherit everything:** an estate's goods now pass with its money (it was only herbs and meals).
- **Households split into free homes:**
  - weekly, a household of more than six sends an unmarried grown member of 18 to 32 to a free home in its town (one
    written for someone, now empty);
  - with none free it stays together, as the user decided;
  - the generator's few spare homes come with the DEV rebuild.

**Skills:**

- **A post's trade has a level** (`Position::level`): the skill its business's hardest craft needs (recipes.json). Help
  and other posts have none.
- **Founders** are seeded near their trade's level:
  - a skilled trade's hands at level − 8 + (age − 25) × 0.4 + up to 12, so about half are at the level;
  - a plain post's 5 to 45 by age.
  
  This is for a fresh founding: a saved world keeps its skills.
- **Pace and quality are relative to the level:**
  - a hand at its trade's level works at the usual pace and crafts as a 50 did;
  - a better one works up to a quarter quicker, and makes fine work more often.
  
  Before, both went by skill alone. With plain trades' founders at 5 to 45, an inn's cooks made crude stew, a third
  as many meals came out, and Ridgemere went hungry on days 23 to 25.
- **Learning, a year of work:**
  - 8 beside a better hand: an apprentice beside its master, up to five short of the master, or the level where the
    master is at it;
  - 4 for a shop's or farm's head short of its trade's level;
  - 0.3 otherwise;
  - never past 85.
  
  The old rate was about 10 points a working day, so skills topped out within weeks.

**Apprentices** (every trade has them: the user, 2026-09-27):

- **Who may take one:**
  - a skilled trade's master must be at its level;
  - a plain trade's, 35 or older or at skill 30.
- **Pay and learning:**
  - half the business's wage, from its till;
  - they learn until they reach the trade's level (30 for a plain trade).
- **Who is taken:**
  - family and household first, then one out of work, or working alongside;
  - a masters' trade (level 50 and up) also takes the young from plain work, as in the model, but only while more than
    3% of grown wolves are idle. The model always has some idle; the authored realm has none, so the post left behind
    stayed empty and the realm went short (15 to 24 posts in the runs).

**Succession** (the user's rule):

1. the apprentice, after two days;
2. for a masters' trade, after a day, a master about (idle, or working below its level);
3. family after three days, a local out of work after five, if as skilled as the trade needs (their best at that kind
   of work);
4. otherwise, after five days, the best-trained wolf about who isn't in a master's place, keeping its own shop or the
   clergy. It heads the business and learns toward mastership at the head's rate.

A newcomer comes at least as skilled as the trade needs.

**Tests:**

- New, `theBestTrainedInherits`:
  - with no apprentice and no master about, the forge goes to the best-trained after five days;
  - a master smith baking bread is sought out within days.
- Two re-pinned:
  - the farm's yield counted as brought in (the quicker mill may buy it at once);
  - a lent paw makes batches sooner, not twice as many (a skilled baker's batches come as fast as it looks).
- One successor per retirement: one wolf free to take a post had let 36 retire at once on day 31, when a new year aged
  the realm and its children came of age.

**Measured** (60 days from DEV, the default pace; Phase 7 built from its commit and run the same, for the comparison):

| | Phase 7, day 60 | Phase 8, day 30 | Phase 8, day 60 | the model, day 60 |
|---|---|---|---|---|
| people | 1,083 | 1,082 | 1,083 | 890 |
| births and newcomers / deaths (60 days) | — | — | 5 / 5 | — |
| posts empty | — | 4 | 15 | — |
| apprentices | 85 | 77 | 84 | — |
| masters (at a masters' trade's level) | — | 15 | 15 | 1–5 |
| household Gini | 0.62 | 0.51 | 0.58 | 0.39 |
| households' share | 33% | 37% | 36% | 52% |
| the church's, the Houses', the towns' | 5%, 30%, 8% | 5%, 30%, 10% | 5%, 30%, 9% | 6%, 25%, 7% |
| ms a tick | 0.094 | — | 0.111 | — |

- **The realm's number holds:** five births and newcomers for five deaths.
- **Masters stay about one wolf in seventy** (15).
- **The Gini widens more slowly than Phase 7's** (0.58 against 0.62 at day 60).
- **Hunger:**
  - Phase 7 at 60 days: a single hungry day (49 wolves, day 46);
  - Phase 8: hungry spells from day 30 (83 on day 31; 141 at worst, day 45), each over in a day or three;
  - the hungry are mostly Ridgemere's. In both phases the capital's shops run empty for days at a time after day 36
    (Phase 7's shop food went 15,800 → 800 → 10,900 → 0 nourishment between days 36 and 42).
  
  **The capital's food supply is fragile from the second month, before Phase 8.** Production is still bounded by
  stock caps (`GoodsKept`, `ProducerKept`), not by sales; Phase 3b replaces them. Phase 8's retirements and changes of
  hands deepen the dips. That's the next thing to fix.
- **The tick:** 0.111 ms, up from Phase 7's 0.094, in errands (0.064 against 0.046): more wolves about their days as
  apprentices and in retirement.


### Phase 3b, first part: output follows sales (2026-10-21)

**What was wrong.** A 60-day run of Phase 8 from DEV (build 34, deterministic) had 14 days with wolves starving (96 at
worst, day 39), 503 starving-days in all. Most were in Oakhaven and Frosthollow, not the capital:

- their shops had cash (1,000–4,000p) and nothing to sell: Oakhaven's baker had no flour from day 8, its inn no raw meat
  or milk, Frosthollow's provisioner nothing to cure. Neither town has a mill, a butcher or a dairy; it all comes by road;
- the roads' standing orders were filled a third to a half (Ridgemere's, week 3: 12,083 of 30,526 goods), because the
  selling towns had none to spare: **a shop made only up to `GoodsKept` (4) of a good, a producer brought in only up to
  `ProducerKept` (40), a food shop took only `FoodShelf` (30) from the land**, however much it sold or was ordered;
- an order raised itself only from the buyer's use, and a baker with no flour sells no bread, so Oakhaven's baker's
  order stayed at 6 flour a week (36 loaves for 72 wolves);
- the trader looked an order over after handing that week's goods over, when the shop looked well stocked, so it was
  never raised.

**Built:**

- **`Society::shelfTarget(till, good, least, orders)`:** what a seller keeps is its sales a day (the running average,
  missed sales counting) times the good's `keepDays`, a fifth over for a staple (`StapleSlack`); with orders, at least
  its share of a week of the roads' orders on the town and half a supplier's store. The old fixed stores are floors.
  At most `ShelfMost` (2,000).
- **Missed sales from selling out:** a seller that sold its last of a good counts, at the day's end, the buyers after as
  half again its day's sales (`ranOut_`), so a shelf that empties every day keeps more until it doesn't.
- **Used everywhere the old stores bound output:** a maker's goods and the materials for the batches that fill them
  (`Society::craft`), a supplier's restocking, a producer's yield (`produce`), producers selling to food shops
  (`producersSell`), the hands players and odd jobs lend a farm, and what a town has to spare and wants on the roads
  (`marketOf`).
- **The roads' orders are shared among a town's producers too**, not only its makers (`setStandingOrders`): a farm keeps
  its share of the grain ordered from its town.
- **The trader reviews an order before handing the week's goods over** (`tradeCaravanArrived`).
- **econ_watch:** `RATW_STOCK=account,...` writes those accounts' cash and stock every three game hours to `stock.csv`;
  `orders.csv` has every standing order at each day's end (a week's ask, delivered and short since agreed).
- **Tests:** `crafting_tests` `outputFollowsSales`; the roads' `makersKeepWhatIsOrdered` holds as it was.

**Measured** (60 days from DEV, the default pace; `artifacts/economy/2026-10-21-phase8` and `-phase3b`):

| | Phase 8 | Phase 3b |
|---|---|---|
| days with wolves starving | 14 | 0 |
| starving-days, the worst day | 503, 96 | 0, 0 |
| hungry, the worst day | 141 | 21 |
| small towns' households under a day's food (days 30–60) | 91% | 62% |
| Ridgemere's shop food (days 5–60): least, median | 0, 564 | 1,098, 2,508 |
| household Gini, day 60 | 0.58 | 0.63 |
| the poorer half, day 60 | 0.11 | 0.09 |
| ms a tick | 0.114 | 0.070 |

- **No starving at all,** and the tick is cheaper: fewer wolves walking from shop to empty shop.
- **The small towns are still lean.** Orders now ask for what the towns use (36,500 goods a week on day 58, from
  10,300 on day 9) and are filled about half (flour and oats from Larkfield, rabbit and apples from Southmeadow). That's
  the land's hands, not a cap: at the default pace a farm worker brings in about 8 oats a day. Phase 6's hiring by
  sales works only with calibrated hours on, so the old pace has no way to add hands to the land.
- **The Gini widens faster** (0.63 against 0.58 at day 60; the model's 0.39). More goes through the land: the producers'
  own purses rise from 14% to 16% of the money and paid workers' fall from 13% to 10%. The widening was there before
  (Phase 8), and is the next research.

**Left of Phase 3b:** seasonal fields (sowing and harvest by hours, a year's store), wood laid in for winter, and tool
wear in a batch's cost (tools still bought from the worker's own purse, `tradeUpkeep`).

### The households' gap (2026-10-25)

**The question.** Phase 3b's runs had the household Gini widening, 0.58 to 0.63 by day 60 (the model's 0.39).

**Half of it was the measure.** The 51 bunkhouses and barracks each counted as one household, and between them they held
about half of all household money. econ_watch now counts each grown wolf of a shared home (over 8 grown, or
`communalHome`) as a household of its own, as the game's household rule already treats them. Corrected, the default
60-day run goes 0.52 to 0.48.

**Three leaks, fixed:**

- **Shared homes skipped the household rule.** About 235 wolves (farmers, loggers, shepherds) live in bunkhouses and
  barracks, and their purses could only fill. Each now runs the rule as its own household (`householdRule(..., lodger)`):
  all its spare to its own want fund, no help about the home or servants for a bunk. A `roads_tests` check fails without
  it.
- **A market stall reckoned the whole market's wages into its working money** (`workingMoney`): every post at its work
  cell, a month of dozens of wages for a stall selling a score a day. It counts only the help it employs now
  (`postKey_`). That was 684k of the 1.75M businesses held back.
- **A batch's materials were charged against each of its goods whole** (`materialsCost`): a butcher charged a whole
  carcass against every cut. They're shared by worth now, as `normalCost` does.

Together these cut about 1.1M of the 1.75M businesses reserved.

**econ_watch:** `residents.csv` (each resident's home, kind of holder, job, cash and savings, daily) and `tills.csv`
(each till's cash, working money, owed, reckoned sales rate, materials a day, orders a week, units sold and takings).

**What's left is the pace, not spending.** 120 days from DEV (deterministic):

| day 119 | the fixes | + commissions, more hands (12p) | the same at 60p |
|---|---|---|---|
| household Gini (from about 0.53) | 0.67 | 0.671 | 0.737 |
| the poorer half | | 0.074 | 0.024 |
| hungry, starving | 0, 0 | 0, 0 | 12, 5 |
| producers' purses (from about 0.5M) | | 0.60M | 1.16M |
| wage earners' purses (from about 0.95M) | | 0.32M | 0.35M |

- **Rich households spending more doesn't close it.** Tried: well-off wolves commissioning pieces made to order, and
  help and servants scaling with the budget (to 4 and 8). Commissions came to about 12,000p in 120 days; the Gini didn't
  move. Not kept.
- **The drain is from wage earners.** Days 90–120 they took in 228k and spent 304k. Shop tills went 421k to 673k,
  producers 516k to 603k, and the going wage stayed at 12p (12.7 by day 119).
- **A 60p labourer's day** (the wage 5,000,000p supports, doc 64's founding) made it worse: the producers doubled and
  the realm went hungry from day 60.
- **The cause:** goods are priced at the calibrated hours but made at the old pace (`memory.pace` 0, the switch off),
  so the difference is the producers' profit. At 12p the penny floor hides part of it (44% of everything sold goes at
  1p); at 60p it's plain. Pricing at the hours actually worked (tried, reverted) didn't help at 12p, for the penny floor.
- **The fix is work by the hours**, so goods cost the hours they're priced at. That waits on the land feeding the realm
  at calibrated hours: the rest of Phase 3b.

### Building first, balancing last (the user, 2026-10-25)

"We probably shouldn't try balancing everything until we are done implementing everything." The rest of the phases
are built as designed, each with its tests and a short smoke run for breakage (crashes, money not conserved). What the
smoke runs show about balance is noted here for Phase 13, not tuned.

### Phase 3b, the rest: seasonal fields, wood for winter, tool wear (2026-10-25)

With work by the hours on (`Society::fieldsOn`, the Phase 3 switch). The game's default, the old pace, is as it was.

**Seasonal fields** (`RatwFarmhands.cpp`, `Society::produce`): farms, orchards and vineyards (`fieldWork`).

- **The year:** sowing to day 92 of the year (spring), the harvest from day 213 to 274 (the end of summer and early
  autumn), as the model's 0–90 and 196–252 of its 336-day year.
- **An acre** is `FieldYields` (20) of the producer's yields. A third of its hours go at sowing, the rest at the
  harvest (`SowShare`). Between, the fields bring nothing in; an orchard's or a farm's work out of season brings in its
  `offSeason` goods that aren't its fields' (prunings for firewood). The store is sold from all year. Field goods don't
  spoil in the catalogue, so the store keeps.
- **Planning** (`planField`): at the harvest's end, next year's acres are a year and a harvest of its sales a day
  (`fieldNeed`: the most of any of its goods, missed sales counting), a tenth over (`FieldMargin`), half again after a
  year its store ran dry (`DryYear`), less what its store will still hold at the next harvest. What wasn't brought in
  is lost.
- **Founding:** a farm first seen with sales to plan by has this year's fields as if sown, and a store made to last it
  to the harvest ("a year's store").
- **Hands:** a field wants the work left in its season over the days left of it (`handsWanted`); between, one hand.
  Its hires at sowing and the harvest are field hires (`OddJob::field`), at the harvest's pay.
- **The harvest call** (`calledToTheLand`): at sowing and the harvest, each morning a tenth (`LandCall`) of the hands of
  trades that aren't staples (none of their crafts' goods on the staple rung), and the town's own posts, may take a
  field's hire whatever it pays. Not the land's own, the clergy, guards or keepers.
- **Saved:** `memory.fields` (acres, sown, to harvest, part-yield, the year last planned, dry).

**Wood laid in for winter** (`shelfTarget`): from a month before winter (day 247), what households burn faster in
winter (crafts.json `winterDays`: firewood and peat, twice as fast) is kept at its winter rate. All paces.

**Tool wear** (`Society::wearTools`, `tradeUpkeep`): a producer's tools wear by its hours of work, a tool lasting its
`everyDays` of 8-hour days, and a new one is bought from the till each time one is worn through (it was bought on a
timer). Their cost in a yield is reckoned at the calibrated hours, as the yield's labour is (it was the old pace's).

**Tests:** `crafting_tests` `seasonalFields` (founding, nothing between, the harvest, planning at the end, sowing next
spring; the smith's hand called to the land and the baker's never, and not in summer) and `woodLaidInForWinter`;
`roads_tests`' pick bought by the mine after a pick's worth of work.

**Smoke run** (30 days from DEV with the hours on, deterministic): money conserved, no faults; 0.064 ms a tick. One
hungry spell on day 25 (205 hungry, 105 starving), as the hours runs had before (Phase 13).

### Phase 9: wear and mending for NPCs (2026-10-25)

With work by the hours on (`RatwBelongings.cpp`, `tendBelongings` in the day's pass).

- **What a grown wolf owns** (`memory.belongings`, saved): its clothes (the household need bought a head, `perPerson`:
  a wool scarf), the finery and blades in its purse (`belongingKind`: jewellery, hats, scarves, shawls, paw wear;
  swords and rapiers), and a guard the sword it's issued (not in its purse). Pieces follow its purse: bought ones come
  in whole, sold or given ones go.
- **Wear by days of use** (the model's): clothes 150 days whole to worn out, finery 240, a sword 360, a rapier 540, a
  guard's issued sword 120. Founded pieces start between 0.35 and whole.
- **Worn to 0.35 (`MendAt`), seen to**, one piece a day:
  - an heirloom (swords 20%, rapiers 40%, finery 5%, clothes never) is mended;
  - the rich (three months of its income to spare) move up a third of the time: a sword for a rapier;
  - "fashion" buys new: clothes 15%, finery 35%, blades 2%;
  - otherwise mended if the cheapest maker's quote is under 60% of new (`MendIf`), else bought new.
- **Who pays:** an issued sword, the guard's payer (the watch, a House); otherwise the wolf, beyond eight days' food
  money, finery and blades from its want fund.
- **A mend:** the maker's quote is the wear times the kind's share of the piece's hours (blades 30%, clothes 50%,
  finery 60%) at its town's hour, and of its materials (10%, 30%, 40%), with the ladder's markup. The maker uses its
  own materials (whole ones, a part by lot), and the hours are booked into its work in hand, worked out of what it
  banks next, while its mending booked is under half a day of its hands' time. Mending counts toward its hands
  (`mendRate_`, a month's running average, in `handsWanted`); a maker too busy or short hears it was asked. A mended
  piece is whole again (doc 35).
- **Retired with the hours on:** households buying clothes on a timer.
- **Not yet:** the Houses' own blades (their retainers who are guards are covered).
- **Tests:** `crafting_tests` `piecesWornAndMended` (the scribe owns its sword and clothes; an heirloom worn to 0.3 is
  mended by the smith, for a fee).

**Smoke run** (30 days from DEV with the hours on): money conserved, no faults, nobody hungry; 141 pieces mended, 27
replaced, 1,004 tools bought as they wore through; 0.063 ms a tick.

### Phase 10: hunting and hides (2026-10-25)

Most of it was there: wolves without a post already hunt, fish and gather out of town (doc 42's outwork, from the same
finite game and patches players use), and sell the hides and pelts to the tanneries and butchers at the shop's offer.
The sheep farms already give sheepskins, which the tanneries make into leather.

- **The catch feeds the household first** (`Society::keepForTheLarder`, with the hours on): what a wolf brings in from
  the wild to eat goes into its household's larder, up to three days of a wolf's day's food (47) a head, before it
  sells the rest. The hides and pelts are sold as before.
- **Tests:** `crafting_tests` `theCatchFeedsTheLarder` (twelve rabbits: the larder takes three days' food, the rest and
  the hides are kept for selling, a full larder takes no more; at the old pace, all of it is sold).

### Phase 11, first part: the mint (2026-10-25)

`RatwMint.cpp`, as the model's `players.py` `Mint`, in the day's pass:

- **Hoarders:** players' purses and Holds' treasuries (`chapter:<id>`). Each one's spending a day is followed from the
  ledger (a month's running average, `memory.mint.spending`).
- **Weekly:**
  - dormant money is what each hoarder holds beyond two months of its own spending;
  - hoarded money is the dormant money's four-week average;
  - the money in use is all the money less what's hoarded;
  - the target is the money it first saw (the realm as founded).
- **It strikes**, into the capital's treasury, when the money in use is 5% under the target *and* there's harm: wages
  down 3% over eight weeks, a tenth of the grown idle, or a hundredth of the realm hungry. It strikes the gap, at most a
  hundredth of the target a week, and not before it has eight weeks of wages to judge by.
- **It melts** only what it struck, when the money in use is 5% over and wages aren't falling: at most two hundredths a
  week, the same share of every purse, but not a wolf's with under a week's food money.
- **Logged:** "the mint strikes" and "the mint melts", counted in `minted` and `sunk`.
- **The Dungeon Master's lever:** `memory.mint.on` (saved; on by default). With no players it never acts: nothing is
  dormant.
- **Tests:** `crafting_tests` `theMint` (a player sitting on a third of the money while the realm is hungry: nothing
  for seven weeks, then it strikes, a hundredth a week at most; the hoard spent and the realm fed, it melts what it
  struck and no more, and not from a poor wolf's purse; turned off, nothing).

### Phase 11, second part: Holds as keeps (the user, 2026-10-25: "the Chapter picks trades")

`RatwGameCamps.cpp` (`Game::keepWork`), at every pace:

- **Up to three workshops and a field** at a place (`camp::WorkshopsMost`, `FieldsMost`). The field is a new building
  (20p, 6 work-hours, a Hall's), with two farmhands' posts; it doesn't block the way.
- **A workshop's trade is the Chapter's choice:** an Officer picks one of the realm's businesses that makes things
  (`trade`; the camp panel's picker). Saved with the structure.
- **The day's work, at dawn:** each of the site's hands holds the next post of its role, and puts a day's 8 hours into
  its building (up to two days' in hand).
  - A workshop makes its trade's goods from the Chapter's stores into them: the first of its crafts it has the
    materials for, again while the hours last, at the realm's hours for the craft (`Society::workHours`: the old pace,
    or the pace toward the calibrated).
  - The field brings in the farm's yields in season, and the barn's threshing out of it. It is not on the seasonal
    fields' year: a Hold's field is small and its Chapter's own.
- **Hands are paid the realm's going rate** (`keepWage`; at least the old 5p), so a Hold's hiring draws on the realm's
  labour as a business's does. Those already working keep their wage.
- **The Chapter's stores take any of the realm's goods** (they took herbs, meals and swords), and the place view lists
  all of them. Players lay materials in and take the goods out to sell (their stalls, at their own price).
- **Houses filled from the towns** were there already (doc 16's migration, the user's "draw" rule).
- **Tests:** `camp_tests` (a workshop at the Hall takes up the smithy's work, not a trade that makes nothing; with
  Bracken its hand, a day's work makes swords from the stores' bronze; a field planned; the trade survives a restart).

**Players' goods** were there already: a player's stall sells at the player's own price (doc 54), and residents buy
at it. **Test:** `crafting_tests` `aStallDumpAndTheRebound` (three days of a player's penny bread: the bakery sells
and keeps less; the stall gone, it comes back as it was).

### Phase 12: the orchestrator steps back (2026-10-25)

The user chose to take the orchestrator's steering out now. It had been off since 2026-10-15; what it did is the
society's own rules (Phases 1–11).

**Kept, as a monitor** (`RatwOrchestrator.h/.cpp`, `RatwOrchestrate.cpp`; the names stay):

- **The 23:00 snapshot, every day, in an authored world** (there is no mode any more). Its measures are shown from the
  turn of the day (`Society::orchestrator().last`), and the week's on the evening of the reckoning (`.decision`).
- **Each town:** distress (smoothed over three days) and its kind, hungry, starving, short, poor, idle, a day's food
  cost, days of food on its shops' shelves, takings against running, net coins in from other towns, players' help.
- **The land:** the residents' Gini and median purse, the poorer half's share (by household, a head), the residents'
  share, the money supply.
- **`orchestra::measure()`** is pure, on the game thread: the `Runner` thread is gone (it is cheap without the plan).
- **What reads it:** the troubles' food cost, the "fed the town" deed and the granary project's "empty shelves".
- **econ_watch's** `orchestrator.jsonl` and `orchestrator_towns.csv` carry the measures, now every day.

**Removed:**

- **`plan()`'s steering:** prices, margins, wages, bands, tax levels, channels, funds, orders, grants, rescue, price and
  wage support, the playbook's ladder, the banks, reach and learning, its own pressure and the floor's lift.
- **`RatwChannels.cpp`** (applyOrders, runChannels, the banks, the levy, the savers) and every `mode == "on"` branch:
  `applyPrices`, the wage table in `dayWage`, the tax level in the reckoning, wage support, `postOddJobs`.
- **`Data/Economy/orchestrator.json`** and **`scenarios.json`**; the steers (`orchestra::Steer`, `ScriptedSteer`, the
  `economy.steer`, `economy.unsteer` and `economy.scenario` actions, the DM tool's routes, econ_watch's `--steer`).
- **Kept as constants:** the labourer's day before the first measure is `items::ladder().catalogueDay` (12, as the
  dials' 12); a day's food is `orchestra::NourishADay` (50).
- **Rescue loans** are no longer made. One still owed is written off a month on, or paid off by a friend (doc 57's
  trouble) into the town's treasury.
- **Saves:** an old save loads. Its steers, bands, prices, wages, tax levels and savers' books are left behind. Old
  `fund:`, `bank:` and granary accounts keep their coins and goods; nothing spends them. (DEV was founded afresh at
  Phase 1, so it has none. PROD, if not founded afresh, would keep its savers' coins in the banks.)

**The Dungeon Master's three levers** (decision 5), in the Money tab under the measures. Each is a DM action, audited,
and logged in the game's ledger:

| Lever | Action, payload | Ledger |
|---|---|---|
| The tax rate, 0% to 50% of a week's income, from the next reckoning | `economy.tax` `{"rate": 0.1}` | "the tax rate set from 8% to 10%" |
| The mint on or off | `economy.mint` `{"on": false}` | "the mint turned off" |
| A relief grant from the capital's treasury to a town's | `economy.relief`, the town, `{"coins": 500}` | "a relief grant" (the coins) |

- **The tax rate** is saved (`memory.taxRate`; 8%, `Society::TaxRate`, in a save from before). The households' budgets
  count it too.
- **The DM tool:** `POST /api/economy/tax`, `/api/economy/mint`, `/api/economy/relief`; the Money reply has `levers`.

**The check that nothing changed** (5 days of DEV's export, `--deterministic`):

- **The digest:** 2149ef26b108f66c before and after, with the monitor now measuring every day.
- **`days.jsonl`, `flows.csv`, `prices.csv`, `wages.csv`, `tills.csv`, `food.csv`, `events.csv`:** byte for byte the
  same. Day 5: supply 5,000,000p, Gini 0.489, household Gini 0.536, poorer half 0.137, hungry 0, wage 12p.
- **The tick:** 0.032 ms both.
- **In the game, not in econ_watch:** the troubles' food cost, the deed and the granary project see the measures again
  (they saw none while it was off).

**Tests:**

- `orchestrator_tests`: the monitor alone, 22 checks (distress and its kind, the week weighed, the money's spread, the
  same snapshot the same measures, the saved state and a save from before Phase 12).
- `roads_tests`: `theOrchestratorWatches` became `theMonitorWatches`, with the three levers. `theChannelsSpend` and
  `oddJobs` (the towns' odd jobs, a channel) are gone; `whoPaysWages` lost its wage-support part.
- `project_tests` `visible`: the town's measure needs no shadow mode now.
- `test_dungeon_master.py`: the steers, scenarios, staples and funds tests became `test_the_economys_levers` and
  `test_the_monitors_measures`.
- All suites pass.

### Phase 13: what the wolves buy (2026-10-25)

**The question.** The first long runs with the hours on spent most of the realm's work on wants and on children. Days
15–30 of the 32-day run: children bought 3,784 goods a day for 5,000p (on stipends of 4,541p a day), and wants 1,512
goods for 5,565p.

**Why:**

- **Every want cleared shelves.** `buyForSurplus` takes half of every matching shelf in town, up to the budget: a
  drink fancy was nine jugs, drunk at once.
- **Finery was thrown away.** Bought past six pieces, it was "used" the same day: about 95% of it. Phase 9 already
  wears, mends and replaces what a wolf holds, and Phase 7 planned wants as owned pieces.
- **Children's 3p "trinkets" were materials:** flour, oats, firewood, planks, clay and nails, the bakers' and the
  supply shops' own, destroyed as "kept by a child". They took about 85% of the realm's oats.
- **Stipends grew with the household's money:** a penny more for every hundred to spare, about 31p a child a day.
- **The want fund was counted again each day.** It was clamped only on a day with nothing to spare.

**Built:**

- **One thing at a time, from what a shop sells** (`RatwSurplus.cpp`). `offersOf` lists a town's goods on sale of a
  kind once a day pass, in an order by lot: a shop's wares, and food at a food shop (`onSale`), never what it holds to
  make things from, never a shelf's last two. `buyOne` takes, from the buyer's own place in that order, the first four
  still on the shelf that it can pay for, and buys the cheapest (the model's `buy`: the cheapest of four sellers).
  `buyForSurplus` keeps its bulk buying for institutions, alms and farms, but skips materials too.
- **Wants are the model's** (`wants`): one a day per household, each lodger its own, out of its want fund, bought by
  the member whose turn it is. In the model's order:
  - a rapier for the rich (the top sixth by month's income, holding twice a rapier's going price) with none;
  - a sword for a household that works, with no blade (a guard's issued sword counts);
  - finery while it holds fewer pieces than it has members, two more for the rich, the dearest of the four;
  - all kept, and worn, mended and replaced as belongings (Phase 9).
- **With none of those to buy, one thing it fancies,** by its own taste as before, used up: a drink, soap and scent, a
  pastime, something for the home, or a treat. A treat is a dish, never a staple; never in a town with wolves hungry
  (70, as the monitor counts them); only off a shelf holding its days' keeping of its sales.
- **And works on its home** (`worksOnHome`, doc 63's outlet): with a fund that covers a unit, up to half of it, by the
  amount. A unit is 11 hours of a building business's time (`WorksHours`; a business with a craft making building
  goods) and a batch of its own building goods, at the town's hour and their cost, with the ladder's markup. Four
  builders by lot, the cheapest first, as a mend is booked: out of the hours it banks, while what's booked is under
  half a day of its hands' time. Its goods are used up. The hours count toward its hands (`worksRate_`, a month's
  running average beside `mendRate_` in `handsWanted`). What found no hours, a unit at most, the four hear of between
  them, a quarter of it, as a mend's. Lodgers' works are on their bunkhouse. With work by the hours only, as mending.
- **Founded with what they own** (the model's): once, the day belongings are first counted, a sword for 70% of
  households that work (20% of those that don't), a rapier for 12%, a piece of its town's finery for half. 658 pieces
  on DEV's founding.
- **Children** (`childrenAndStipends`): a stipend of at most 3p, a penny for every two weeks' worth over the household
  rule's target and the tax owed, out of the household's want fund (one rule). None goes to a child holding a
  fortnight's (42p). A child buys one thing a day, at half its purse and 3p at most: a bite when hungry with nothing on
  it, otherwise a trinket (jewellery, a hat, a scarf, paw wear, a toy; not a pipe). Trinkets are kept; past three, one
  is lost ("lost by a child").
- **The want fund is clamped every day** to what the household holds over its target (`householdRule`).
- **Not saved:** `worksRate_`, as `mendRate_`. Nothing new in a save.

**Smoke run** (30 days of DEV's export, `--deterministic --hours`): money conserved every day, no faults. Against the
32-day run before:

| | before | after |
|---|---|---|
| children's spending, days 15–30 | 5,000p, 3,784 goods a day | 366p, 165 goods |
| stipends | 4,541p a day | 73p |
| wants | 5,565p, 1,512 goods a day | 1,825p, 404 goods |
| works on homes | | 603p a day, and 72 building goods |
| hungry wolf-days, days 15–30 (starving) | 568 (369) | 164 (58) |
| day 30: hungry, starving | 174, 147 | 106, 41 |
| the pace, day 30 (mean, days 15–30) | 0.25 (0.30) | 0.43 (0.35) |
| household Gini, day 30 | 0.569 | 0.567 |
| the poorer half | 0.138 | 0.135 |
| the going wage | 12p | 12p |
| hours lost to full banks, days 15–30 | 35.3% | 36.3% |

- **The fill:** about 900 pieces of finery and 90 swords in the first nine days, then 5–10 pieces and a sword a day.
- **The money stays home.** Shop tills hold 203k on day 30 (333k before), farm tills 117k (175k); wage earners 918k
  (821k).
- **Works are short of builders' hours:** about 600p a day of the funds' half. The rest waits in the funds, clamped,
  and the builders want more hands (carpenters, sawmills, masons and the brick and plaster works, by name: 53 hands
  wanted and 21 held on day 29, against 31 and 21 before).
- **More hours are lost:** goods nobody buys now fill shelves. That is for the labour market (layoffs below pace 1,
  the wage), not wants.
- **The tick** is the same as before: 6 days run side by side on a loaded machine, 0.056 ms against 0.058.

**Tests:**

- `roads_tests` `childrensStipends`: the town has a jeweller, as the child buys trinkets, not the baker's flour.
- `roads_tests` `grownWolvesBuyWhatTheyWant`: a want is the household's, bought by any of its grown members.
- `crafting_tests` `aHouseholdsWants`: one piece of finery a day, kept, never from a till that only holds it; with all
  it wants, the carpenter works on its home with its planks, for half the fund at most.
- All suites pass.

**For the user:**

- **No business makes rapiers** (only the players' armourer recipe), so the rich never find one and go on to swords
  and finery.
- **Children's trinkets:** lost one a day past three keeps a little spending going. Kept for good, children would
  stop buying once they have three.
- **Treats** are the game's own (the model has none). They're kept here, out of a town gone hungry.
