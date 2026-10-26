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

### Phase 13: players who push (2026-10-25)

Before the balancing, `econ_watch` measures what the review of the frozen wage and the lost hours asked for, and can put
scripted players in the world. Nothing in the society changed but three small accessors (`Society::goingRates`,
`employerTownOf`, `workBankedOf`): a 30-day run without players gives the same `days.jsonl` as before, day for day.

**The measures** (`Tests/econ_watch.cpp`):

- **`work.csv`:** a producer's work is credited to the farmer but its hands are its till's, so its hands and hands
  wanted are now looked up by its till (they read 0). New columns: the till, its hours in hand at the day's turn, and
  its running hires' slots and slots taken. (Since the review, the till's own columns, hands, hands wanted and hires,
  are in `tills.csv`, once a till a day: in `work.csv` a till could have two rows, the farmer's and its own.)
- **`days.jsonl`:** every town's going rate and the realm's (`model.going`, "" the realm's).
- **`books.csv`:** every employer's book each day: wage, the going rate for its skill, open days, calm weeks, short
  pay, posts open and filled, skill.
- **`hires.csv`:** each hire running, at 23:00: payer, kind, slots, pay a slot, last day, slots taken, those open at
  its dawn, and who took them (jobless, child, works, idle, called, better or other, with its own employer), as they
  stand at 23:00.
- **The run's stamp**, at the top of the log and in `run.txt`: the command, the source's `git describe` and commit,
  and the binary's build time (b0 and b1 straddled the hands-wanted fix unseen). (Since the review, the describe is
  taken when the binary is built, a dirty tree's with its changes' SHA-1 (`Tests/econ_watch_stamp.cmake`), and a run
  warns when the source has changed since; the build time is the binary's, in UTC, and the compile time local.)
- **`--from` past 24** is hours into the calendar (6006: day 250 at six), by a save restored at that moment.

What the new measures show on day 30 of that run (DEV's export, `--hours`): the realm's going rate is still 12p and
Ridgemere's too, while the small towns' have moved (Larkfield 12.2p to Mirebridge 14.3p); 104 of the 384 businesses'
books are at 12.0p. Producers' hands now read: 234 against 587 wanted, each till once. (Both were first given twice
over, 208 books and 468 hands against 1,117: day 30's rows were there twice, the run's end repeating them, and a few
tills twice besides; recounted by till.) Of the hires, 1,419 slots were running on day 29
and 188 taken; over the month, of the slots of the 4,426 hires, those taken went to idle hands (2,503), children
(1,325), wolves drawn by better pay (699), the jobless (60) and others (39). Most slots are households' help about
the home (3,776 of 6,465).

**Players who push** (`--players SCENARIO[,...]`, `--players-from DAY` (7), `--players-days N` (to the end),
`--players-bodies walk|still|jump` (walk); `Tests/econ_players.cpp`). They act as the game lets a player act:
`Society::addPlayer` (its welcome grant), `Society::trade` with a shop's keeper (99 a trade, what the shop wants and
can pay for), `World::addPlayer` and `World::moveTo` for a body (a door opened and entered, a seam walked across),
`World::steal` with the body beside the mark, `World::setDead`, and `World::removePlayer` when their days are over.
Four stand-ins, said so: hoard's yield, the whale's chests, flood's catch and gift's walk to each household (below);
and a player's own goods never spoil (`Society::spoil` passes players by), as in the game. Every coin they're given comes
from the capital's treasury by the ledger ("test: a player's money", not earnings). Their days count from when they
start (a run that starts after `--players-from` starts them on its first day, and says so). `players.csv` has each
thing done; `days.jsonl` a `players` block a day (a day's record is made at its midnight, before the players act at
that midnight, so what they do then is the new day's in every file).

| Scenario | What it does |
|---|---|
| `hoard` | 20 players roam the capital's hunting and gathering grounds, a spell an hour 8 to 17, from the same game and patches as its own hunters (`World::harvestAt`, through `Society::day().harvest`); at 17 they sell it all to the shops, and keep every coin. A stand-in: a spell gives an outworker's yield (a hunt's kill at its odds, fierce game passed by; a forage without its wait or practice), not a player's `World::forage` and hunts, which would want twenty bodies in the wild and a fight each |
| `corner` | a player with 30,000p buys every loaf, sack of flour and meal on the capital's shelves at six each morning for its first week, then sells them back to whichever shops take them |
| `dump` | 5 players sell swords, iron swords and hides to their towns' shops every day, as many as they take (the goods as a Hold's smithy makes them, and hunted; their materials not counted) |
| `whale` | a player with 5% of the money (250,000p) buys every rapier and piece of finery on every shelf each morning, into chests past a pack's kinds, 99 at a time. A stand-in: the chests are accounts of its own ("let:"), free, with no lodging and no rent |
| `poach` | a Hold takes 20 of the capital's hands from their businesses (off their schedules, as `station`ed staff are) at twice the realm's going rate, paid each dawn: the Chapter's treasury has the first day's pay when it takes them, and is topped up each dawn to that day's pay at that day's rate (what the capital's treasury can't give, noted "short") |
| `kill` | the key makers die on the first day: every smith of the realm, a town's only miller, the biggest farm's farmer; succession is the world's (the dead keep no shop the rest of that day) |
| `theft` | 5 thieves with bodies try a purse in the capital every quarter of an hour from 9 to 21 (`World::steal`; the watch is the world's own), and fence what isn't coin at 21:30. Their bodies arrive at five of the capital's shops and walk up to a mark in the place they're in (`World::moveTo`, trying the purse on arrival), and on the hour (or with nobody about) on into the next of the capital's places; `still`, they stand where they arrived and try a purse that comes within reach, each purse once a day (since the second review: it had tried the keeper beside it every quarter); `jump`, they're put beside each mark (`World::teleport`, a Dungeon Master's move: the first runs') |
| `visit` | 5 players with bodies go about the capital as the thieves do, and take nothing: theft's control (bodies on the stage alone) |
| `winter` | 10 players with 3,000p each buy every bundle of firewood in their towns each morning of the calendar's autumn (days 184 to 274), and keep it |
| `flood` | 10 players sell a staple (`--players-staple`; raw meat unless asked, since the third review: fish sat at the penny's floor) to the capital's shops that buy it on the hour from 8 to 18 for two weeks (`FloodDays`), as many as they take, then stop; does the price come back, and do the capital's makers of it (the posts that bring it in or make it, by work or home: for raw meat its two butchers) keep their living? A stand-in: no player's hunt or catch is run, so each pack is topped up to 99 each morning ("landed"), made; the selling is `Society::trade`'s. Two things the game does that shape it: a shop pays a player its own normal cost (`Society::offerAt`; a shop that doesn't make it, the average it paid), so buying the flood never lowers a shop's cost, and only its margin can fall; and a player's goods never spoil, so a pack landed days ago goes to a shop as a fresh lot. A staple the stores price (meals, herbs, swords) is refused |
| `strike` | a Hold takes a tenth of the capital's hands (wolves of working age paid by an employer, not keepers, the watch or the land's producers; rounded up, 26 on DEV's export) from their work for a month (`StrikeDays`, 28), then lets them go: do hires and wages answer, and does the town come back? As the game lets a Chapter take a wolf in work: hired as a party's companion out of its working hours, then stationed at the Hold (off its schedule, its post still its own), on a companion's wage (6p, 10p for a roving trade: `Game::wageFor`), paid each dawn as `poach` is. Not those `Game::whyNotJoin` refuses at any hour (since the third review): the Quickened, a wolf beaten down, one with a warrant or in the watch's custody. What the stand-in skips: the game asks each awake and out of its working hours ("They are asleep.", "They have work to do until..."), and companions are 4 at a time in the realm and one more for each 4 players online (a stationed one frees its place), so a tenth wants a few players over some days; here all are taken at once, at the start's quarter (midnight on DEV's runs). A Hold takes into its own posts only wolves without work |
| `buyout` | 5 players with 30,000p each buy every piece of food the capital's shops sell (bread, meals, smoked fish, cheese, raw meat: every good that nourishes, of every quality) at six each morning for a week (`BuyoutDays`), as far as their purses run, and hold it to the end (never sold back, never spoiling). A food corner, unlike `corner`'s (one player, bread, flour and meals, sold back on the eighth morning): do the capital's households go short, do its makers answer, and do the prices come back? |
| `drain` | players take every place at every bench in a town: two beside each maker who works a bench (a business that crafts, not the land's; not a shop's keeper, whose task there is to trade), the most a bench takes (the workshop's `most` 3, its maker one), arriving at its work spot. Each quarter of an hour its maker is at work, a body not at work asks to be (`World::helpAtWork`); at work, it does its part each beat (`World::noteActive`), paid by the beat at the town's hand wage (`a hand at the bench`) from what `Society::tillOf` gives for the maker (for a hand, whose post has no till, its own purse: below), and its paw makes the maker's batches go faster. The town is the one (not the capital, which would want a hundred bodies and more) with the most such benches: Frosthollow on DEV's export, 15 benches, 30 bodies. Not a stand-in, but not the brief's either: a player takes no odd job and no hire (only residents take them, `Society::oddJobFor`), so the nearest the game lets players come to taking a town's work is its benches (and its farms' harvest and threshing, in autumn and winter) |
| `gift` | a rich player (150,000p, a month of it) gives 100p at ten each morning to each of the realm's 50 poorest households (as econ_watch counts a household: a home; each wolf of a barracks or lodgings its own), into its first grown member's purse: does it lift prices, and is every coin still there? As the game gives (`Game::gifted`: `Society::shift`, kind `a gift`; 10,000p a give at most); a stand-in in that no body walks up to each (the game wants the two face to face, within 2 tiles), and the warmth a gift brings isn't kept |

When their days are over, the thieves' and visitors' bodies leave the world (as a player's who logs off; one in a
fight stays till it's over), and the Hold's hands go back to their own lives.

**What the game didn't let them do** (each a question for the user, at the end):

- **Nobody makes rapiers** (no craft in `crafts.json`), so the whale buys finery only: about 2,000p a day of hats,
  charms, beads and scarves. **Houses** can't be bought; a Hold's buildings cost a few hundred pennies, paid to the
  capital's treasury. A whale can't spend: in 9 days it spent 17,801p of 250,000p.
- **A Hold can't outbid:** a Hold pays the realm's going rate (`keepWage`) and takes only wolves without work; a
  stationed companion keeps its 6p or 10p (`wageFor`). `poach` pays twice the rate anyway, as a stress.
- **A Hold's hands and companions never eat:** off their schedules (a leader, `LifeBody::companion`) they decide
  nothing, so they go hungry within a day and starve by the next. In `poach`, 13 to 18 of the 20 were hungry from the
  second day, the only hungry in the realm.
- **Theft takes from a wolf's purse** (1 to 5p, now and then a masterwork, a meal or herbs), never a till.
- **Shops take few of a player's goods:** a good a shop makes, up to 4 on its shelf; what it supplies or buys, up to
  40. A player dumps at scale only at a stall (doc 54; `crafting_tests` `aStallDumpAndTheRebound`).
- **Players can't fish:** no `World::fish`; `flood`'s catch is made in its packs. (A player forages herbs, a picking
  every 4 seconds while the patches last, but herbs are still priced by the stores, and the capital has no herb
  gatherer.)
- **A Hold takes no wolf in work into its own posts** (`whyNotMigrate`: "They have work where they are."); one comes
  only as a party's hired companion, stationed, at a companion's 6p or 10p. `strike` takes its tenth that way.
- **Players take no odd jobs and no hires** (the third review's): the board's work is taken only by residents
  (`Society::oddJobFor`). A player works beside a resident: at its bench (`World::helpAtWork`, paid from its till by
  the beat), at a farm's harvest or threshing in their seasons, or on a town project (unpaid). `drain` takes every
  bench of a town that way.
- **A gift is face to face** (`Game::giveRefusal`: within 2 tiles, out of a fight, 10,000p a give at most); `gift`
  gives by the same ledger move without walking up to each household.

**The 10-day checks** (DEV's export, from day 2, `--hours`, deterministic; winter from day 250; before the review's
fixes, so theft's and visit's bodies jumped): money conserved every day in every run.

| Scenario | What happened | Against the run without players |
|---|---|---|
| `hoard` | in 9 days foraged 1,176 pieces and hunted 250 (137 of them meat, 21 hides); sold 1,327 for 1,939p, about 11p a player a day, all kept (2,339p with the welcome grants). Standing at one ground each, the patches gave them half that | Gini and the poorer half within 0.0004 |
| `corner` | bought 2,020 pieces (5,740p) the first morning (1,195 flour, 1,147 meals, 92 loaves: the capital had little bread), 2,439 in the week; sold 2,361 back on the eighth morning for 9,240p, 576p more than they cost, all of it on the meals (a meal is priced by the town's stores, dearer when the town is short, and the corner had made it short) | no hunger in 10 days (households' larders); Gini and the poorer half within 0.0011 |
| `dump` | sold 363 hides (2,432p) and 6 iron swords in 9 days, 133 pieces the first day and almost none after; no bronze sword | within 0.0002 |
| `whale` | spent 17,801p in 9 days (4,772 pieces of finery) | the capital's treasury 64,000p against 312,000p (its funding); Gini 0.002 lower, the poorer half 0.0013 higher |
| `poach` | 20 hands taken, 480p a day paid | 13 to 18 hungry (the hands); Gini and the poorer half within 0.0012 |
| `kill` | 12 died: 10 smiths, Coldwater's miller, Barleycombe's farmer; 1 post filled by day 5, 10 by day 8 | Gini and the poorer half within 0.004 |
| `theft` | 1,002 caught at it and 246 thefts took 947p in 9 days (about 20p a thief a day, all coin); the watch held a thief 90 times and took fines and restitution back, so the five ended with 10p | 104 hungry and 63 starving on day 10 (none without players), the pace back to 0 (0.16), 9% fewer hours worked: see `visit` |
| `visit` | 5 bodies moved about the capital each quarter of an hour | 11 to 21 hungry from day 6, the pace 0.04 on day 10, 5% fewer hours worked: residents on the stage fetching a meal from home don't get there (on day 8 of `theft`, 47 of the 66 hungry were in the Heights Market's cell, `w_ridgemere_1_1`, on their way home; others are offstage in houses not theirs) |
| `winter` | bought 16,835 firewood for 17,075p in 7 days: 132 the first morning, 5,700 the fourth, 729 the seventh | Gini up to 0.005 lower, the poorer half up to 0.003 higher; the run ends before winter (day 275), when it would tell |

**The bodies' control** (the review: were the hungry the jumps' doing?). The same 10 days from day 2, on the merged
Phase 13 with the review's fixes (`4e83cec`), the bodies walking, standing still and jumping; the run without players
is the 30-day smoke's first 10 days (its digest is the merged run's, `m1`'s). Money conserved every day in every run;
Gini and the poorer half within 0.006 of the run without players in all; the pace 0 in all, as without players.

| Bodies | `visit`: hungry, starving (day 10) | `theft`: hungry, starving | Hours worked on day 10 (4,841 without players) | A tick (2 threads) |
|---|---|---|---|---|
| none | 0, 0 | | | 0.09 ms (3 threads: the 30-day smoke's) |
| walk (`4e83cec`, before the re-aim throttle) | 191, 118 (the first on day 3) | 93, 45 (day 4) | `visit` 3,495 (28% fewer), `theft` 3,936 (19%) | 1.6 ms, 1.1 ms |
| still | 179, 142 (day 3) | 176, 141 (day 3)\* | 3,784 (22%), 3,853 (20%) | 1.2 ms |
| jump | 47, 46 (day 5) | (the first runs': 104, 63) | 4,564 (6%) | 0.7 ms |

\* Not a control of its own: the still thief then tried the one purse beside it, its shop's keeper, every quarter
(1,363 tries in all, 14p taken, never held), so its hungry were standing `visit`'s, day for day, from day 3 to day
8 (3, 33, 81, 76, 122, 135). Since the second review a still thief passes over a purse it has tried that day (below).

So it isn't the jumps: bodies that never move starve the most, and walking ones as many. Where: with the still bodies
(in five inns) 162 of the 179 hungry are in the street outside them (`w_ridgemere_1_2`), on the stage, 135 of them
fetching a meal from home (most of the rest asleep); with the walking ones (most in the Heights Market,
`w_ridgemere_1_1`), 135 of the 191 in the next cell along (`w_ridgemere_2_1`) and 46 in the market's own; all but 10
on the stage. With the jumping ones the hungry are elsewhere: offstage, in houses not theirs (the Grey Gull Inn, the
Houses' halls), on their way upstairs home. A trace (`RATW_TRACE`, still bodies) of three of day 3's hungry: each
walked onto the stage into `w_ridgemere_1_2` across its north seam (at 52,0 and 235,0) on its way home to eat, and
stood there 10 and 21 game hours before it got home, the third still there at the run's end. Real players' bodies
on the stage would do the same: a fault in the residents' walking at the stage's edge (World: `placeOnStage`,
`headFor`), not the harness's.

Standing still again on the merged Phase 13 with the second review's fixes (`e0e6f45` and `af5d842`: the same 10
days, 2 threads; the economy is not `4e83cec`'s, so set these beside each other, not beside the table): `visit` 55
hungry and 36 starving on day 10 (111 and 92 at the worst, day 8), 4,275 hours against 5,214 without players (18%
fewer), 1.18 ms a tick; `theft` 152 and 106, 4,001 hours (23% fewer), 1.22 ms. The two were the same, day for day,
to day 6, and parted after (the watch's doings set the thief's world apart). The thieves, in five inns, each tried
its inn's keeper once a day (35 tries in nine days, 34 of them on the five keepers; one theft, 3p): standing at a
shop, nobody but its keeper comes within reach, so a still thief is still no control for theft of its own; `visit`
still carries the bodies' control. Without players, no one hungry.

**Notes for the long runs:**

- The players' funding comes out of the capital's treasury, so a big one (the whale's) leaves the capital less to
  spend: a run that measures it should read the treasury beside the rest.
- **Players' bodies make residents hungry**, whether they walk, stand or jump (the bodies' control, above). Every other
  run (and every run before) has nobody on the stage; with five bodies in the capital, residents on the stage near
  them fail to get home to eat. It wants fixing before players come, and the long runs, which have no players on the
  stage, can't see it. (Mended in the world's walking: "residents near players' bodies", below.)
- `theft` and `visit` cost far more: everyone near the bodies is on the stage, and a tick took 0.6 to 3 ms on one to
  three threads against 0.1 to 0.2 (`theft`'s 10 days took 55 minutes; walking `visit`'s, 75).
- `troubles_tests` failed once with ten runs going at once ("the loan paid") and passed on three runs after and on
  the commit before: it is flaky under load.

**For the user:**

1. Should a Hold's hands (and a party's companions) eat? Today they starve at their posts within two days.
2. May a Hold outbid the going rate, and take wolves who have work? Today it can't (`keepWage`, the jobless only).
3. Rapiers: should the armory make them (a craft)? None is made, and a whale has little else to buy.
4. A corner can profit on meals by selling back into the shortage it made (576p on 7,453p in a week). Is that a fair
   reward for the risk, or should a shop buy meals at its own normal cost, as it does every other good?
5. Residents near players go hungry (above), with bodies that walk or stand still as with jumping ones: residents
   who walk onto the stage at its edge, on their way home to eat, stand there for hours. If the stage's life is meant
   to match the unseen's, it's a fault to find first (in the world's walking, not the economy): the long runs, without
   players, can't see it. (Found and mended: "residents near players' bodies", below.)
6. A hand stationed at a Hold keeps its post, and its employer counts it a hand though it never comes (`strike`):
   a month away moved no wage, opened no post and posted no more hires; the realm's hours hardly moved (0.2% fewer
   with the pace held, the tidy's run: the 1.5% first read was the pace's), and its service wages fell 11%. Should a
   post whose holder hasn't come for some days stand open (a stand-in hired, or the post given up), so that taking
   hands draws the wage up?
7. (Reworded by the third review: the first `flood` was on fish, which sat at the penny's floor at a 12p day, so it
   couldn't show a price falling, and its finding was the floor's, not the market's. And again by the tidy, on the
   run with the pace held: "the players' runs with the pace held", below.) A shop pays a player its own normal cost
   for a staple (`Society::offerAt`), so a flood never lowers what a shop pays, nor what it asks: through two weeks of
   raw meat the shops that buy it in asked 14.0p to 14.4p, and after it 14.4p rising to 15.6p, as without players. No
   shop's price fell. What residents paid on average fell, from 17.0p to 14.3p, only because they bought five times
   as much of it (3,139 pieces against 603) from those shops rather than from the butchers, who price it at their
   cost of making it. Should a shop pay a player less than its own cost when its shelf is over what it wants (a
   glut's price), so that a flood lowers what it pays and then what it asks? Recommendation: yes, below its cost by
   how far its shelf is over its target, never under the penny, so a glut shows in the price and passes.

**The review's fixes (2026-10-26)** (`4e83cec`; `Tests/econ_players_tests.cpp` checks each, and econ_watch's files
on a little world of its own, a day and a fifth in under two seconds):

- `poach`: the Chapter's treasury has the first day's pay when it takes the hands, and each dawn it is topped up to
  that day's pay at that day's rate (it had a week's, at one rate, put by only when the players started at midnight
  on a seventh day; started mid-day, every hand went home unpaid at the first dawn).
- When their days are over, the thieves' and visitors' bodies leave the world (`World::removePlayer`; they had stood
  on in the capital, keeping the stage there), and the Hold's hands go home.
- `theft` and `visit`: bodies walk (`--players-bodies`, above), or stand still, or jump as before.
- `winter` buys in the calendar's autumn only (it bought from spring on, before `Society::WinterStart`).
- The players' days, and the corner's buying week, count from when they start; a run that starts after
  `--players-from` starts them on its first day and says so (`--from 6006 --players-days 10` had never started them).
- `work.csv` has each account's hours; a till's hands, hands wanted and hires are in `tills.csv`, once a till. The
  run's end writes no rows of its own in `work.csv`, `books.csv`, `tills.csv` or `residents.csv` (it repeated the
  last day's under the same day). The figures above that came from them are recounted.
- The players act after a midnight's record is made: what they do at midnight is the new day's in `days.jsonl` as in
  `players.csv` and `flows.csv` (the record before had it).
- The stamp is taken at build time; times say their zones.
- The dead keep no shop: a keeper who died since the morning's list, or left its post, is passed by, and `kill`
  makes the list again (in the test's little world, the `dump`ers had sold swords to the smith `kill` had just
  killed).
- The whale's goods go into its chests 99 at a time (the journal notes 99 a move at most; a stack of 190 showed as 99),
  and the chests are said to be free; hoard's yield is said to be a stand-in (the table).
- `camp::HoldWage` for the Hold's least wage; the comments put right.

**The second review's fixes (2026-10-26)** (`af5d842`, `95a4057`, `1c5b648`; each guarded in
`Tests/econ_players_tests.cpp`, and each guard was seen to fail with its fix taken back, but the scratch folders'):

- The test's worlds and econ_watch's output are each in a folder of the run's own (the process id in its name: suites
  run several at once here, and one run had cleared another's), removed once every check has passed.
- A walking body turns after its mark every two seconds at most since it last aimed (`Walker::aimedAt`); it had
  re-planned at every look, twice a second, once two seconds had passed since it set off. (A mark kept across the
  place from it, crossing 16 times in ten seconds: it turned 15 times before; now once in two seconds at most.)
- A thief standing still passes over a purse it has tried that day (above); `players.csv`'s `tried` rows name the
  purse.
- The stamp: built from no git checkout, it says it can't tell, never a warning (the run had read "" against the
  build's "(unknown)"). And the run's check is now of the code econ_watch is built from (`CMakeLists.txt`, `Core`,
  `Tests`): the build writes the SHA-1 of the changes from its commit to them (`RATW_BUILT_CODE`), the run works the
  same out against that commit (`econ::sourceDrift`), so a docs edit, or a docs-only commit since, warns of nothing.
  Checked on a git tree of the test's own, with the stamp's own script.
- `poach`'s `short` row has the pennies it lacked as its quantity (item "pennies lacking") and no coins: coins are
  pennies that moved, in every row.
- Real guards: econ_watch runs the little world for 2.2 days, so `books.csv` has rows (an employer's book is opened
  at its weekly review, on its own day of the seven), each day's once; a `poach` with the treasury emptied notes its
  shortfall and sends the unpaid home; econ_watch runs `flood` and `strike` by name.
- The bodies leave on the real map: DEV's export, 3 days, `--players theft,visit --players-from 1 --players-days 2`
  (`af5d842`'s binary, as its `run.txt` says; the two commits after it, `95a4057` and `1c5b648`, change only `flood`
  and `strike`, so theft's and visit's code is the same). All ten left at day 3's midnight (`World::removePlayer`), none then in the watch's hands (three
  had been at day 1's end), and the watch raised 13 more warrants for the thieves gone, on day 3, for earlier tries;
  the end's record has no bodies, and money was conserved in every record. The 65 hungry and 35 starving of day 2's
  end were none six hours after the bodies left.
- The 30-day smoke without players (`1c5b648`, `--threads 3`, 0.1 ms a tick, 14.4 minutes): money conserved every
  day; `days.jsonl` but its end, `flows.csv`, `tills.csv`, `work.csv`, `books.csv`, `residents.csv` and `prices.csv`
  are byte for byte the merged baseline's (`m2`; digest `aded872975ff165c` in both) and the clean `e0e6f45`'s first
  30 days: nothing changes without players. Day 30: no one hungry or starving, the household Gini 0.5612, the poorer
  half 0.1402, the wage 12p, the pace 0.071, the going rates 12p (the realm's and Ridgemere's); days 15 to 29, the
  owners' profit 14,549p a day against the keepers' wages 4,378p, the hands' odd jobs 2,613p and service wages 1,448p.

**`flood` and `strike`** (DEV's export, `--hours`, deterministic, 3 threads, players from day 2, `1c5b648`; against
the clean `e0e6f45` without players, 44 days, whose first 30 are the smoke's). Money conserved every day in both.

`flood`, 30 days:

- The ten sold 7,662 fish in their two weeks to the capital's six shops that buy it (three fishmongers, three
  provisioners), at 1p each, and kept it (with 200p of welcome grants): 117 the first day, then up to all 990 they
  landed a day (days 8 to 11), as fast as the shops sold and used them. The shops' shelves of fish went from 99 to
  240 (40 a shop) on the first day and stayed full for six days.
- (The third review: this finding is the penny's floor's, not the market's. `Society::pennies` is 1p at least, and
  fish asked 1.07p to 1.16p before rounding, so however big the glut it could never sell under a penny; and a shop
  pays a player its own normal cost, so buying the flood couldn't lower its cost either. `flood` is now on raw meat,
  at a 60p day: "players who push, the third review", below.) **The price didn't fall, so there was nothing to come
  back.** The shops' asking price, before the penny, was
  1.07p and 1.13p the two days before, 1.13p to 1.14p through the flood and 1.13p to 1.16p after; every fish was
  sold at 1p before, during and after, and a player was paid the shops' normal cost, 0.99p to 1.00p (a penny). A
  shop prices fish at what it paid for it, and its margin falls only on a shelf that won't move: fish keeps a day and
  a half, and the shops made it into smoked and salt fish (10,757 used in crafting in the two weeks against 5,644
  without players), passed 2,477 more to other shops, and let 653 more spoil. The households bought the smoked fish
  (397 on day 28 against 234).
- **The makers kept their living.** The capital's nine fishing posts (four on the piers, five on the Sound) were
  held throughout; they sold 3,949 fish in the two weeks against 3,474 without players, and 6,316 against 4,440 in
  the two after (the shops' trade in fish had grown), working 2,026 and 3,994 hours against 1,835 and 2,602; their
  tills held 2,937p on day 30 against 3,033p.
- Day 30: no one hungry or starving; Gini 0.5620 (0.5612 without players), the poorer half 0.1405 (0.1402), the pace
  0.054 (0.071), the wage and the going rates 12p; days 15 to 29, the owners' profit 14,404p a day (14,549p), the
  keepers' wages 4,352p (4,378p), the hands' odd jobs 2,621p (2,613p), service wages 1,456p (1,448p).

`strike`, 44 days (the hands held from day 2 to day 30's dawn):

- 26 hands taken (wharf hands, porters, laundresses, hall servants, a gardener, the Wharf Tannery's, the Blue Vats'
  and a masons' yard's hands...). Off their schedules, 24 were hungry by the first day's end and all 26 the next,
  12 then 25 starving, and all 26 by the month's end (as `poach`'s hands, and the companions'); the day after they
  were let go, none.
- **Hires and wages didn't answer: the labour market didn't see them gone.** Every one kept its post all month (26
  of 26) and after; their employers counted them as hands still (Ridgemere's books' hands 518 against 519 on day 30)
  and opened no posts for them (72 posts open a day against 77 without players), so no wage rose: Ridgemere's
  employers' mean wage was 12.68p on day 30 against 12.75p, the going rates 12p throughout, the hires' slots as
  without players (42,473 against 42,571 in the month; taken 15,976 against 16,658). Their employers paid them
  nothing while they were away: the Hold's 6p a day was all their pay, more than the 4.8p a day they had earned
  (4,368p in the month against 3,529p), so they ended richer (63,994p against 61,791p on day 44).
- The work just wasn't done: 1.0% fewer hours in the month (139,426 against 140,838), service wages 10% down
  (1,302p a day against 1,448p, days 15 to 29), the hands' odd jobs 5% (2,471p against 2,613p), the owners' profit 3%
  (14,157p against 14,549p), the keepers' wages 1% (4,325p against 4,378p).
- **The town came back at once** in the work: the day after they were let go, the hours were the run's without
  players (+0.3% over the two weeks after, +1.0% the last week), and no one was hungry. The wages paid stayed 2.2%
  lower for the two weeks, fewer posts stood open (76 a day against 105), and on day 44 the poorer half held 0.1378
  of the money against 0.1410, Gini 0.5618 against 0.5588, the pace 0.125 against 0.143.

### Phase 13: the labour market unpinned (2026-10-25)

The first of Phase 13's balancing, from a diagnosis of two runs (60 and 32 days from DEV, calibrated hours): the going
wage never left 12p, and a third of the realm's credited hours were lost to full banks. This part is the labour market
and the hands; the food chain, the wants and the pace are other parts.

**The going rate** (`tendLabour`; doc 63's `market_wage`):

- **Whose wages:** the unskilled trades that make things (skill 10 or less): businesses with crafts, and the land's
  producers but its seasonal fields (their pay follows the rate). Not food stalls or moneychangers, which make nothing:
  they never want more hands than they have, never post a hire, and so never raise. Half the old measure's weight was
  at shops paying 12p that never wanted more, and two of Ridgemere's market stalls alone held 56 street posts
  (`payerOf` gives a cell's posts to its first merchant).
- **Each as a labourer's day:** its wage over 1 + skill / `skillWage`. Nearly every counted maker is skill 10, and the
  premium was counted twice.
- **By the hands its work wants,** one at least and those it has at most (`handsWanted`, worked out once a day for the
  reviews and the rate).
- **One market:** a town has its own rate only with 40 hands' weight (`TownRateWeight`; today only Ridgemere), else the
  realm's. A small town's producers are kept to twice the realm's rate, not their own.
- **Measured afresh daily** (a town that loses its makers falls back to the realm's) **and saved** (`memory.going`): a
  restart no longer prices its first day at 12p.

**Posts and hires:**

- **A post whose holder is away on another business's hire stands open** while it's away, as the model's `hire()`
  opens the post it leaves: its employer's open days count, and it raises after four. Not when its own business has no
  work for it (its hands are free to go), nor the Town Works'. Its hands count as before: it comes back.
- **The jobless and the idle** (not children, not the land's call) take the best-paid business hire open to them
  before chores, the first of the best by the list. A raise now draws them.
- **A business with more work in hand than its hands can use** (`idleBusiness`: its bank full; a producer's its own; a
  field with none in its window) hires none, and its running hires end at its daily review.
- **A business that can't pay a hire's share** ends the hire, and its weekly review hears it fell short.

**Hands wanted** (`handsWanted`):

- **A craft by its first good,** as `craft()` judges it (a butcher's meat, not the hides). **A good several recipes
  make** (pig and sheep for meat, three for leather, two for plaster) **counted once,** by the first in `craft()`'s
  order with its materials in hand.
- **A craft short of materials** today or yesterday plans no more than 1.1× what it could make (what it made lately, a
  fortnight's average, or what a week of its materials in hand would make), and not half again near bare: its shelf is
  bare for want of them (doc 63's feasible). The foundry's copper and tin crafts, whose ores nobody brings in, asked
  for hands they could never use.

**The cord loop.** A maker buys what its own crafts make as a supplier does: from other makers and the land, never from
the town's suppliers, who only bought it back (5,000–30,000 cord a day went round Ridgemere's chandlers and general
stores, against a few hundred used). A workshop selling a good it works with keeps back its own batches' need (`count ×
MaterialBatches` over its crafts), so two chandlers can't strip each other.

**Businesses that make nothing** (`makesThings`: no crafts, not a producer; a stall):

- their help's hours aren't credited as work in hand (they were all lost);
- they neither hire nor let help go;
- their working money counts no help.

**Fields and producers:**

- **A field's farmer with no work in its window** (all sown, all brought in, between windows nothing wanted) is free
  for a hire in its town, as an idle hand. The founding's fields are sown, so 52 farmers stood idle from day 2 to the
  harvest.
- **The harvest's-end plan is in the day's pass,** so it doesn't wait for the farmer to come to work.
- **A yield's hours are taken only when it brought something in.** A full store's hours stay in hand, and with the
  bank full they show as lost, and its hires end.
- **Tools wear by the hours the work took,** not those credited.

**Not done yet, on purpose:**

- **Prices at the seller's own wage** (`normalCost`, doc 63's `normal_cost`, this doc's Phase 4 plan): measure the
  going rate first. It reverses Phase 6's "prices follow it", so it needs the user.
- **The market stalls' street posts paid by the town** instead (about 670p a day from the capital's treasury): a
  design choice.
- **A field farm sized by its food goods, and its working money for a harvest's wages;** a slow staffing rate; a
  keeper's real hours in its hands.

**To watch:**

- **A farmer away on a hire when its window opens** holds the hire to its end (a week at most), and its fields wait:
  the farm's hands bank hours only the farmer's own work spends. Not seen in 30 days (the harvest is day 213).
- **A chandler keeps back 40 cord** (its candles' and nets' batches), so the makers that work with cord (jewellers,
  tinkers, the general stores' charms) get only what's over it. The realm's cord on hand fell from 470–1,500 to
  20–380; no shortage showed yet.
- **The going rate's new measure applies at the old pace too:** stalls out, and towns under 40 hands on the realm's.

**Measured** (30 days of DEV's export, calibrated hours, deterministic, against the same run before these changes;
`days.jsonl`'s label N is the midnight that ends day N-1):

| | before | after |
|---|---|---|
| hungry, starving: label 29 | 157, 81 | 17, 0 |
| label 30 | 174, 147 | 24, 0 |
| the first hungry spell | label 22 (95, 42) | label 29 (17, 0) |
| the pace: label 30 | 0.25 (backed off from 0.375) | 0.43 (0.5 at label 29) |
| Ridgemere's going rate | 12p throughout | 12p to label 25, then 12.13p |
| employers' wages against it: label 30 | 1.12×, 8.5% over 1.3× | 1.05×, 4.1% over |
| household Gini; the poorer half's share: label 30 | 0.569; 0.138 | 0.565; 0.141 |
| hours credited a day, days 15–29 | 4,290 | 5,073 |
| lost: all; businesses'; producers' | 35.5%; 50.9%; 19.6% | 33.7%; 43.4%; 24.2% |
| businesses' hands wanted, summed (held) | 625 (357) | 364 (358) |
| pay a day for hires: a hand, a hand at the shop | 684p, 263p | 1,859p, 834p |
| pay a day for chores (about the home) | 946p | 407p |
| cord bought a day | up to 31,800 | up to 540 |

- **Money conserved every day;** the digest 4bf2852fcc5ac2cd twice (before and after the cheap-checks pass).
- **What moved:** hands went to the businesses' hires instead of chores, the farmers off their sown fields, and the
  realm credited a fifth more hours; the hands wanted lost the loop, the byproducts and the crafts with no materials;
  the pace climbed to 0.5 before the first hunger, a week later than before and without starving.
- **What barely moved: the going rate.** The measure is unpinned, but few makers have a post open four days a week:
  hires are filled by the idle and the jobless at once, and no business hires into a full bank. Its raises need the
  pace higher, or more work wanted than there are hands. Producers now show a full store's hours as lost.
- **The tick** (both runs side by side, the machine shared): 0.110 ms before, 0.096 after (fewer hungry errands); the
  society's share 0.032 to 0.035.

**Tests:** `crafting_tests` `theGoingRateFromTheMakers` (a stall's wage not in it, the scribe's labourer's day is, the
realm's for every town, saved), `handsByWhatCanBeMade` (leather counted once; a bakery short of flour wants its keeper),
`theCordGoesRoundNoMore` (no cord from the general store; the other chandler keeps its batches'),
`aFullStoreLosesItsHours`, and `seasonalFields` (the farmer free while its fields are sown and between; the harvest's
end planned at night). All suites pass.

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
(As built and then reviewed twice: the review's mends are in these bullets, and "the second review, mended" below.)

- **Wants are the model's** (`wants`), with work by the hours: one a day per household, each lodger its own, out of its
  want fund, bought by the member whose turn it is. In the model's order:
  - a rapier for the rich (the top sixth by month's income, holding twice a rapier's going price) with none;
  - a sword for a household that works, with no blade (a guard's issued sword counts);
  - finery while it holds fewer pieces than it has members, two more for the rich, the dearest of the four;
  - all kept, and worn, mended and replaced as belongings (Phase 9).
- **Only a piece its fund runs to** (model.py's `wants`, line 1084): at the cheapest its town's shops sell it for, on
  their shelves or not. One none of them sells (a rapier: no shop makes one), or too dear, goes on to the next piece,
  and then to works. One it could pay for and finds on no shelf (none holding three), its sellers hear of: a sale
  missed at one of them by lot, counted with its sales at the turn of the day (the model's asks), so it makes or buys
  in more. It waits for that piece, no works meanwhile, `PieceWaitDays` (3) at most (`EconomyMemory::pieceWait`, the
  day its wait began, saved); then its works go on while it still looks, and tells its sellers, each day. (Waiting for
  any its town sold, whatever the fund, 4% of households waited a fortnight or more for a sword, one holding 117p, and
  never had works.)
- **And works on its home** (`worksOnHome`, doc 63's outlet) when it wants no piece it can buy, first: with a fund that
  covers a unit, up to half of it, by the amount. A unit is 11 hours of a building business's time (`WorksHours`) and a
  batch of its own building goods, at the town's hour and their cost, with the ladder's markup. The builders are the
  five building trades by name (`builds`: carpenters, sawmills, masons, the brick and plaster works), not the smiths,
  tinkers and potters who make a few building goods among their wares. Four by lot, the cheapest first, as a mend is
  booked: out of the hours it banks, while what's booked is under half a day of its hands' time. Its goods count as the
  builder's sales (`soldToday_`; a shelf the works emptied a sell-out, `ranOut_`), so its shelf's plan and its hands
  follow them. The hours count toward its hands (`worksRate_`, a month's running average beside `mendRate_` in
  `handsWanted`). What found no hours, a unit at most, the four hear of between them, a quarter of it, as a mend's.
  Lodgers' works are on their bunkhouse.
- **On a day with none of those, one thing it fancies,** by its own taste as before, used up: a drink, soap and scent, a
  pastime, something for the home, or a treat. The day's one want: never works and a fancy both. A treat is a dish,
  never a staple; never in a town with a wolf at 70 hunger (as the monitor counts them); only off a shelf holding its
  days' keeping of its sales.
- **Founded with what they own** (the model's): once (`EconomyMemory::piecesFounded`, saved), in a new world's first
  day pass, its purses still being founded: a sword for 70% of households that work (20% of those that don't), a rapier
  for 12%, a piece of its town's finery for half. 658 pieces on DEV's founding. Never into a world that has lived (an
  older save, or one first run at the old pace and turned to the hours later: the flag is set at either pace).
- **Children** (`childrenAndStipends`), with work by the hours: a stipend of at most 3p, a penny for every two weeks'
  worth over the household rule's target and the tax owed, out of the household's want fund (one rule). None goes to a
  child holding a fortnight's (42p). A child buys one thing a day, at half its purse and 3p at most: a bite when hungry
  with nothing on it, otherwise a trinket (jewellery, a hat, a scarf, paw wear, a toy; not a pipe). Trinkets are kept;
  past three, one is lost ("lost by a child").
- **The want fund is clamped every day**, with work by the hours, to what the household holds over its target
  (`householdRule`).
- **At the old pace, as it was** (the hours off, the game's default; the review's ask): a want for each grown wolf
  from its share of the fund, on what it fancies (a treat any food of 3p or more, in any town, off any shelf; finery
  and jewellery among its fancies, kept while its buyer holds six pieces or fewer, used past them), no piece wanted
  first, no works, nothing founded; children's stipends by two weeks' food spared and a penny more for every hundred,
  and up to half a child's purse on anything of 3p or less, kept; the fund clamped only on a day with nothing to spare.
  Bought one thing at a time (`buyOne`; the brief allowed it). Still at both paces: what a shop has on sale (`onSale`:
  never a maker's materials) and the founding flag.
- **Saved:** `piecesFounded`, the makers' mending and builders' works rates (`mendRate`, `worksRate`, from the day
  pass's end, loaded back on a restore), and the households' waits (`pieceWait`). An older save has none.

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

After the merge and both reviews (the same 30 days, `--threads 3`; days 15–29 a day): children's spending 373p and 166
goods, stipends 47p; wants 2,314p and 418 goods (1.5 swords, 7.5 pieces of finery); works on homes 621p and 75
building goods; on day 30 nobody hungry, household Gini 0.558, the poorer half 0.145, the pace 0.036.

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
- **Works or a fancy first** on a day nothing is wanted: works first (the model has works as the fund's outlet), a
  fancy only when no builder had the hours. Fancy first would almost stop works.
- **The wait for a piece** (`PieceWaitDays`, 3) is a placeholder: the model waits as long as the fund covers the price,
  and tells the makers each day. A fancy still goes on a waiting day (the model has none).
- **An older live save that turns the hours on later** is never founded pieces, so its households fill up by buying
  (about 900 pieces of finery in the first weeks). Founding them mid-life would make goods from nothing.
- **The building trades** are a fixed list by name; a new building trade would need adding to it.

### Phase 13: feeding the city (2026-10-25)

The first balancing pass found Ridgemere starving every week or so from day 21 (b0 and b1: 60 and 32 days from DEV
with the hours on). It has 683 of the realm's 1,083 wolves, 4 bakeries and 12 inns, and no mill or farm; its oats,
flour, vegetables, milk and fish come by caravan. This part is the supply side: the roads, the farms' stores and the
city makers' buffers. (Children's spending, wants, the wage and the hands are other parts.)

**The roads** (`RatwTrade.cpp`):

- **99 at a time.** A ledger entry holds 99 at most (`Society::transfer`), and a caravan bought one sale a quality from
  each holder, so Larkfield's fifteen farms sent the city 1,485 oats a week (15 x 99) whatever was ordered. Buying for
  the road, handing over a standing order, selling the rest on arrival and selling the unsold back to the stores now
  go on 99 at a time while there are goods, money and want.
- **Short of a good, a share each.** When a caravan holds less of a good than its orders want, each order has its
  share (`held x perWeek / all ordered`), then what is left in the order they were signed. In signing order alone, the
  supply shops signed first had the city's oats and its inns had none on day 54.
- **Orders agreed again** (`agreeAgain`) are raised only when the seller kept up: it sent some, and a quarter short at
  most of all it was to send since the last agreement (a share: a monthly order a little short each week was never
  raised), a week its road's caravan didn't go counting as a week short (it counted as kept up; not for an order signed
  that day, whose week begins). A shop with over two weeks of its use piled up has its order lowered, never raised (a
  smith's charcoal went from 18 a week to 126 so, its use being over the order). A shop ran low because
  the town couldn't fill its order, and its order rose. They rise toward a week and a quarter of the buyer's use
  (`ShelfMost` at most) where that is over the 90 a week they stopped at, under a city bakery's flour; but twice over
  at most at once, and by no more than the seller's town had to spare at the day's trade, over its own makers' wants and
  what the orders promise (`World::canSpare_`, not saved): a city potter's clay went from 23 a week to 1,133 at once,
  and Mirebridge sent none of it. A seller that sent under half has its order halved, and one that sent nothing in two
  weeks' worth loses it, so that another town may be signed (Southmeadow's flour stayed with Larkfield, which sent
  none); the buyer doesn't sign with the town that failed it for a month (`RoadsState::failed`, saved). What the buyer
  couldn't pay for, still on the wagon, isn't the seller's shortfall. A raise its seller's room held where it was is
  looked at again in a week, as a raise is; and one whose seller's room isn't known yet (no day's trade since the world
  was loaded) isn't agreed: the order stays due. The load is handed over by what each order was for as the wagon set out
  (`Caravan::loaded`, saved): a raise agreed while it was on the road isn't laid a shortfall the seller didn't make.
- **The week's caravans first.** The roads due today go first, in an order of the day's (a hash with the day); taken
  by name, with new orders' wagons counted among them, the twelve a day (`TradeCaravansADay`, unchanged) left
  Southmeadow's road to the city without its week's caravan on the days the cap was reached. Then a road with an order
  new today and no wagon out, with what the cap leaves, carrying only today's orders (`Caravan::orders`, saved), unless
  its week's caravan is due within `RoundTripDays` (2, a placeholder): then the whole load, and the road's week is
  served (`RoadsState::sentEarly`, saved): it doesn't go again on its day, and an order new on it meanwhile goes by
  itself. (In econ_watch, where a wagon is home the day it sets out, it went again: each order had two weeks' in two
  days, and its town's producers were told of a week's sales missed that they had made the day before.) Every new order
  sent a full week's load again, 28 trips from Larkfield to the city in 60 days, which hid the 99.
- **A load the trader can't pay for whole** goes as much of it as it can (it was all or nothing; Larkfield's load to
  the city had come to 18,652p).
- **No weekly roads for food that rots within the week** (`rotsOnTheRoad`, `RoadKeeps` 7: fish, rabbit, mutton, raw
  meat, bread, meals and porridge keep 1 to 5 days): no new orders, none in "Food between towns", and an order already
  signed ends when it is next agreed. The city's makers buy it from its own producers and makers, the producers told of
  what they missed (below). A faster cadence for those roads is left for later, if this isn't enough.
- **What rots** is one rule, `Society::rots` (keeps a number of days and feeds): spoilage, the share of the roads'
  orders a seller keeps (`shelfTarget`), and craft's keeping goods. Milk keeps a day but feeds nobody, so it never
  rotted; it had no road share, and Larkfield's dairies held about a day of their own sales. Now they keep their share.

**The city makers' buffer** (`Society::craft`, `inputStore`, the model's `lay_in`): a maker of food or flour keeps days
of each material's use, a week for a staple (bread, porridge: `StapleInputDays`), four days otherwise (`InputDays`),
no longer than a material that rots keeps less a day (fish, two days). It lays them in once a day, in a pass of its
own, whatever its shelves hold (it bought only for a craft whose shelf was low, about a day's worth); what is over its
batches' store only with what its till can spare, never on credit. A town's wants for the roads (`marketOf`) count the
same store.

**Sales missed reach the land** (`Society::missedAt`, the model's unmet asks): when a maker or supplier finds none left
to buy in its town (not short of money, nor left by a ledger entry's 99), what it went without of a day's use is
counted at the town's producers of the good as sales missed, once a day; so are the week's caravan's goods its town
couldn't spare (not what its money didn't stretch to at its holders' prices). A farm's sales a day, its next acres and
its hires follow. They are counted with the buyers who found none (`missedToday_`, as the pace's hungry are), not with
its sales: a producer that sold its last that day counts them or its sold-out half again, the more, not both. And only
when its town's sellers had no more as they keep back for themselves (a workshop's batches' store, as the buying does),
the day marked only when one is counted (a call short only of money marked it, and the want found later that day went
uncounted), and against all it bought of the good that day (a mill that laid in two days' wheat from one farm, calling
again for the rest, told the farm with an empty barn of fourteen sales it had missed).

**The fields** (`RatwFarmhands.cpp`):

- **Founded on a week of sales** (`FieldFoundDays`), not the first day's: its sales a day over its whole weeks of them
  (`foundingRate`; its sales and missed sales since its first, summed in `Society::spoil`, not saved), each weekday
  alike. (On the running average of the day, set right for the days it had run, a farm whose caravan buys once a week
  was founded on half to nearly twice its sales a day, by the weekday.) Until then a farm brings in as it always did.
- **Its month's rate** (`FieldYear::month`; the model's `daily_sales`): its yields a day wanted, into a running average
  over four weeks a week at a time (each week's mean of its days), set right for the weeks it has run; begun at its
  founding as if over the weeks it was founded on. Fed a day at a time, it still went a quarter over in the founding
  spring for a farm sold to once a week; a week at a time, within 2%. After a restart its running average of sales
  begins again from nothing (not saved), and a farm sold to once a week read its lump as a day's sales: its month went
  up to 1.29 times. Its days aren't folded in till that average has a week behind it (or, with no sales, the society
  has run a week), its week begun again.
- **Its founding spring:** founded before the roads' orders on it were known, it looks at its acres again once a week
  on a day of its own, at the day's pass (its farmer at work or away on a hire), by its month's rate (what is sown
  stays sown; its hands sow the rest; by the last few days' it ratcheted to the day after the caravan's, up to 2.3
  times its year), and a fortnight on (`FieldTopUpDays`) its store is topped up to the harvest, once ("a year's
  store"), by its month's or year's rate, the more; not till one is known (a field read from a save without them waits
  for its sales; it was marked done with nothing stored).
- **Planned by its food** (`grownToEat`, `plannedBy`): what feeds, or goes only into food or flour (a farm's wheat, oats
  and vegetables); its straw, hemp, flax, hay and firewood come with them, as hides with the model's sheep. A field that
  grows no food (an orchard's apples, for cider) is planned by all it grows. It was planned by the most sold of all
  nine goods. Splitting the farm by crop in crafts.json is the real fix, later.
- **Dry** when any of what it is planned by runs out, not only its first good (wheat held, its oats gone).
- **Next year** is planned from the year's rate if that is more than the month's (`FieldYear::rate`, a slow average
  over the year, daily in `tendFields`; the model's `max(year_sales / YEAR, daily_sales)`): a farm whose store ran dry
  stopped selling, its running average fell to nothing, and it planned no acres at all.
- **Saved:** `memory.fields` has six more numbers (the year's rate, the day founded, topped up; the month's rate, this
  week's so far, and its days). A save from before reads, its fields as founded long ago; one with nine, its month's
  rate begun with the next day's sales.

**Tests:**

- `crafting_tests`: `seasonalFields` (not on under a week of sales; founded on its first week's sales a day; the year
  saved, and six- and nine-number saves read; more acres sown as sales grow in the founding spring; topped up a
  fortnight on, once); `foodMakersLayIn` (a week of flour for a bakery selling sixty loaves a day, laid in with its shelf
  full, once a day, not on credit; a smith's bronze unchanged); `fieldsPlannedByTheirFood`; `missedSalesReachTheLand`
  (and a farm that sold its last and missed sales besides counts the more, not both); `missedOnlyWhenNoneLeft` (a mill
  short only of money misses no sale and the day isn't spent, its want later that day counted; an ironworks' batches'
  charcoal isn't for sale, and the smith's want is counted at the burner); `fieldsPlannedByTheirMonth` (seven farms
  sold seventy once a week, each on another weekday: all founded on ten a day, their spring's acres within a twentieth;
  a field with no rate yet isn't topped up till it has one).
- `roads_tests`: `caravansCarryWholeOrders` (over 198 bars bought from one smelter for two orders of 120, shared
  between them within one, each over 99), `noRoadOrdersForWhatRots`, `anOrderRisesNoFasterThanItsSeller` (a monthly
  order a quarter short raised, twice over and no more, the load handed over with nothing short; not raised with
  nothing to spare, nor with bars piled up), `anOrderItsSellerCantFillIsCut` (a week with no caravan a week short, but
  not for an order signed that day; a quarter sent, cut by half; nothing in two weeks, ended),
  `aWagonShortOfMoneyMissesNoSale`, `anEarlyCaravanIsTheWeeks` (the week's load
  early with a new order, not again on its day, each order its week once; saved through the checkpoint, and an order
  of 1,500 a week, which the checkpoint cut to 999). `anOrderCatchesUpWithUse` now expects twice over at most.
- Each of these fails with its fix undone (19 checked one at a time); the lay-in's cord-loop rule (`supplied(item) ||
  madeHere(item)`, as the batches' buying) changes nothing in today's catalog, where no food maker makes a material it
  works with that it doesn't also supply.

**Smoke run** (30 days from DEV's export with the hours, `--deterministic`, against b1; this part alone, the other
parts not landed):

| Day 30 (the turn of day 29) | b1 | This |
|---|---|---|
| Hungry / starving | 174 / 147 | 144 / 82 |
| Days with hunger | 21, 22, 24, 29, 30 | 11, 15, 16, 30 |
| Household Gini, the poorer half's share | 0.569, 0.138 | 0.576, 0.134 |
| The going wage, the pace | 12p, 0.25 | 12.65p, 0.18 |
| Work hours lost, that day and all run | 32%, 35% | 35%, 35% |

- Money conserved every day; no faults. The tick: 0.072 ms against the base's 0.070 run side by side (6 days).
- **Ridgemere's bakeries** lay in their flour: Grist & Crust went from the 5 to 8 it started with to 20 by day 5 and
  74 by day 11, its bread at 150 to 174 of an evening. The city went hungry on day 30 (122 of its people), not from
  day 22 (2% on day 15); the small towns had short spells on days 11 and 15-16 instead (Oakhaven, Southmeadow,
  Frosthollow: a fifth of their people at worst).
- **Ridgemere's oats orders** stay at 1,200-1,330 a week: they no longer rise when Larkfield falls short (b1's rose to
  3,052). A week brings what was ordered, once (b1's repeat trips brought 2,900-7,500 a week).
- **Southmeadow's bakery had no flour from day 3**: its two orders with Larkfield (26 a week) brought nothing after the
  first week, and an order its seller can't fill is never raised and never signed elsewhere. In b1 the repeat trips
  brought it some. Larkfield's mills grind too little for the city and Southmeadow both (the hands' part).
- **The farms' founding spring** re-planned their acres up (wheat and oats ordered by the city), and city hands went
  to sow them, lodging at the farms (Ridgemere 605-640 people on days 9-11 against 680): their credited hours doubled
  (444 to 813 a day), still 78% lost at full banks (hires into full banks, the hands' part).
- **Not this part's to fix, and in the way:** children still buy a bakery's flour and an inn's oats (a bigger store is
  more for them to take, as the review warned); a wage that barely floats.

**For the user:**

- **Milk** keeps a day but never rots (it feeds nobody, so spoilage skips it). This part keeps it so, and gives it its
  road share. If milk should sour, the road share should go and the city's own dairies make it.
- **Food that rots within a week** no longer goes by the weekly roads: fish, rabbit, mutton and raw meat, but also
  bread, meals and pies (they keep 3 to 4 days). The small towns had fresh fish, mutton and bread by road in b1. A
  faster cadence for short roads (every `keeps - 1` days) is the alternative.
- **`RoundTripDays`** (2) is a placeholder for how long a wagon is out.
- **An order its seller can't fill** is halved, and ended after two weeks' worth with nothing, so another town may be
  signed, and the one that failed it isn't for a month (the reviews' fixes). Sharing a seller's goods across its roads
  is still a choice.

**After the review** (2026-10-26; the fixes above are in the bullets): 30 days of DEV's export, `--deterministic --hours
--threads 3`, against the five parts merged (the same run before these fixes):

| Day 30 | Merged | With the fixes |
|---|---|---|
| Hungry / starving | 0 / 0 | 0 / 0 |
| Household Gini, the poorer half's share | 0.562, 0.141 | 0.563, 0.138 |
| The going wage (realm, Ridgemere), the pace | 12p, 12p; 0.107 | 12p, 12p; 0.089 |
| Work hours credited, lost (all run) | 154,889, 62,953 | 150,255, 65,397 |

- Money conserved every day; no faults; nobody hungry on any day in either. The tick: 0.046 ms against the merged
  binary's 0.048, run side by side for 8 days.
- **A week's load twice in two days:** 9 times in the merged run (Oakhaven's 7,759 to the city on days 11 and 12, and
  Larkfield's and Southmeadow's), none now. Big loads (over 500 ordered) went 12 times, not 23.
- **Orders:** the largest is 240 a week on day 30 (2,000 merged). 13 ended from day 27 (none merged): 12 whose sellers
  sent nothing in two weeks' worth (Frosthollow's iron for four city smiths; the city's salt fish, smoked rabbit and
  limestone for the towns; Coldwater's flour for Oakhaven), one cut below 2. An order more than twice over from one
  day to the next: 72 times merged, twice now (iron orders ended and signed again with Frosthollow, for more).
- The pace stepped up a little later (0.089 on day 30, against 0.107).

### Phase 13: the pace, steadied (2026-10-25)

**The trouble.** The realm's pace (Phase 6) went on a 56th a day until the realm was already hungry. Its one signal,
2% of the realm hungry, came after the shelves and larders were bare, so it climbed into a famine, backed off, and
climbed again from empty shelves: in the run before (30 days from DEV with the hours on), 95 hungry on day 22, 141 on
day 24, 157 to 346 from day 29.

**The pace now** (`tendLabour`), with work by the hours:

- **Its reading:** each day pass, the realm's food in hand from the monitor (its homes' and its shelves', days a head,
  each town's at most 4 days, `PaceCoverCap`, so a glutted town doesn't hide a hungry one), measured the evening before
  for that day; none if the monitor missed the evening. A week of them is kept (`memory.paceCover`, 8, saved).
- **Back a 14th a day** while 2% of the realm is hungry, as before.
- **Back a 56th** while the week's food fell by over 0.15 days a head (`PaceSlide`: today against the same day a week
  ago, so Restday is at both ends or neither) or averaged under a day (`PaceFloor`).
- **On a 56th** only while the week's food didn't fall and **the land's hands keep up**: its producers at work and
  their hires taken, against the hands their sales want (`handsWanted`), short by 5% at most (`PaceShortHands`); or no
  further behind than a week ago by more than 5 points (`memory.paceHands`, saved). (Since 2026-10-26: the week's
  food "didn't fall" within `PaceNoise`, 0.05 days a head; and the land's hands no longer hold it. See "money by holder,
  and the wage bid".)
- **Otherwise it holds.** Until it has a week of readings it only holds or backs off.
- **Why the land's hands by their week:** their sales want two to three times the hands they have even at the old pace
  (day 10: 963 wanted, 449 had). Buyers lay in stores, a producer that sells out counts its day half again, and a bare
  shelf plans half again more. Held to the 5% alone it never went on (a first run, stopped at day 15, had the land's
  hands at 36% to 92% of those wanted), and a pace held at 0 keeps the households' gap open.
- **Saves:** both lists are read only when they hold a week's readings at most, each in bounds; an older save has none,
  and the pace waits a week.

**The monitor** (`TownReading.homeFoodDays`): each town's homes' food (larders and pockets), days a head, kept in the
brief (0 in an older one). Its shops' food counts their meals now, at the store price (they were skipped). Drinks were
left out already.

**The food buffers** (at every pace):

- **Shelves over Restday** (`shelfTarget`): on the eve of a day the shops aren't properly open, a food is kept for that
  day too, as far as it keeps (`keeps` − 1): bread 2 to 3 days, a meal 1.5 to 2, porridge as it was.
- **The larder errand** (`householdShopping`): each household starts at a shop of its own (by its home's lot), and goes
  on to the next while what it holds is still under the point it lays in at (`buyFood`'s `under`). The plan for a
  weekday is as it was: the household lays in by what it eats from home.
- **A missed meal** (`missedToday_`): a hungry buyer who finds nothing left to eat at a shop counts a meal of its first
  food as wanted, into its sales rate with its sales (the more of that and a sold-out day's half again).
- **The founding larder** holds two days of meals (`FoundingLarderDays`), not 2 to 5: meals keep three days, and 207k of
  the founding's 253k nourishment spoiled on day 5. The first trips no longer spread over days 2 to 5; each household's
  own first shop spreads them over the town's shops instead.

**The land's food on Restday** (`decideAuthored`'s Restday rule; **waiting on the user's OK**): the producers of food or
milk (fishers, eel traps, rabbit and sheep farms, dairies, pastures) work Restday after the service, as the food shops
do. It widens the user's 2026-10-07 rule (food makers work Restday after the service) to the land's food. Their hires
already worked Restday.

**Measured** (30 days from DEV, `--deterministic --hours --threads 3`), against the run before:

| day 30 | before | now |
|---|---|---|
| hungry, starving | 174, 147 | 0, 0 |
| days with anyone hungry | 5 (from day 21) | none |
| pace | 0.25 (0.375 at its height) | 0.107 |
| household Gini | 0.569 | 0.561 |
| the poorer half | 0.138 | 0.147 |
| the going wage | 12p | 12p |
| hours lost to full banks (the day's) | 32% | 41% |
| food brought in on a Restday | 0 | 15k to 31k nourishment |

- Money conserved every day. "The run before" is the commit before (its run matches the lead's baseline day for day).
- **The tick:** 0.0735 ms over the 30 days, with six other runs on the machine. Ten days side by side with the commit
  before: 0.063 ms against 0.079, the society's part the same (0.0289 against 0.0286), fewer hungry walking to the shops.
- **The pace:** 0 to day 15, then on at 6 of the 15 day passes after (0.107 by day 27), and a step back at the last.
  From a traced run (the same pace day by day): the founding's meals spoil and the week's food falls to day 12, and the
  land's hands fall from 85% to 36% of those wanted as the first hires end; of the 8 holds, 6 were the week's food
  falling a little (under 0.06 days a head) and 2 the land's hands; the last pass's food fell 0.53.
- **The food in hand** (`larders.csv`, homes and shops, a town's at most 4 days, by people): 3.0 to 3.6 days a head from
  day 7, against 0.9 to 2.7 before.
- **Still to do:** the land's hires go untaken (in the traced run two thirds of their slots stood open most days from
  day 9; day 20, 224 of 312) and the wage stays at 12p, so the pace holds low and the hours of the full banks stay
  idle. That is the labour market's to mend; the pace follows as the land's hands come.

**Tests:**

- `orchestrator_tests`: the homes' food (five days a head in Wellby, none in Hungerford), and none in an older brief.
- `wire_tests`: both lists round trip; too many readings, or one out of bounds, are dropped, the society still read.
- `roads_tests` `thePaceIsSteadied`: the first reading is the monitor's homes and shelves, and the pace holds; a week in
  it steps back when the food fell, holds when it didn't (the strip has no producers), and steps back a 14th while the
  realm goes hungry.
- `crafting_tests` `foodKeptOverRestday`: more bread kept on the eve, firewood's store the same. `seasonalFields` feeds
  its farmer before the harvest's end: its body can't walk to the bakery, and with the pace no longer rising by itself
  there was bread on the shelf it stood waiting to buy.
- `schedules_tests` `restdayLandFood`: Greyfen with Rook keeping rabbits rests through the service and works after it.
- All suites pass.

### Phase 13: the labour market and the pace, reviewed (2026-10-26)

What the reviews of the two parts above found, mended.

**The land's hands:**

- **A field's farmer is called back** from another business's hire at the day's pass once its own window has work
  (more acres to sow, the harvest open): the farm's hands worked into a bank only its farmer's own work draws on, and
  were paid for hours that were lost.
- **A field's hands' wages are kept in its working money** (`fieldWages`): from a month before its window opens to its
  end, the window's work still ahead (what's left to sow, or what's sown to bring in) less a day's hours a day of the
  farmer's own, in hands' days at its wage and the harvest's pay. Its owner's monthly take no longer leaves the harvest
  unpaid; a farm that still can't pay ends its hires, as any business does. (The founding's fields, sown as founded,
  want none at their sowing.)
- **A hire ended early** (its business couldn't pay a hand's share, or had no work for it) **is shut at once**
  (`OddJob::ended`): its other holders don't work the day out for nothing, nor do the jobless take it, ranked by the
  pay it didn't pay; it goes with the day's odd jobs. (On day 0 an ended hire no longer turns into a day's odd job.)
- **A business with no work for more hands** (`idleBusiness`, and a hand's `idleHand`) is one whose work in hand was
  full at the last turn of the day and is still: not a moment's full bank, a hire's day of hours come in after its
  keeper's last look. (A restart still finds none idle for a day: work in hand isn't saved.)

**What a workshop keeps back** (`keepsBack`): of what its own crafts work with, two batches of each (`MaterialsLow`,
the point it would buy more at), not eight; and of what rots, only its shelf's few. A butcher held back 24 raw meat
that spoiled, a plasterworks 48 lime; a chandler keeps 10 cord, not 40.

**Tools wear by real hours:** what goes out of the work in hand, its hands' with the farmer's (they work with its
tools), back to real hours on improved premises (an improved hour in hand took less than an hour).

**The pace:**

- **The land's hands by their sales as they are:** a producer's want without the half again of a shelf near bare
  (`handsWanted(till, false)`): its goods go on to the shops, so its shelf always is, and the level test never passed.
  A producer with no work for more hands counts at the hands it has. A realm with no land keeps up.
- **The week's trend is gone** (`memory.paceHands`, no longer saved or read): a fill share stuck low, or sliding a
  little each week, passed it.
- **Each reading carries its day** (`memory.paceCover`: day and reading); the week is today's against the reading
  exactly a week ago, and its mean what was read between. A day the monitor missed leaves a gap; a server away for
  days finds no week to weigh, not a reading from weeks ago. An older save's bare readings read as none.

**Missed meals, where they happen** (`missedMeal`), with work by the hours: a wolf at 60 hunger or more, nothing to eat
about it or at home, whose town has food shops open but none with food left, counts the meal it wanted (of the first
food one of them, by lot, makes or supplies: `foodMade`, not the meal every food shop sells, which at a brewery nobody
makes) into that shop's sales at the turn of the day; once a day a wolf, at most a day's real need. A buyer at a shop
it finds bare counts so too. Before, every errand went only to shops with food, so a shelf bare all day was never
visited and nothing was counted.

**At the seconds' pace, as it was** (with work by the hours only): the best-paid hire first, a stall's help crediting
no work and its working money counting no help, a full store's hours kept in hand, an unpaid hire ended, a post open
while its holder is away on a hire, tools worn by the hours drawn, the going rate's new measure (at the old pace, its
towns' unskilled shops by their hands, as before), and missed meals (the second review). The cord loop's mend and the
keep-back stay at both paces.

**Not mended here, for the user:** the founding larder's two days for every household (its 2-to-5-day spread of first
trips gone), the land's food on Restday, and the monitor's shop food counting meals.

**Measured** (30 days of DEV's export, `--deterministic --hours --threads 3`), day 30 against the merged Phase 13
before these mends:

| day 30 | before | now |
|---|---|---|
| hungry, starving | 0, 0 | 0, 0 (1, 0 at the run's end) |
| household Gini | 0.562 | 0.565 |
| the poorer half | 0.141 | 0.139 |
| the going wage: the realm's, Ridgemere's | 12p, 12p | 12p, 12p |
| pace | 0.107 (on from day 17) | 0.036 (on at days 14 and 15, then held) |
| employers' wages against the rate | 1.043×, 3.4% over 1.3× | 1.052×, 3.2% over |
| hours credited to day 30; lost | 154,889; 40.6% | 161,305; 42.4% |
| hours lost, day 30's | 47.4% | 52.9% |

- **Money conserved every day;** digest 83f19ae007d0f350 (a traced copy repeated it). The tick 0.055 ms on three
  threads with the machine shared, against 0.052 on six.
- **What holds the pace now is the land's hands,** as it should: from a traced copy, their want without the bump was
  540 to 850 hands a day against 520 to 780 had; from day 16 they had 72% to 88% of it, and the one day they kept up
  (day 30) the week's food had slid. The week's food moved little (−0.13 to +0.33 days a head, averaging 3.4 to 3.6),
  so before these mends the week's trend let the pace on while the land fell behind. It goes on as the land's hires
  are taken: the labour market's to mend (the going wage still 12p).

**Tests:**

- `crafting_tests`: `missedMealsCount` (a bakery sold out, nothing in town to eat: the meal a hungry wolf wanted is in
  the bakery's sales at the turn of the day, once), `theErrandGoesOn` (half of the first bakery's bread isn't enough:
  the household goes on to the second), `whatAWorkshopKeepsBack` (two batches of cord; no store of raw meat),
  `toolsWornByRealHours` (an improved miner's pick by its real hours), `theHarvestsWagesKept` (a month before the
  harvest the farm's working money holds its hands' wages; none between), `farmersCalledBackAndHiresShut` (the farmer
  called back as its acres grow; an unpaid hire shut, its other hand not working the day out); `aFullStoreLosesItsHours`
  (not idle the day its bank first fills, idle at the next turn); `theGoingRateFromTheMakers` with work by the hours;
  `theCordGoesRoundNoMore` against the new keep-back.
- `roads_tests`: `thePaceIsSteadied` (with no land it goes on; a week whose first day was missed holds; a day missed
  between doesn't; a fortnight-old week is dropped); `thePaceStepsUpWithTheLand` (a rabbit farm selling a hand's day,
  its shelf bare: the pace goes on, though the farm plans for two; selling three hands' days: it holds).
- `wire_tests`: the readings round trip with their days; out of order, out of bounds, too many or a bare older save's
  read as none.
- Each fails with its mend undone. All suites pass.

### Phase 13: the second review, mended (2026-10-26)

What the second round of reviews (of the three parts' mends, merged at e0e6f45) found, mended. The players' harness is
another part's; what is the user's to choose is at the end.

**The land:**

- **A field's founding spring is looked at in the day's pass** (`tendLabour`, `reviewField`): every field in its
  founding year's sowing, its holder's or (its post empty) its founder's, on its own day of the week; no longer in its
  farmer's work (`produce`). Founded sown, its farmer took 7-day hires, its look ran only in its own work, so its acres
  never grew and the recall never came. In the smoke two farmers were called back; the rest of the founding's farmers
  (84 to 102 a day on hires from day 9, against 94 to 102) have nothing more to sow: their month's rate is about what
  they were founded on.
- **A field's month through a restart** (`tendFields`): its days aren't folded in till its running average of sales
  has a week behind it again (or, with none, the society has run a week, `fieldPasses_`), its week begun again. A farm
  sold to once a week, rebuilt from its save on day 40, read 0.80 to 1.29 times its sales; now within a twentieth.
- **Sales missed against the day's buying** (`buyMaterials`; `unmetOn_` keeps what each till bought of a good that day):
  what it went without is the least of all it sought that day and a day's use, less all it bought. A mill using 39
  wheat a day that bought 99 from one farm, calling again, told the other (its barn empty) of fourteen sales it missed.
- **A producer's help is free for a hire as its business is** (`idleHand` -> `idleBusiness`: its field's work done, or
  its holder's work in hand full). Its hours go to the till's own bank, which nothing draws on (latent in DEV's run, as
  the review found).
- **What a workshop keeps back** in the sales-missed test was already its batches' (`keepsBack`, mended in the merge);
  a foundry's charcoal (six, over the four a shelf keeps) now guards it.

**What households buy** (with work by the hours): only a piece its fund runs to at the cheapest its town sells it for;
one none sells, or too dear, goes on to the next and then to works; one it could pay for and found on no shelf, its
sellers hear of (a sale missed), and it waits for it three days at most (`PieceWaitDays`, `EconomyMemory::pieceWait`,
saved; a household gone is dropped). The rest are in "what the wolves buy" above. `madeFrom` (a food shop's own
materials, not on sale) is worked out once a business.

**Missed meals** with work by the hours only, and only at a shop that makes or supplies a food (`foodMade`): not a
brewery's meal (every food shop is listed as selling meals), which reached no maker.

**The old pace, as it was** (the hours off, the game's default): each grown wolf's want from its share of the fund
(`wantsAtTheOldPace`), its treats any food of 3p or more anywhere, finery and jewellery among its fancies; the old
stipends and children's spending; the fund clamped only on a day with nothing to spare. One thing at a time, as the
brief allowed. Missed meals too (above).

**The roads** (`RatwTrade.cpp`):

- **A raise its seller's room held where it was** is looked at again in a week, as a raise is (it waited a month while
  the shop ran low).
- **A seller's room not known** (`World::canSpareKnown_`: no day's trade since the world was loaded, and cleared by
  `World::restore`): a raise isn't agreed, the order stays due. (A restart read none; an in-process restore the last
  timeline's.)
- **A seller that failed an order** (cut or ended for what it didn't send) isn't signed with that buyer again for
  `MonthDays` (`RoadsState::failed`, saved with the roads): Frosthollow's iron orders were ended and signed again the
  next day, for more.
- **What the buyer couldn't pay for**, still on the wagon, isn't its seller's shortfall. (So made, nothing counted it,
  and an order whose buyer never paid never ended: mended in "the roads and the walking, reviewed" below, with
  `StandingOrder::unpaid`.)
- **A load is for the orders it set out with** (`Caravan::loaded`, each order's week at departure, saved): a raise a
  porter agreed while the wagon was out isn't laid a shortfall. An order signed after it set out has none of that load.
- **Halved:** the cut's "toward what it sent" never applied (under half sent, its floor of half always won); the code
  and the text say halved.

**Saved:** the households' waits (`memory.pieceWait`), each caravan's `loaded`, the roads' `failed`; bounds-checked, and
an older save has none.

**Measured** (30 days of DEV's export, `--deterministic --hours --threads 3`), against the merged Phase 13 (m2, the same
run from e0e6f45):

| | merged | now |
|---|---|---|
| day 30: hungry, starving | 0, 0 | 0, 0 (3, 0 at the run's end) |
| household Gini; the poorer half | 0.561; 0.140 | 0.558; 0.145 |
| the going wage (realm, Ridgemere); the pace | 12p, 12p; 0.071 | 12p, 12p; 0.036 |
| hours credited, lost (all run) | 153,429; 68,661 | 155,116; 69,692 |
| days 15-29 a day: the owners' profit | 14,549p | 14,184p |
| the keepers' wages; hands' odd jobs; service wages | 4,378p; 2,613p; 1,448p | 4,351p; 2,592p; 1,440p |
| wants (swords, finery) | 2,257p, 414 goods (0.9, 5.9) | 2,314p, 418 goods (1.5, 7.5) |
| works on homes | 586p, 70 goods | 621p, 75 goods |
| children's spending; stipends | 380p, 168 goods; 49p | 373p, 166 goods; 47p |

- Money conserved every day; no faults; digest edc08328cb6106f4, the same on one thread and three (3 days). The tick:
  0.0647 ms against the merged binary's 0.0649, six days side by side (the society 0.0309 against 0.0306).
- **The end's three hungry** are asleep at 70 to 71 (a Frosthollow watchman, an Icegrip miner in its bunkhouse, an
  Oakhaven wolf); Frosthollow has no food shop (the same in the merged run). Nobody hungry at any day's end.
- **The pace** stepped up on day 15 in both, then held till day 29 (the merged run went on again on days 16, 29 and
  30). Not traced.
- **More pieces** are bought (swords 1.5 a day against 0.9): makers hear of the pieces wanted and make them.

**Tests:**

- `crafting_tests`: `farmersCalledBackAndHiresShut` (its sales grown, the day after the hire not its day: acres as they
  were; on its day, twice as many, the farmer called back); `missedAgainstTheDaysBuying` (99 and none: the empty farm
  told of nothing; 10 and none: of its share of the thirty); `aRestartKeepsTheMonth` (seven weekdays, rebuilt on day 40:
  within a twentieth); `aFarmsHelpFreeAsItsFields`; `aHouseholdsWants` (the wait saved, the store's missed sale);
  `aPieceWaitedForAFewDays` (three days' wait, then works while it looks; saved, and an older save has none; over when
  found; a household gone dropped); `piecesItCantGetGoOnToWorks` (no sword sold; an armourer's swords past the fund);
  `worksSellOutTheShelf`; `theWantFundClamped` (to what's held over its target, exactly; not at the old pace);
  `noTreatsInAHungryTown` (70 and 69; at the old pace a treat either way); `missedMealsCount` (none at a brewery, none
  at the old pace); `missedMealsWhereFoodIsMade` (a bakery and a brewery bare: four meals at the bakery; two at a
  counter with one loaf, the second's meal counted, and at the old pace only the loaf); `missedOnlyWhenNoneLeft` (a
  foundry's charcoal); `theHarvestsWagesKept` (the year's last month and the sowing; a field founded sown, none);
  `wantsAtTheOldPace` (each grown wolf its own want).
- `roads_tests`: `anOrderRisesNoFasterThanItsSeller` (with nothing to spare, looked at again in a week; piled up, in a
  month); `anOrderItsSellerCantFillIsCut` (halved; Frosthollow signs the far forge's, never the two it failed; saved);
  `anOrderWaitsForItsSellersRoom`; `whatTheBuyerCantPayForIsntShort`; `aLoadIsForTheOrdersItLeftWith` (saved through
  the checkpoint); `thePaceStepsUpWithTheLand` (its store full and its work in hand full at two turns, its sales three
  hands': the pace goes on); `childrensStipends` (with the hours; at the old pace, a penny more for every hundred).
- `wire_tests`: the waits round trip, a day that can't be dropped, an older save's none.
- Each fails with its mend undone (29 checked one at a time); `madeFrom`'s cache changes nothing to test. All suites
  pass (pg_tests skip without a database).

**For the user:**

- **The wait for a piece** is three days (a placeholder); its sellers hear of one missed sale a household a day (the
  model's asks are a twentieth of a piece). A fancy still goes on a waiting day; the model would keep the fund.
- **The want fund's clamp** is with the hours only now, so the old pace is as it was; it was a mend (the fund counted
  the same money again each day), and could be at both paces.
- **A failed seller's month** (`MonthDays`) is a placeholder; a buyer with only that town to buy from signs nothing
  meanwhile. The roads' rules (this one, the cut, what a buyer couldn't pay) are at both paces, as all of the roads'
  are.
- **A producer's help** works into a bank nobody draws on (before this part): it does no work for the farm.
- **Left as they were:** the founding larder's two days for every household, the land's food on Restday, the monitor's
  shop meals at store price, `RoundTripDays`, the roads for food that rots, milk, a seller sharing its goods across its
  roads, no top-up for a farm founded after day 78, the keep-back of the common sort only, and the wage barely floating.

### Phase 13: the price level (2026-10-26)

**The question.** The user's decision 1: found with 5,000,000p in whole pennies, a labourer's day about 60p. The realm
was founded with that money, but its going rate began at the catalogue's 12p (`goingRate`'s fallback) and never left
it; every amount written in pennies when a day was 12p stayed so; and goods were costed at their calibrated hours
whatever the pace. Three experiments (30 days from DEV: the founding wage, costing at the pace, both) tried them; this part builds them.

**Prices at the hours worked** (doc 63's `normal_cost`): with work by the hours, a good's normal cost (a producer's
yield and its tools' wear, a craft's batch) and the cost a batch is made at take the hours it takes now, at the realm's
pace (`Society::costedHours`, `hoursFor`). With the hours off, the calibrated `hours`, as before. (Since `3cec19a`
only with `ladder.json`'s `costAtPace` on, and it is off: goods are costed at their calibrated hours at either pace
until the user decides, below.)

**The founding wage** (`Society::foundWage`, `RatwFounding.cpp`). `ladder.json` has `foundingDay` (60): in a land of
`WorldMoneyResidents` or more, whose money is made up to `WorldMoney`, the realm's going rate (`memory.going[""]`, and
every town's there) starts there, once, before its purses are made: in `resetAuthored` before the tills are founded
(their floats), and in `foundPurses` for a save that never founded them. So the first wages, employers' books, floats
and prices start at 60p. A village made for a test keeps the catalogue's day (12). A realm whose measure finds no maker
on a day keeps the rate it had (`tendLabour`; it fell back to the catalogue's 12p). (The review, below: the realm
remembers the day it was founded at, `memory.foundedDay`; only a realm so founded keeps its rate, a village falls back
as it did; and an older save founding its purses now has its employers' books moved with the rate.)

**Where `catalogueDay` is read:** as the unit the catalogue's prices are in (an item with no place on the ladder is
worth `price / catalogueDay` LD: `worthOf`, `item_catalog.py`, `ladder.py`), and now `atGoingRate` (a catalogue price
at the going rate); as a wage only before the first measure, in a village.

**The 12p amounts, in labourer's days** (`Society::pennies(days, town)`: whole pennies at the town's going rate, ""
the realm's, a penny at least; each written as its 12p amount over 12, so at 12p each is what it was). (The review,
below: at the town's day for amounts, `amountDay`, which by the hours is its going rate and at the seconds' pace the
realm's founding day, because that pace's measure keeps the skill's premium in and three towns measured 12.8p; caps at
the realm's day only. In a realm founded at 12p each is what it was at either pace.)

| Amount | At 12p | At 60p |
|---|---|---|
| a wolf's food a day (`FoodADay`): a household's need (its relief, the rule's target, comfortable or poor), a month's living (the wealth line, a keeper's at the founding), a week's kept back from a want or a stall's buyer (eight days' from a piece), the mint's spared purses, talk's "on the dole", the troubles' food cost before the monitor's | 5p | 25p |
| a keeper's reserve (`KeeperReserve`) | 20p | 100p |
| a child's stipend at most (`MostStipend`), its day's spending at most, its least purse to spend | 3p, 3p, 2p | 15p, 15p, 10p |
| a treat at most; a want's least budget, a town buyer's | 9p; 2p | 45p; 10p |
| a float (`floatOf`): the manager's wage, a help's, materials, and its first craft's store at catalogue prices; a House's manager's keep | 8p, 6p, 30p | 40p, 30p, 150p |
| a House's floor (`HouseFloor`), upkeep a level a week (`UpkeepALevel`) | 100p, 5p | 500p, 25p |
| a wolf's purse weighed: the price of a bite, poor (under a day's wage: odd jobs from eight, a child's every day, the town's poor), to lay in the larder, alms (under 6p) | 4p, 12p, 5p, 6p | 20p, 60p, 25p, 30p |
| the plate: 1p over 20p, 2p over 60p; a beggar's penny from those over 100p | | 5p over 100p, 10p over 300p; 5p over 500p |
| wholesale meals and herbs from the stores; a treasury's payroll reserve | 4p, 1p; 2p a spell | 20p, 5p; 10p |
| the town's buyers' baskets, provisions for the road (catalogue prices) | | at the going rate |
| meals, herbs and swords (`storePriced`: shelves, the going price, a player's quote) | 6p, 2p, 30p | 30p, 10p, 150p |
| the dearest piece the ledger moves (`PieceMost`), a story's prize at most, a stall's price at most | 1,000p, 1,000p, 999p | 5,000p, 5,000p, 4,995p |
| a player's welcome grant; a stall's rent; a Gift's help (paid by the till) | 20p; 3p; 4p | 100p; 15p; 20p |
| the watch: a fine for an assault, a theft; a meal's, herbs', a sword's worth; penniless (crime), poor (the church's care) | 8p, 3p; 6p, 2p, 40p; 6p, 20p | 40p, 15p; 30p, 10p, 200p; 30p, 100p |
| caravan guards; the towns' contracts: a bounty (20p and 3p a point of strength), an escort, a supply run; a letter (10p to write, 3p to 7p for it) | 6p; 20p+, 12p, 15p; 10p, 3–7p | 30p; 100p+, 60p, 75p; 50p, 15–35p |
| a Hold's draw (a purse under 30p); a child's first pennies at its birth | 30p; 5p | 150p; 25p |

**Kept as they are:**

- **The founding's seeds:** the starting purses (`ChildPurse` to `KeeperPurse`), `TreasuryHead`, `ChurchHead`,
  `HouseFortune`. In a founded realm `worldMoney` makes them up, each kind to its share, so the opening spread is the
  model's at either day; at 60p, even purses five times over would flatten it.
- **The catalogue's own units:** a shop's cheap goods (`CheapPrice`, `goodsSold`), a trinket (3p at most on the
  catalogue), a treat (3p at least).
- **Counts:** days (`PoorDays`, `ComfortDays`, `HomeNeedDays`, a fortnight's stipends), batches, months of income.
- **`camp::HoldWage`** (5p): a floor under the going rate that binds only under a 5p day.
- **The demo roster's** (`Roster::Demo`), the DM's own coins and the sanity bounds far above any price.
- **Players' services, for the user** (scaled since the review, below; content prices, some in labels and the client): lodgings (2p a night, 10p a
  week, 6p a lodger's week, 8p the upstairs), estates' rents, couriers' fees, a notice (1p), a Chapter's founding (20p),
  a festival's entry (1p) and prizes, the tavern's stakes (5p), a faction's report (5p) and tithe (20p), the troubles'
  offers, a favour's lost loan (at its catalogue price) and its courier (1p), a letter's enclosure (50p).
- **Removed, unused since Phases 6 and 7:** `HireRaise`, `TaxBand`, `DoleADay`, `PovertyLine`, `OwnerWage`,
  `LeanTreasury` (`treasuryLean`), and `OddJobPay` with `businessSpends`.

**Saves:** nothing new is saved (`memory.going` was; `foundingDay` is data). An old save loads with its rate: DEV,
founded at 12p, stays there until it is founded afresh (`--refound-economy`). (Since the review, the day it was
founded at is saved too, `memory.foundedDay`; an older save has none, the catalogue's 12p.)

**Measured** (30 days of DEV's export, `--deterministic --hours --threads 3`, five runs side by side; the middle three
from a build with a part switched off by `RATW_TRIAL`, not kept):

| day 30 | base (e0e6f45) | 60p alone | 60p, priced at the pace | 60p and the amounts | all three |
|---|---|---|---|---|---|
| household Gini | 0.561 | 0.539 | 0.561 | 0.554 | 0.562 |
| the poorer half | 0.140 | 0.154 | 0.138 | 0.149 | 0.141 |
| hungry, starving | 0, 0 | 2, 2 (from day 23) | 0, 0 | 0, 0 (1, 0 at the end) | 0, 0 (1, 0 at the end) |
| the going wage: the realm's, Ridgemere's | 12p, 12p | 60p, 60p | 60p, 59.8p | 60p, 60p | 60p, 59.5p |
| pace | 0.071 | 0.107 | 0.054 | 0.071 | 0.018 |
| hours credited; lost | 153,429; 68,661 | 166,256; 78,650 | 161,160; 73,202 | 168,513; 81,506 | 162,590; 76,374 |
| bread, a bronze sword, a rapier (LD) | 0.081, 2.5, 25 | 0.071, 0.5, 25 | 0.028, 0.5, 25 | 0.076, 2.5, 25 | 0.027, 2.5, 25 |
| an iron, a steel sword (LD) | 5.1, 9.8 | 5.1, 10.2 | 1.5, 1.1 | 5.1, 10.1 | 1.4, 1.8 |

A day, days 15–29 (pennies; LD in brackets):

| | base | 60p alone | 60p, at the pace | 60p and the amounts | all three |
|---|---|---|---|---|---|
| the owner's profit | 14,549 (1,212) | 13,293 (222) | 5,404 (90) | 13,889 (231) | 6,767 (114) |
| an owner's investment | 1,431 | 15,748 | 21,432 | 15,894 | 22,424 |
| the keeper's wage | 4,378 | 20,217 | 19,953 | 20,431 | 20,059 |
| an odd job: a hand | 2,613 | 14,494 | 11,470 | 14,970 | 10,364 |
| service wages | 1,448 | 7,963 | 7,459 | 7,861 | 7,783 |
| wages, six kinds (with the shop's, the home's and the watch's hands) | 9,740 (812) | 46,179 (770) | 42,756 (714) | 46,859 (781) | 43,195 (726) |
| the owner's profit against them | 1.49 | 0.29 | 0.13 | 0.30 | 0.16 |
| farms' takings; their tills' cash (working money) | 7,090; 137k (86k) | 29,712; 365k (444k) | 13,267; 85k (435k) | 30,810; 341k (453k) | 12,362; 83k (426k) |
| farms' hire slots, taken | 447, 352 | 429, 371 | 379, 326 | 436, 385 | 312, 295 |

- **Money conserved every day in every run;** the base's digest aded872975ff165c is the merged run's (`m2`) day for
  day; all three's 99fabb73f1c82454. The tick: 0.106 ms in each, side by side (the society's part 0.058).
- **The founding:** the first day's money before it is made up is 430,688p (169,890p before: the tills' floats at
  60p), and the day's wages 21,100p (4,086p).
- **60p alone** is what `x1` found: the owners' profit falls from one and a half times the wages to under a third,
  money stays in the businesses' tills (their working money is a month of 60p wages: a fifth of all money is in tills,
  against a fourteenth), and the household Gini falls 0.022. But the 12p amounts are a fifth of what they meant: a
  household's food money, its relief and its rule's target; a child with a fortnight's 3p has its stipend stopped (7p a
  day, against 49p); store meals and swords at their 12p prices (a bronze sword 0.5 LD); and the realm goes a little
  hungry from day 23.
- **The amounts** put those back (the children's stipends 236p a day, 3.9 LD against the base's 4.1; relief 73p; a
  bronze sword 2.5 LD) and keep the realm fed, for some of the Gini's fall (0.554, the poorer half 0.149).
- **Pricing at the pace makes it worse, with today's labour market.** At a pace of 0.02 to 0.11 a good's labour is
  costed at a small share of its calibrated hours (a loaf's at 4% to 12%), but its hands are paid by the day, and
  nearly half their hours are lost to full banks: what a business sells no longer pays its wages. The land sells at
  less than half (farms' takings 12,400p a day against 30,800p), the farms' tills fall to a fifth of their working
  money, owners put 22,000p a day into their businesses (15,900p without it), and the farms hire fewer hands (312
  slots against 436). With the land's hands behind, the pace stalls (0.018 at day 30), and
  more hours are lost. Prices fall out of the ladder's bands (bread 0.027 LD, against 0.05–0.09; a steel sword below a
  store-priced bronze one), and the Gini goes back to the base's.
- **So** (corrected by the review, 2026-10-26): the founding wage and the amounts are *not* better than the base on
  every measure. Of the bands, the bronze sword's 2.5 LD and the rapier's 25 are so by construction (the sword is
  store-priced, its catalogue price at the day; no rapier sold, so the yardstick stands), and the only measured band
  price, bread, was out of its band at 0.027 LD in the run costed at the pace, not in the one without (0.076). What the
  60p runs without costing at the pace show (`m3`: `3cec19a`, 60 days, `costAtPace` off): the realm fed (none hungry on
  day 30 or 60), bread 0.074 and 0.072 LD (band 0.05–0.09), an iron and a steel sword 5.0 and 10.1 LD, the pace 0.107
  and 0.196, the household Gini 0.552 and 0.562 and the poorer half 0.152 and 0.136 (the 12p base 0.561, 0.140 on day
  30). But money drains out of the founding's holders while the 12p base stays level: from day 2 to day 30 and day 60
  the great houses lose 17% and 21% (the base 0.8% by day 30), the treasuries 26% and 61% (the capital's 34% and 74%),
  the paid civilians 21% and 22%, while the tills gain 46% and 70%; owners put 16,300p a day into their businesses
  against 12,700p of profit (days 15–29); and 29, then 35, grown wolves are without work (the base 5). So the owners'
  profit against wages (0.16–0.30 against 1.49) is paid for by the founding's capital: not an equilibrium, and doc 64's
  "money by kind of holder steady within about 20%" fails within weeks. The drain is the balance work's (Phase 13).
  Costing at the pace is right (doc 63's `normal_cost`) once a business's paid hours are its worked ones, which is the
  labour market's to mend (hands that follow the work, not the bank); until then it undoes the rest. What the holders' fall
  is, and what isn't the model's in it, is in "money by holder, and the wage bid" below; costing at the pace is now
  behind `ladder.json`'s `costAtPace`, off.

**Tests:**

- `crafting_tests` `pricedAtTheHoursWorked`: by the hours at the pace 0, a loaf costs the old pace's hours less, and so
  does the batch it bakes; at 0.5, half way; with the hours off, the calibrated.
- `crafting_tests` `foundedAtTheFoundingDay`: a realm of 500 starts at `foundingDay` (499: the catalogue's); its
  bakery's float, a meal, the ledger's dearest piece (1,200p moved, refused in the village) and a penniless household's
  relief at the first turn (25p, 5p in the village) follow the day; its measure keeps 60p; one with no maker to measure
  keeps it too.
- `roads_tests` `shopkeepersDontHoard` and `greatHouses`, and `gathering_tests` (the stall's fee returned is the fee
  paid): the amounts in LD.
- Each fails with its change undone (eight checked one at a time). All suites pass (`troubles_tests` and
  `newcomer_tests` failed once each with the machine loaded, and passed when run again).

**For the user:**

1. **Price at the pace now, or after the labour market?** Built as asked; on its own it undoes what the founding wage
   and the amounts gain (the tables). The alternative is the calibrated hours until a business's paid hours follow its
   work.
2. **Players' services** (the list above) are still at their 12p prices: a bed for the night at 2p is a thirtieth of a
   day's wage. (Answered and scaled by the review, below.)
3. **DEV** keeps its 12p rate until it is founded afresh.

### Phase 13: the price level, the review mended (2026-10-26)

The price review's findings on `p13-r3-price` (all but the money drain at 60p, which is the balance work's, and costing
at the pace, which is behind `costAtPace` and off), and the price report's questions 2 and 4.

**The seconds' pace stays as it was** (the review's major). That pace's going-rate measure keeps the skill's premium in
(as it always did), so frosthollow, mirebridge and oakhaven measure 12.8p from day 2, and every amount written in LD
moved there in round 3 (a sword 30p to 32p, a keeper's reserve 20p to 21p...). Of the two ways the review gave, the
amounts' rate: taking the premium out of that measure would move the pace's own wages (a new employer's wage, the wage
cap, the works' and the institutions' wages all read the town's rate), so the default game would not be as it was.
Instead the amounts and the catalogue's prices are read at `Society::amountDay(town)`: with work by the hours, the town's
going rate, as before; at the seconds' pace, the realm's day at its founding, `memory.foundedDay` (set by `foundWage`;
saved through `RatwWire`, bounded to 0..100,000; an older save, a village and a realm founded before this have none,
the catalogue's 12p). In a realm founded at 12p every amount is what it was at either pace; one founded at 60p (DEV
after `--refound-economy`) has them at 60p at the seconds' pace too, however its towns measure.

Proved on DEV's export with a `Data` copy at `foundingDay` 12, 10 days without `--hours`, `--deterministic --threads 3`:

| build | digest |
|---|---|
| `e0e6f45` (round 3's base) | c7d9780768522469 (the review's) |
| `1aa4047` (`p13` before the price merge: the second review's and the harness's) | 4ed72a4dec8e66e2 |
| `3cec19a` (`p13` now) | 64616e353aba92bb |
| this branch | **4ed72a4dec8e66e2** |

This branch is `1aa4047` exactly: the same digest, and `flows.csv`, `tills.csv`, `prices.csv` and `wages.csv` byte for
byte; `days.jsonl` differs only in `model.going[""]` reading 12 on days 0 and 1 (round 3's `foundWage` sets it at the
founding; nothing in play reads the record). It can't be `e0e6f45`'s, deliberately: `b6b9a86` (the second review,
mended; the harness merge changed no Core file) changes the default pace on purpose — the roads' rules at both paces (an
order cut by half, a failed seller passed over for a month, unpaid goods not short, loads by departure), missed sales by
the day's buying, the old pace's wants bought one at a time, the founding flag. Its own report says so; its digest
differs from `e0e6f45`'s from day 2.

**A merge fault, mended.** `b6b9a86` brought the old pace's children's stipends and wants back verbatim, with
`FoodADay` and `MostStipend` as pennies; the price merge made both LD. So in `3cec19a` a household's fortnight of food was
5.8p a head (not 70p), a stipend's base 0.25 (truncated to nothing), and the 2p, 3p and 9p floors and caps were raw. Now
each is `pennies()` at the town's day. `roads_tests` `childrensStipends` catches the fortnight and the stipend's base
(the old-pace check `stipend > MostStipend` compared with 0.25 and could not fail; a household of 700p must now give 3p
and a penny or two, one of 300p none, and one founded at 60p five times either). The wants' week of food and the
child's 2p floor are converted too but no test can tell: the household rule keeps the fund under what the purse can
spare, and at 60p nothing costs under 5p; with either taken back the 10-day digest above is unchanged.

**The amounts still unconverted** (the review's list, and a grep of the merged code):

- `repairCost`: half the catalogue's price for the wear, at the town's day (`atGoingRate`).
- The town's talk of a meal's or herbs' price: `World::talkedOfPrice`, the catalogue's 6p or 2p at the town's day, by
  the stores' factor.
- A resident's letter of thanks: a gift is worth one from 5p at a 12p day (coins and the goods' catalogue price, at the
  resident's town's day: `Game::giftWorthThanks`), and the pennies it sends, a tenth of its purse and 6p at most at a
  12p day (`Game::thanksGift`; missed in round 3).
- The client's stall price box: the server sends `priceMost` with one's stall; the box and its prompt use it.
- A farm's money for its bunkhouse's food: a penny for each 15 of the nourishment it wants, 2p at least, at a 12p day
  (`Society::farmhandsFoodMoney`; it was a 12p-era food price).
- The troubles' food a day: in whole pennies at the town's day for amounts, as the households' (`pennies()`).
- The old pace's children's and wants' amounts (above). `ResidentPurseLeast` (the tables) went with the players'
  services (below).
- Kept, as round 3 kept them: the catalogue's own filters (3p or less, 3p or more), sanity bounds (a gift of 10,000p at
  most, a project's 100,000p), the demo roster.

**One rate for the caps** (the review's minor). The ledger's dearest piece (`PieceMost`), a stall's dearest price and a
story's prize are all at the realm's day, `amountDay("")`; a stall's 999p at a 12p day is under the ledger's 1,000p in
any town, so a stall never takes a price `transfer()` refuses (`gathering_tests` `stallsAtTheFoundingDay`, by the hours in
a town at 72p against a 60p realm: 4,996p refused, 4,995p taken and sold). A keeper's reserve is at its own town's day
(`accountTown`) in `spendable`, the wealth line and its owner's top-up alike.

**The founding, its edges** (the review's minor). `foundWage` moves an older save's employers' books with its towns' rates
when it founds them at the day (scaled by the founding's day over the town's old rate), so the next measure keeps the
day. The "kept" rule applies only to a realm founded at its founding day (`foundedDay` set); a village with no maker to
measure falls back to the catalogue's 12p, as it did.

**At 60p, tested** (the review's minor: most conversions passed every suite at 12p by construction).
`Tests/founded_fixture.h` (`FoundedAtTheDay`, with `Society::setFoundAnyLand`, a test's switch) founds a test's village
at `foundingDay` through the real `foundWage`; `crafting_tests` founds a realm of 500 for real. Checked five times a
village's (each fails with its conversion taken back):

- `crafting_tests` `foundedAtTheFoundingDay`: a meal, herbs and a sword at the stores' prices, at both paces; a
  player's welcome grant; a keeper's reserve; a farm's bunkhouse food money; the amounts' day, and at the seconds' pace
  a town measuring above it not followed (the troubles' food a day with it); a village falling back; an older save's
  books; plus round 3's float, meal, dearest piece and relief.
- `crime_tests` `finesAtTheFoundingDay`: a theft's fine and what was taken; a shop's wholesale meals from the stores; the
  price the town talks of.
- `roads_tests` `contractsAtTheFoundingDay` (the bounty and the escort) and `childrensStipends` (the old pace's).
- `gathering_tests` `stallsAtTheFoundingDay`: a stall's rent, the welcome grant, the dearest price and its box, a
  keeper's reserve at its town's day, a Hold's draw (`Game::drawnToAHold`), a gift worth thanks and the thanks' pennies.
- `schedules_tests` `restdayService(true)`: the plate, 5p or 10p.
- `hunt_tests` `mendingAtTheFoundingDay`; `wire_tests` (the founding's day saved, dropped when it can't be, none in an
  older save).
- Mutation checks: 22 of this part's changes taken back one at a time, 20 caught; the two not (the wants' week, the
  child's floor) are the unobservable ones above. Twelve more for the players' services, all caught.

**The players' services (the price report's question 2): scaled.** From the code: every one of these is a transfer in
the ledger between a player and the realm's own accounts (an inn's till, a resident's purse, a treasury, a faction's
official), so each is part of the closed economy, not a label; a player's income was already at the day (the welcome
grant, contracts, bounties, the going prices it sells at, residents' letters' rewards), so leaving its costs at 12p made
them a fifth of what they meant; and the catalogue's own prices, which these were set beside, are already read at the
day. In a realm founded at 12p each is what it was. So each is read at a 12p day, at the town's day for amounts
(`Game::atTheDay`, `priceWords` for "a penny"): a bed upstairs (2p a night, 10p a week), a lodger's week (6p), the
whole upstairs (8p); a Chapter's rooms and warehouses (30p, 50p a week) and Atlas's lettings (`let` records); a notice
(1p); a Chapter's founding (20p); a festival's entry (1p), a resident's purse to enter (10p), the town's 2p an entrant
up to 10p; Liar's Bones' stakes (5p at most), the house's penny, a resident's purse to stake (30p); a faction's report
(5p) and tithe (20p), in their action labels; an apprenticeship's fee (`troubles.json`'s 20p) and the notable debt
(100p); a lent thing's courier (1p) and, lost, its catalogue price; the courier's fees (`letters.json`), sending letters
on, and the most a letter carries (50p); a companion's wage (6p, 10p roving; the harness's `strike` mirrors it); archive
work's pay (`archives.json`'s 2p); a Hold's toll (5p at most); a Chapter's buildings' materials. Kept as set: what a
Dungeon Master enters (a place's rent for individuals, a treaty's tithe, DM coins) and what a player chooses under a cap
(a toll, a stake, a gift). The client shows what the server sends: the notice's fee and a contest's entry on the board,
ENTER at a festival, the stakes to choose, the courier's fees, a founding's fee on its proposal, the toll's most, the
archive's pay. `gathering_tests` `servicesAtTheFoundingDay` checks the beds, the notice, the stakes, the courier's fee
and the enclosure, the upstairs' week, a report, a tithe, archive pay, a tent and a companion's day at 60p. Not checked
at 60p (each is the same at 12p, and every suite passes): the festival's pennies, the house's penny and the residents'
purses at the tables, the founding fee, the apprenticeship's fee and the notable debt, the loan's courier and price,
sending letters on, the toll's most and Atlas's lettings. (Since the tidy, below: the loan's courier, an Officer's
draw and the archive's pay itself, not only its figure, are checked at 60p.)

**The founding's seeds (the price report's question 4): keep them unscaled.** From the code (`worldMoney`): in a realm
of `WorldMoneyResidents` or more the seeds (`ChildPurse` 5p ... `KeeperPurse` 80p, `TreasuryHead` 40p, `ChurchHead`
10p, `HouseFortune` 3,000p) are only the floor each purse starts from before its kind is made up to its share of the
5,000,000p (households 40%, by an exponential lot; businesses 15%, by floats already at the day; houses 30%; the church
5%; the treasuries the rest, by heads). Each kind's total is its share whatever the seeds; the seeds set only how much of
it is spread evenly. A grown wolf's 30p against a household share of about 2,500p each is an even 1.2% floor; scaled
to 150p it would be 6%, flattening the model's opening spread. After the founding (the tidy's review found) one is
still read: a church founded late, as an older save's never opened, is given `ChurchHead` a resident from the treasury
if that is more than its month's basket; it was the raw 10p, a fifth of its worth at 60p, and is now at its town's day
(below, "the reviews' minors"). `HouseFortune` is read for a house counted new, but only before the purses are
founded. A new purse otherwise comes from its treasury; a child's first pennies are already at the day. In a test's
village they are its whole money, at its 12p day. Scale them only if the user wants a starting purse to be so many days' food rather than the
model's spread.

**Measured** (the 30-day smoke, `--deterministic --hours --threads 3`, DEV's export, `foundingDay` 60, against `m3`:
`3cec19a`'s code, built from the main checkout at `94e8e7f` with changes and run with `--threads 6`, as its `run.txt`
says; a deterministic run is the same on any number of threads): money conserved on every day; digest
9c4c291d44a5f898. It parts from `m3` from day 3: the farmhands' food money at 60p (about 10p a day against 1p) and
keepers' reserves at their town's day. That the adults without work halve (29 to 15 on day 30) and the pace falls
(0.107 to 0.089) was put down to those without a run to show it; the tidy's runs take each back in turn (below, "the
reviews' minors"). A tick
0.0307 ms against 0.0309 for `3cec19a`, side by side (5 days, the society's part 0.0121 in both).

| day 30 | this branch | `m3` |
|---|---|---|
| hungry, starving | 0, 0 | 0, 0 |
| household Gini; the poorer half | 0.552; 0.150 | 0.552; 0.152 |
| the going wage: the realm's, Ridgemere's; pace | 60p, 60p; 0.089 | 60p, 60p; 0.107 |
| bread, a bronze, an iron, a steel sword, a rapier (LD) | 0.073, 2.5, 5.08, 10.12, 24.98 | 0.074, 2.5, 5.04, 10.12, 24.98 |
| grown wolves without work | 15 | 29 |
| great houses, day 2 to 30 | 1,494,143p to 1,253,127p (−16%) | to 1,238,233p (−17%) |
| treasuries (the capital's) | 496,291p to 369,499p (−26%; −33%) | to 366,643p (−26%; −34%) |
| the church | 248,352p to 280,610p (+13%) | to 281,026p (+13%) |
| households | 2,016,252p to 2,039,897p (+1%) | to 2,018,125p (0%) |
| tills (farms', houses', shops') | 727,036p to 1,022,134p (+41%) | to 1,062,802p (+46%) |

A day, days 15–29: the owner's profit 13,968p (`m3` 12,725), the keeper's wage 20,480p (20,530), an odd job: a hand
15,105p (15,636), service wages 8,102p (7,711), an owner's investment 15,334p (16,336). The drain the review found is
still there, as it should be: this part doesn't touch it.

**For the user:**

1. **Players' services at 60p** were scaled (above): recommended, since a player's income is already at the day. If any
   should stay a token (a festival's penny entry), say which.
2. **The founding's seeds**: recommend keeping them as they are (above).
3. **What a DM or Atlas enters**: Atlas's lettings and `Data`'s fees are read as pennies at a 12p day, like the
   catalogue's prices; a DM's amounts as the pennies of the day they are entered. Recommend Atlas and the DM tools say
   which, or move to entering in LD.

### Phase 13: residents near players' bodies (2026-10-26)

What the bodies' control (above) found, mended: residents near players' bodies went hungry, on their way home to eat.
It was the world's walking (`World::integrate`, `World::headFor`), not the economy's or the harness's. All of it shows
only on the stage, where residents walk tile by tile, press against each other and use doors and seams; offstage they
hop between known places and meet none of it, so the runs without players never could see it. Traces of the hungry
(`RATW_TRACE`, and a count of every walker getting no nearer its next turn) found four faults.

- **Bodies at a turn** (`World::heldOff`). A walker passes a turn of its path only on reaching it, and bodies
  (`step::BodyRadius`) keep any two a body's width apart. So of several making for the same turn at once only one
  could stand on it, and the rest pressed round it for good: knots of three to six at the tile a seam is crossed by
  (`w_ridgemere_1_2`'s north seam at 49,0, residents of four houses standing there for game hours: "in the street
  outside" the inns), whirls of five or six at a street corner the roads' paths share (220.5,64.5). Now a walker that
  has come no nearer its next turn for two seconds goes on to the turn after, within a tile of it.
- **Face to face.** Two coming head on along one row of tile middles (the roads' paths keep to them) pushed each other
  straight back, for hours. Now a resident held as long, further than a tile from its next turn, steps aside a body's
  width to its right (or left; in a lane a tile wide, half that, and two pass) and goes on. (As first built, any walker
  held, even by nothing but the ground, a player's path too, and one on its last leg stepped aside as well: see "the
  roads and the walking, reviewed" below for what it does now.) The clock is numbers on the entity, never saved (paths
  aren't).
- **A door out of sight** (`headFor`). A resident pushed off its way within reach of its own front door, but where it
  couldn't see it (beside it, past its wall's corner, at night), tried the door at every pass ("You cannot see that
  here") and stood there for good. Now it walks up to the door, as from further off.
- **The parts of a cell** (`headFor`). Ridgemere's cells are in parts: `w_ridgemere_2_2`'s body (48,310 tiles) and,
  behind it, a district of 2,308 reached only across its north edge and the west edge's first rows, where Ashworth
  House's door is; `w_ridgemere_2_0`'s body and a district of 2,319 where Greatly House's door opens. A way into a cell
  was judged good if it landed in the cell's body and a pocket otherwise, and a cell whose every way in from here
  landed in a "pocket" was taken for a dead end, for everyone, for good (`deadEnds_`).
  - Residents going home to Ashworth House crossed at the road's seam into the body, found no way on, crossed back,
    and in again, every few seconds for game hours.
  - One resident of Greatly House leaving by its door, and another making for her spot upstairs in a room no stair
    reaches, made the street and the stairs dead ends: the whole household, onstage or off, had no way home after
    ("route unavailable"; in the old jumping control, the hungry "offstage, in houses not theirs").
  - Now a pocket is a part nothing leads on from but back; a part with ways of its own is somewhere to go on from. On
    the way through, a way in that lands in a part the next cell's way on leaves from (a way on into a pocket doesn't
    count) is best of all. A resident in the wrong part of a cell goes out first to a neighbour with a way back in to
    the right part (told by this cell's own ways, which come in pairs, so the neighbour needn't be in memory). And a
    dead end is recorded only on the way through, for a true pocket, never by one walker's goal.

**Measured** (10 days of DEV's export from day 2, `visit`'s five bodies from day 2, `--deterministic --hours --threads
2`, against `e0e6f45`; every run's money conserved every day; the pace 0 in all, as without players):

| Bodies | hungry, starving on day 10: before / now | the worst day before | hours worked on day 10: before / now (5,214 without players) | a tick: before / now |
|---|---|---|---|---|
| none | 0, 0 / 0, 0 | | | 0.058 ms / 0.050 ms |
| still | 55, 36 / 0, 0 | 111, 92 (day 8) | 4,275 / 5,248 | 0.91 ms / 0.18 ms |
| walk | 193, 134 / 0, 0 | 193, 134 (day 10) | 3,627 / 5,143 | 1.40 ms / 0.29 ms |
| jump | 90, 56 / 0, 0 | 90, 56 (day 10) | 4,396 / 5,210 | 0.53 ms / 0.27 ms |

None hungry at any day's end in any of the three (the walking run's last row, a morning, has one hungry: ordinary
morning hunger). Most of the stage's cost was the knots: keeping bodies apart took 0.44 and
0.63 ms a tick with the still and walking bodies, now 0.02 and 0.04; their walking 0.24 and 0.38, now 0.03 and 0.06.
Gini and the poorer half are within 0.011 of the run without players on day 10 (the walking run's Gini 0.555 against
0.545; 0.551 before the mends). **Without players nothing changes:** the 30-day smoke (`--threads 3`) gives the digest
aded872975ff165c, `e0e6f45`'s and the merged run's (`m2`), its `days.jsonl` and `flows.csv` the same byte for byte; a
tick 0.047 ms.

**Tests** (each fails with its mend undone):

- `world_tests`: `heldAtATurn` (two meeting at a turn from either side both go on; alone, a walker turns on the turn
  itself; face to face along one line, they step aside and pass), `aDoorOutOfSight` (beside the yard door, past its
  wall's corner, the scout can't see it to open it, and walks up to it).
- `stream_tests`: `aCrossingIsNotHeldShut` (three making for one seam at once all get over), `aLaneATileWide` (face
  to face in a lane a tile wide, they pass), `aStripIsNoWayOn` (a strip whose only way on lands in a pocket isn't
  taken for a way on), `aCellCutInTwo` (home to a door behind a wall, round by the next cell, in at most six crossings;
  and out again in the morning: the part the door opens on is no pocket), `aSpotNoWayReaches` (a spot no way reaches
  makes no dead end for the rest of its household).

**Left:**

- One carrier's delivery to Ridgemere's market (`errand`: the market's point and a tile below it, 255.5,37.5 in
  `w_ridgemere_1_1`) can't be walked to on the stage ("No reachable route"): he tries every five seconds, and the goods
  wait while he's on the stage (offstage he hops there). Not hunger (an errand gives way to a meal) and not the
  walking's: the market's point is the roads' (`RatwRoads.cpp`).
- A resident whose spot at home no way reaches (a room upstairs no stair leads to: Greatly House's) still can't get
  to it on the stage; now she alone is held, going in and out, not her household. (Mended in "the roads and the
  walking, reviewed", below: she keeps to the nearest of the floor she can stand on.)

### Phase 13: players who push, the third review (2026-10-26)

The third review of the players who push (`p13-r3-harness`), mended, and three more things a player base would do:
`buyout`, `drain` and `gift` (the table in "players who push", above). `d372517` and `e64f172`; only the harness
(`Tests/econ_watch.cpp`, `Tests/econ_players.{h,cpp}`, `Tests/econ_players_tests.cpp`,
`Tests/econ_watch_stamp.cmake`), no Core file. Each change is guarded in `econ_players_tests` (421 checks), and each
guard was seen to fail with its change taken back (the harness review found two that weren't: `strike`'s warrant and
custody checks and `gift`'s custody skip; guarded since the tidy, below).

**The review's fixes:**

- **`flood`'s staple is a choice** (`--players-staple ITEM`), raw meat unless asked. Fish sat at the penny's floor at
  a 12p day (`Society::pennies`: 1p at least; it asked 1.07p to 1.16p), so it could never be seen to fall; at the
  60p day fish is 5p to 6p, raw meat 14p to 17p. A staple the stores price (meals, herbs, swords) is refused. Raw
  meat is a butcher's make (`butcher_pig`, `butcher_sheep`), not the land's, so a shop whose business makes the
  staple counts among its makers (the capital's two butchers). Such a shop isn't asked its price in the day's block:
  its cost of making it is worked out once a day when first asked for and kept for the day (`unitCost_`), so asked
  at midnight by the harness it would be the day's for the game too. Before the players start, a `flood` run's
  files and records are byte for byte the run's without players. The doc now says what the game does that shapes a
  flood: a shop pays a player its own normal cost (`Society::offerAt`), and a player's goods never spoil.
- **`strike`** sets its "let go" when the players' days end before its month (it read `false` with none held), and
  takes none `Game::whyNotJoin` refuses whatever the hour: the Quickened, a wolf beaten down, one with a warrant or
  in custody. The table says what the stand-in skips: the game asks each awake and out of its working hours, a few
  at a time over days (the companions' cap).
- **The comments:** a flood's goods don't spoil "as anyone's do" (`Society::spoil` passes players by, and a pack
  landed days ago goes to a shop as fresh); `flood` sells 8 to 18, not every hour.
- **`run.txt` says which Data it read**, as a line of its own (never a warning: Data is read at run time): the
  directory (`RATW_DATA_DIR`, else the working directory or one above it, else the source's: the catalogue's own
  rule), its checkout's describe, and Data's own changes since its commit (`none`, or the SHA-1 of their diff and
  of its untracked files): `data: <worktree>/Data at d3725173abad; its own changes since: none`. Data in no checkout
  says so. And the stamp's code check covers only econ_watch's own test files (`Tests/econ_watch.cpp`,
  `Tests/econ_players.{h,cpp}`, `Tests/econ_watch_stamp.cmake`), not every suite's: another suite's edit gave
  "rebuild it".
- **Labels:** the bodies' walk row is `4e83cec`'s, before the re-aim throttle (said in the table); the 3-day bodies
  smoke ran on `af5d842`'s binary (the two commits after it touch only `flood` and `strike`). Question 7 is
  reworded, and the stray half-line after it put back at the end of question 5.
- **The three test gaps:** `flood` runs through a second morning after a part sale and lands exactly what it sold,
  each pack full again; `poach` with three hands' day in its purse and the treasury empty pays those three, notes the
  rest short, and sends three home; and the run's `end` record is checked to carry the players' doings since the
  last midnight (the Hold's dawn pay) and none of the day before's.

**Found on the way:** `buyout` first bought almost nothing (50 pieces the first morning): it moved on to its next
player at every food left on a shelf, and the first five foods a shop held and didn't sell (its materials) used up
all five buyers. Now it tries only what a shop sells (`e64f172`, guarded).

**The runs** (DEV's export, `--deterministic --hours --threads 3`, players from day 2, against the merged baseline
`m3`, a 60-day run of `3cec19a`'s code: its `run.txt` names the main checkout at `94e8e7f` with changes, but its rows
to day 29 are byte for byte this branch's smoke's). Money conserved in every record of every run. `run`, `flood`, `strike` and `gift`
are `d372517`'s binary, `buyout` and `drain` `e64f172`'s (`drain`'s run matched `d372517`'s byte for byte to day 23,
where that one was stopped from outside the run).

(The harness review found these runs' readings mixed with the realm's pace, which each run moved by its own hunger and
food: `strike`'s 26 starving hands, 2.4% of the residents, over the 2% at which the pace steps back each day, held it at
0 for the whole month against `m3`'s 0.107 on day 30, and `flood`'s ran ahead, 0.196. So the hours, wages and profits
below set beside `m3` are each scenario's and its pace's together. The tidy (below, "the players' runs with the pace
held") runs each again with the pace held to the run without players, day by day, and the questions are put on those
runs; what is wrong here is marked where it stands.)

The 30-day smoke without players: 11.3 minutes, 0.078 ms a tick, digest `61df5f39a3ec5247`; every row of
`flows.csv`, `tills.csv`, `work.csv`, `books.csv`, `residents.csv` and `prices.csv` to day 29 and every record to
day 30 the same as `m3`'s. Day 30: no one hungry or starving; the household Gini 0.5519, the poorer half 0.1519,
the wage 60p, the pace 0.107, the going rates 60p (the realm's and Ridgemere's). Money by holder, day 2 to day 30:
the great houses 1,494,143p to 1,238,233p, the treasuries (the capital's and the towns') 496,291p to 366,643p, the
church 248,352p to 281,026p, households (every resident's purse) 2,016,252p to 2,018,125p, tills (shops', farms',
houses') 727,036p to 1,062,802p. Days 15 to 29, a day: the owners' profit 12,725p, the keepers' wages 20,530p, the
hands' odd jobs 15,636p, service wages 7,711p, owners' investment 16,336p.

| Run (day 30) | hungry, starving | Gini | poorer half | pace | households | treasuries | tills | players |
|---|---|---|---|---|---|---|---|---|
| without players | 0, 0 | 0.5519 | 0.1519 | 0.107 | 2,018,125p | 366,643p | 1,062,802p | |
| `flood` | 0, 0 | 0.5316 | 0.1593 | 0.196 | 1,935,694p | 363,215p | 1,044,965p | 109,309p |
| `strike` | 26, 26 (its hands) | 0.5537 | 0.1486 | 0 | 2,034,112p | 374,690p | 1,047,226p | |
| `buyout` | 0, 0 | 0.5391 | 0.1578 | 0.125 | 2,082,674p | 219,347p | 1,123,755p | 7,682 pieces of food |
| `drain` | 0, 0 | 0.5560 | 0.1483 | 0.107 | 1,988,066p | 359,493p | 1,032,847p | 52,305p |
| `gift` | 0, 0 | 0.5026 | 0.1848 | 0.125 | 2,134,145p | 227,115p | 1,069,133p | 10,100p left to give |

The wage and the going rates were 60p in every run on every day. Days 15 to 29, a day (without players: profit
12,725p, keepers 20,530p, odd jobs 15,636p, service 7,711p, investment 16,336p): `flood` 6,471p, 20,205p, 15,535p,
7,879p, 16,650p; `strike` 13,296p, 20,237p, 14,410p, 7,042p, 16,135p; `buyout` 13,141p, 20,600p, 16,407p, 7,809p,
16,305p; `drain` 12,725p, 20,375p, 15,700p, 7,951p, 15,643p (and 1,140p to the players at the benches); `gift`
12,564p, 20,586p, 15,546p, 8,088p, 17,119p (and the 5,000p of gifts).

`flood`, raw meat, 30 days (selling days 2 to 15):

- The ten sold 8,792 pieces in their two weeks for 108,309p (12.32p each, the shops' own cost) to the 39 capital
  shops that buy it in, and kept it: all 990 they landed the first day, then 193 to 990 a day. The shops' shelves of
  raw meat went from 264 to 1,180 on the first day and 1,233 to 1,602 through the flood; two days after it, 522, and
  from the fourth day after, 81 to 335 as before.
- **No shop's price fell; what residents paid on average fell, by where they bought** (the review: this was headed
  "the price fell, and had come only part way back", a price that never moved). The shops that buy it in asked,
  before the penny, 14.10p before, 14.0p to 14.4p through, and 15.2p to 15.9p from the sixth day after (prices were
  rising without players too); a shop pays a player its own cost, so none of them paid less. What moved was where
  residents bought: 3,002 pieces against 589, from those shops at about 14p rather than from the butchers, who price
  it at their cost of making it, so the mean paid was 14.27p through the flood against 17.17p, and 16.30p in the two
  weeks after against 18.68p (704 against 515 pieces: the same mix, not a recovery). Shops buying it
  as materials paid 14.98p against 16.15p during, 16.29p against 16.79p after. Residents' food as a whole cost
  0.993 of what it did without players, during and after.
- **The makers kept their posts but lost trade.** The capital's two butchers sold 1,983 pieces of everything
  (32,297p) through the flood against 4,032 (62,037p), half; in the two weeks after 2,830 (42,571p) against 3,399
  (54,817p), and over the 16 days after 12% fewer (17% less taken). They worked the same hours (348 against 352
  through the flood, 424 against 407 after).
- **The money didn't come back.** The 108,309p came out of the capital's shops and stayed in the players' purses:
  the owners' profit was 13% lower through the flood and a third lower in the two weeks after (11,051p a day against
  16,500p, days 16 to 30; days 15 to 29, the run table's, give 6,471p against 12,725p, half: the profit is paid in
  lumps, each business's month on its own day, so a window's sum swings with the pay days it holds, and the pace
  differed besides: below, by whole weeks, with it held); on day 30 households held 82,000p less and the tills
  18,000p less. Gini and the poorer half moved the
  other way (0.5316, 0.1593), likely because the owners' profit, which goes to richer households, fell most.

`strike`, 44 days (the hands held from day 2 to day 30's dawn), at a 60p day, as the first run at 12p found:

- 26 hands taken; 25 hungry by the first day's end and all 26 hungry and starving to the month's end; none the day
  after they were let go. 26 kept their posts to day 29, 25 on day 30 and 24 by day 44.
- **The labour market didn't see them gone.** The going rates 60p throughout; Ridgemere's employers' mean wage
  62.69p on day 29 against 63.07p; hires' slots (running at 23:00, summed) 16,733 against 17,541 in the month, taken
  13,456 against 13,939.
- 1.5% fewer hours in the month (155,511 against 157,800), service wages 9% down and the hands' odd jobs 8% (days 15
  to 29); the owners' profit 4% up. (Not "the work wasn't done", as this said: the hands' hunger held the realm's
  pace at 0 all month, so every craft took its old hours; below, with the pace held.)
- In the two weeks after, hours 1.6% fewer (76,090 against 77,306), service wages 2% and odd jobs 3% down (the pace
  still behind; not "the work hadn't come back", as this said). Day 44: no one hungry; Gini 0.5593 against 0.5596,
  the poorer half 0.1354 against 0.1385, the pace 0.179 against 0.196.

`buyout`, 30 days (buying days 2 to 8):

- The five bought 7,667 pieces of food in five mornings with their 150,000p: 2,622 (50,489p) the first, then 1,464,
  1,296, 1,419 and 866, when their purses ran out (47p left); nothing on the sixth and seventh. They held 7,682 to
  the end.
- **The corner failed: the makers answered.** At the first day's end the capital's shops held 2,655 pieces of food
  against 3,578 without players, at the next 3,399 against 4,007, and from the fifth day's record, only just, more
  than without (4,335 against 4,289); from the ninth to the twentieth half as much again (8,400 to 14,400 against
  5,700 to 10,700). (Both sides `towns.ridgemere.food`, the capital's shops' food in pieces at the day's end; this had
  set the players' own block, the merchants' tills, beside it.) Its food crafted in the
  buying week was 23,099 against 10,128, then less as the shelves stood full (8,216 against 13,614 in days 17 to
  23), and as without players in the last week (10,144 against 10,804).
- No one went hungry on any day (the households' larders hold 2 to 5 days; "short" 2 to 7 against 0 to 4).
  Residents' food cost 1.007 of what it did without players in the buying week, then 1.001, 0.997 and 1.010: prices
  are costs and margins, and a margin moves a little a week.
- The 150,000p, from the capital's treasury, went to the shops (the tills 61,000p richer on day 30) and on to their
  owners and hands (households 65,000p richer). By day 32 Gini was 0.5496 against 0.5567.

`drain`, 30 days, Frosthollow:

- **What a player can do at a bench isn't what it looks like.** Of Frosthollow's 15 "benches", 11 are wolves who keep
  their houses, one keeps the Snow Chapel, two work at the Smelter and one helps at the Pass Provisioner.
  `World::residentWorkAt` takes the work from a resident's authored work label (`spec->workLabel`, which names a
  business that crafts), not from its post now, so a wolf at home keeping house is "at its bench"; and a shop's
  keeper, whose task is to trade, never is, so on DEV's map no player can lend a paw at a shop's own bench.
  `Society::tillOf` for a wolf whose post has no till is its own purse, so the players were paid out of the wolves'
  own purses (`a hand at the bench`, from paid civilians): 5,382p the first day, about 1,500p a day over the month,
  49,305p in all.
- **The town's wolves lost no work, but their money.** Frosthollow's hires (slots running at 23:00, summed over days 3
  to 31) 1,050 against 1,124, taken 874 against 863; its residents' wages 789p a day against 772p. On day 31 its 56
  residents held 56,730p against 97,122p, their median purse 535p against 1,671p; none hungry.
- 0.149 ms a tick with the 30 bodies on the stage, against 0.078.
- The brief's drain (players taking every odd job and hire so the town's wolves get none) can't be run: a player
  takes no odd job and no hire.

`gift`, 30 days:

- 150,000p from the capital's treasury, given at 5,000p a day (100p to each of the 50 poorest households) every day,
  all of it by day 32. Money conserved in every record; on day 30 the households held 116,000p more and the
  treasuries 139,500p less (the giver's 150,000p came from the capital's), the tills 6,000p more.
- **No inflation.** What residents bought cost 0.999, 0.991, 0.986 and 0.996 of what it did without players, week
  by week (their food 1.000 to 0.988); they bought 4% more in the last week (24,305 pieces against 23,392) and spent
  1% more. Prices are a shop's cost and margin, not what buyers will pay, and the margin moves only on a shelf that
  empties fast or won't move.
- The poorest tenth's purse 175p on day 30 against 82p; the median 1,577p against 1,444p; Gini 0.5026 against
  0.5519, the poorer half 0.1848 against 0.1519.

**For the user** (8 to 11 new; 7 reworded, above):

8. **Bench work pays from the wolf's own purse, and finds benches at home** (`drain`; Core, `World::residentWorkAt`
   and the beat's pay in `RatwTogether.cpp`). Thirty players took 49,305p in a month from Frosthollow's wolves, most
   of whom were keeping house, and halved what its residents held. Recommendation: a bench is a business's with a
   till of its own, worked by its post now (the post's title, not the authored label), and pays from that till;
   none where `tillOf` gives the wolf's own purse.
9. **Should a player be able to take an odd job or a business's hire?** Today it can't, so a `drain` that takes a
   town's work can't happen. Recommendation: not until hires are weighed against the town's own jobless (a player
   who takes one should take it from the board as a resident does, the jobless first), or keep them residents'.
10. **A flood's money leaves the shops for good** (`flood`, and `hoard` before it; reread by the tidy with the pace
    held, which this didn't change): players who sell and never spend take it out of circulation. The 105,465p the
    ten were paid stayed in their purses: the owners' profit was 34% lower over the 16 days after the flood
    (177,268p against 270,101p), by whole weeks 60% lower the week after and 6% the one after that (it is paid in
    lumps, so a week swings), and on day 30 households held 100,818p less. Recommendation: nothing in the economy (it
    is what a player does); the mint's rule (doc 64, Phase 11) is the place to answer it, if any.
11. **A food corner fails** (`buyout`; re-aimed by the tidy: it buys every hour from 6 to 20 all week, its 150,000p
    spread so it lasts, and the pace held; the first ran dry on its fifth morning and bought at six only). It bought
    10,968 pieces, about 21,000p a day; the capital's makers crafted 22,528 pieces of food in the week against 10,159,
    and its shops held more food than without players at most days' ends of the buying week (up to a fifth more),
    2% to 11% less after; no one went hungry, and residents' food cost 0.999 to 1.004 of what it did without players.
    Is that the intent, or should a buyout of a town's food make it dearer for a while? Recommendation: keep it (the
    model's: a price is a cost and a margin, and the makers answer within the day); if it should bite, let a margin
    rise within the week on a shelf emptied faster than it is made, not only at its weekly review.

### Phase 13: the reviews' minors, and the players' runs with the pace held (2026-10-26)

The price review's nine minors and the harness review's findings on round 4 (`p13` at `83714a0`), mended: `f8882be`
(the harness), `aa5a804` (the price minors, and `pace.csv` at every turn). The game's companion feeding is not
changed (a decision for the user, below). Every change is guarded by a check that fails with it taken back
(mutation checks, below).

**The price review's minors:**

- **An older save's books, loaded afresh** (`foundWage`). It moved each employer's book by `employerTownOf`, the
  labour market's record of each employer's town, which is made at the market's day and is empty in a society just
  loaded, so every book went by the realm's rate. Now each employer's town is found from its posts, as the labour
  market finds it (`Society::postEmployer`, shared with `tendLabour`). `crafting_tests` loads an older save of two
  towns afresh from its wire (the north at a quarter over the realm's): each book moves by its own town's (the
  north's had gone to 75p, not 60p).
- **A stall's price going stale.** A stall lives a market morning and the rate is measured at the day's turn, so in
  play a price can't outlive the day it was laid out at; across a save it can. A price past today's dearest
  (`PriceMost` at the realm's day) isn't sold at until its keeper prices it again ("That is priced past the dearest a
  piece sells for today..."). (`gathering_tests`: the realm's rate a hundredth lower, the 4,995p meal isn't sold;
  back, it is.) A resident pays no more than its town's price for what it wants, far under the dearest (the dearest
  such, a jewelled collar, 600p at a 12p day), so its buying needs no check.
- **A Chapter's building refunded as it was paid for.** A planned building keeps what its materials cost
  (`camp::Structure::paid`, saved by `Camps::save` in the checkpoint's camps, bounded to 10,000,000p; an older save's,
  or one past the bound, reads as unknown and refunds the kind's cost now, as before), and taking it back unbuilt
  refunds that. (`camp_tests`: planned by the hours, the town's rate doubled, 4p back, not 8p; saved, read, and an
  older save's and a wild one's unknown.)
- **The loan's courier** says the fee it took ("A courier takes back what you borrowed (5p).", none if the borrower
  couldn't pay it), and `gathering_tests` lends a sword for a day in the realm founded at 60p and sees the 5p go to
  the capital's treasury.
- **An Officer's draw**: at most 20p a day at a 12p day, at its Hold's town's day (the realm's, a Chapter with no Hold:
  a Chapter's treasury is the realm's), like the toll's most (`gathering_tests` `officersDrawAtTheDay`: 100p at 60p, a
  penny more refused, the Head any amount).
- **A save founded at 60p by `904d48c` or `3cec19a`** (which didn't save the day) is known on reading: its purses
  founded and its realm's rate over half again the catalogue's, it is taken as founded at `ladder.json`'s
  `foundingDay` (or at its rate, if the Data now founds at the catalogue's). The seconds' pace's measure keeps a
  skill's premium in (a town's 12.8p at a 12p day), well under. (`wire_tests`: 61.5p with its purses founded reads as
  founded at 60p; unfounded, or at 12.8p, as none.)
- **A church founded late** (above, the seeds) is given `ChurchHead` a resident at its town's day; at the founding
  it is still the raw seed. (`crafting_tests`: an older save with no church, founded after: at least 25,000p for 500
  residents at 60p, where the raw 10p gave 9,345p, its basket.)
- **Paid, not shown**: the archive pays through `Game::archivePay` (half for a copy with errors), the one expression
  its button shows, and `gathering_tests` runs the archive in the realm founded at 60p (paid 10p by the town); a
  keeper's wealth line (`wealthLine`) and an owner's reserve when it puts money into its short till (`tendOwners`)
  are checked at the keeper's town's day, a fifth over the realm's (`gathering_tests`, `crafting_tests`: 7p put in
  over a reserve of 120p, where the realm's 100p would put in 27p).
- **The text:** the founding's seeds paragraph (one reader after the founding: the late church); `m3`'s provenance
  (built from the main checkout at `94e8e7f` with changes, `--threads 6`); and the price branch's departure from
  `m3`, now measured (below).

**The harness review's findings:**

- **The pace held** (its major). With work by the hours, each run's pace (`memory.pace`, the realm's way from the old
  hours to the calibrated) moved by its own hunger and food, so a scenario set beside the run without players was
  measured with its pace. econ_watch now writes `pace.csv`, the pace at each turn of the society's day to the last
  digit, and `--hold-pace PACE_CSV` holds a run's pace to another run's day by day (`Society::holdPace`, a test's
  switch, never saved: at its day's turn the pace is the file's, whatever this run's hunger, food and hands would
  make it). Of the two ways the review gave, holding the pace to the run without players': the scenarios then differ
  from it by what the players did, not by the pace they moved; the runs with the pace free are kept beside them, so
  the pace's own share shows. (The turn falls on either side of the watch's midnight, so the pace is written at each
  turn, not with the day's records: at the records one day of the 44 went missing, day 36, and is filled for the
  reference run from its neighbours, exactly: 0.1428... and two steps of a 56th give day 37's 0.1785... to the last
  digit.) Proved: the run without players held to its own `pace.csv` is that run, all 44 days, every file byte for
  byte and its digest (efc2b6fad139b6ad); and `econ_players_tests` `heldPace` (a run held to its own pace is that
  run; held to another, its pace is the other's on each day; a pace out of 0 to 1, or a file with a bad line, is
  refused; a row at each turn, once).
- **`buyout`'s purse lasts the week** (the review: it bought once a morning and ran dry on the fifth). It buys on the
  hour from 6 to 20 every day of its week (`BuyoutFrom`, `BuyoutTo`), each player's money for the hour its purse over
  the week's hours still to come, this one among them, so the 150,000p is spread over the 105 hours and the last
  hour spends what is left (`Society::quote` first, `buyAll`'s budget). (`econ_players_tests` `buyoutLastsTheWeek`:
  with a shelf fuller than the hour's money, at six it spends its purse over 105 at most and food is left, it buys
  again at seven, on the week's last hour it spends all it has, and after the week nothing.)
- **The guards the review found missing:** `strikeEndsWithThePlayers` now gives the hand the Hold would take each
  refusal in turn (a warrant, custody, beaten down, still down, passing through, Quickened) and sees it passed over;
  `giftToThePoorest` puts a household's first grown in the watch's custody and sees the gift go to the next.
- **Mutation checks** (`r5-tidy/mutate.py`): 23 changes taken back one at a time, each its suite rebuilt and run; all
  23 caught (the book's town, the stall's stale price, the camp's refund, its keeping, saving and bound, the courier's
  words, the Officer's draw, an old 60p save, the late church, the archive's pay, the wealth line's and the owner's
  reserve, the pace held and at every turn, the hour's budget and the hourly buying, strike's warrant, custody, down
  and passing through, gift's custody, the pace file's bound).

**Why `strike`'s hands starve, and what feeding them would take** (question 1; not changed: the user's to decide).
A stationed hand at a Hold, as a party's companion, is a body with a leader (`Entity::leaderId`, "camp:<site>" for the
Hold's staff, set at each of the camp's rounds in `RatwGameCamps.cpp`), so the world hands the society its body as a
companion's (`LifeBody::companion`: a leader, or following) and the society decides nothing for it:
`Society::decideAuthored` stops at `body.companion` before the larder, the rations, the meal and the shop
(`RatwResidents.cpp`: the choice's early return, and in its turn `life.task = "companion"`), while its hunger still
rises with every step (`c.hunger = life.hunger + .0035 * c.step`, before either). Nothing ever lowers it: within a
day it is hungry (60), and by the next starving. In `strike` 26 of about 1,084 residents (2.4%) were, every day of the
month, which is over the 2% (`PaceHungry`) at which the realm's pace steps back each day, so the pace stood at 0 for
the whole month (0.089 without players by day 30) and so did every craft's hours and every price costed by them:
that, not the hands' absence, is most of what `strike` measured in round 4 (below).

Feeding them would take, in order of what it would cost to build:

1. **Eat what it carries** (Society, both companion branches in `RatwResidents.cpp`, about ten lines): hungry and
   carrying food, a companion eats as a resident does (`eatFirst`, `consume`, its hunger down by the food's worth).
   A resident carries one piece and keeps the rest in its larder, so this feeds it a day at most.
2. **The Hold feeds its staff** (Game, the camp's dawn round where it pays them, `RatwGameCamps.cpp`, about fifteen
   lines; the watch's mess is the pattern, `"rations"`): each dawn the Hold gives each stationed hand a day's food
   from the Chapter's stores (what its field, its cookfire and its workshops make, or what the Chapter buys), and a
   hand it can't feed goes home as one it can't pay does, the Chapter told. That is about `FoodADay`, 5p at a 12p day
   (25p at 60p), a hand a day, beside its 6p companion's wage (30p at 60p): `strike`'s 26 would cost the Chapter about
   650p a day more at 60p.
3. **Its own purse buying meals**: a stationed hand is off its schedule at the Hold, so it can't walk to a shop; it
   would want a sale at a distance (a stand-in the game has nowhere else) or leave to eat, which a post at a Hold
   doesn't allow. And a companion's wage is about a day's food, so it would spend nearly all its pay to eat.

**Recommendation:** 1 and 2 together: a companion eats what it carries; a Hold feeds its stationed hands from its
stores at dawn, as the watch feeds its own, and lets go one it can't feed. A party's roving companion (no Hold)
eats what it carries and buys food at a shop when the party is in a town, as a resident on the road would. With
that, `strike`'s hands are fed, and the pace is no longer `strike`'s measure.

**The price branch's departure from `m3`, attributed** (the price review's last minor). `83714a0`'s own 30-day
smoke and the same with each of the two changes the price report named taken back, `--deterministic --hours
--threads 3`, DEV's export: with the farmhands' food money as before (`max(2, wanted / 15)` pennies), the digest is
`m3`'s and the harness smoke's, 61df5f39a3ec5247, every day of it: 29 adults without work and the pace 0.107 on day
30, as `m3`. With the keepers' reserves at the realm's day instead of their town's, it is `83714a0`'s own,
9c4c291d44a5f898 (15 without work, 0.089): in 30 days that change moves nothing. So the whole of the departure,
the 14 fewer without work and the slower pace with it, is the farmhands' food money: at 60p a farm now buys its
bunkhouse about 10p of food a day, not 1p (why that keeps more wolves in work is not traced here).

**The players' runs with the pace held** (the harness review's major). DEV's export, `--deterministic --hours`,
players from day 2, 2 threads, binaries `f8882be` (the runs with the pace free and the run without players, `ref`)
and `aa5a804` (the runs with it held; its Core differs from `f8882be`'s only in the price minors, which change
nothing here: the held run without players is `ref` byte for byte). The run without players is 44 days; `strike`
44 (its month from day 2, and two weeks after), the rest 32 (30 days from day 2). Each held run's pace is `ref`'s
on every day (checked, day by day); money is conserved in every record of every run. A record is named by its day,
as before (day 30's is made at its midnight, so the tables' "day 30" is the end of the game's day 29); the windows of
rows are the game's days, the weeks whole ones from the players' first day (2 to 8, 9 to 15, 16 to 22, 23 to 29).

| Run (day 30) | pace held (free) | hungry, starving | Gini held (free) | poorer half | households | treasuries | tills | the players |
|---|---|---|---|---|---|---|---|---|
| without players | 0.089 | 0, 0 | 0.5521 | 0.1501 | 2,039,897p | 369,499p | 1,022,134p | |
| `flood` | 0.089 (0.179) | 0, 0 | 0.5325 (0.5341) | 0.1582 | 1,939,079p | 364,779p | 1,043,851p | 106,465p |
| `strike` | 0.089 (0) | 26, 26 (its hands) | 0.5510 (0.5482) | 0.1545 | 2,048,062p | 363,216p | 1,038,068p | |
| `buyout` | 0.089 (0.089) | 0, 0 | 0.5403 (0.5416) | 0.1559 | 2,041,016p | 216,794p | 1,174,195p | 10,983 pieces of food |
| `drain` | 0.089 (0.071) | 0, 0 | 0.5586 (0.5573) | 0.1472 | 2,000,985p | 363,764p | 1,015,346p | 52,136p of bench pay by day 31 |
| `gift` | 0.089 (0.143) | 0, 0 | 0.4990 (0.4997) | 0.1856 | 2,131,585p | 230,498p | 1,070,762p | 10,100p left to give |

The owners' profit by whole week (days 2 to 8, 9 to 15, 16 to 22, 23 to 29), and the month's sums of the rest (days 2
to 29), with the pace held:

| Run | owners' profit, by week | keepers' wages | hands' odd jobs | service wages | owners' investment | hours worked |
|---|---|---|---|---|---|---|
| without players | 116,087p; 70,519p; 137,001p; 123,503p | 571,042p | 440,241p | 229,925p | 554,455p | 156,218 |
| `flood` | 104,901p; 51,122p; 55,141p; 115,896p | 565,000p | 437,518p | 229,325p | 580,944p | 156,116 |
| `strike` | 115,525p; 76,249p; 130,994p; 127,663p | 571,741p | 443,147p | 204,918p | 571,152p | 155,840 |
| `buyout` | 80,300p; 83,688p; 164,671p; 145,983p | 573,648p | 466,959p | 225,748p | 556,503p | 157,826 |
| `drain` | 115,603p; 69,123p; 142,063p; 132,991p | 570,600p | 438,637p | 230,824p | 561,594p | 156,439 |
| `gift` | 115,440p; 64,971p; 132,374p; 127,816p | 572,762p | 431,708p | 233,168p | 580,549p | 154,764 |

The owners' profit is paid in lumps (each business's month on its own day of it), so a week's sum swings with the
pay days in it: read the weeks side by side with the run without players, never one window's ratio alone.

**What the pace had done.** With it free, `strike`'s stood at 0 the whole month and was 0.143 against 0.196 on day 44;
`flood`'s ran ahead from day 16 (0.179 against 0.089 on day 30), and `gift`'s from day 20 (0.143); `buyout`'s and
`drain`'s moved a day or two. What it moved: `strike`'s hours in the two weeks after (76,205 free against 79,420
held: free, 2.7% under the run without players; held, 1.4% over) and its hands' odd jobs then (5% under free, 4% over
held); `flood`'s butchers' hours after it (631 free, 475 held, 512 without players: round 4's "the same hours" was the
pace). It moved little else: who holds the money, and Gini, within half a percent held and free; the owners' profit
after the flood 4% apart.

`strike`, 44 days (the hands held from day 2 to day 30's dawn), the pace held:

- 26 hands taken; 25 hungry by the first day's end, and 25 or 26 hungry and starving through the month (as before:
  above, "why `strike`'s hands starve"); none the day after they were let go. 26 kept their posts through the month,
  24 by day 44. The Hold paid them 780p a day (30p each, a companion's 6p at a 12p day): 21,840p in the month, against
  the 17,707p the same 26 earned in it without players.
- **The work was done** (round 4 read 1.5% fewer hours as the work undone; that was the pace): 155,840 hours in the
  month against 156,218 (0.2% fewer), the hands' odd jobs 0.7% more, the owners' profit 0.7% more, the keepers'
  wages the same. What the town lost was the service its absent hands gave: service wages 11% down (204,918p against
  229,925p: 25,007p, more than the 26 had earned).
- **The labour market didn't see them gone**, as before: the going rates 60p throughout; Ridgemere's employers' mean
  wage 62.97p on day 29 against 62.96p; hires' slots in the month 17,765 against 17,671, taken 13,893 against 13,904.
- **After:** in the two weeks after, hours 1.4% more (79,420 against 78,350), the odd jobs 4% more, service wages 3%
  less; Ridgemere's posts open 79 on day 44 against 66. Day 44: no one hungry; Gini 0.5569 against 0.5603, the poorer
  half 0.1412 against 0.1376.

`flood`, raw meat (selling days 2 to 15), the pace held:

- The ten sold 8,544 pieces for 105,465p (12.34p each, the shops' own cost) to the 39 capital shops that buy it in
  (the two that make it, the butchers, aren't sold to), and kept every coin: 106,465p with their grants on day 30.
  The shops' shelves of it held 1,180 to 1,559 through the flood, 395 two days after, about 260 to 400 after.
- **No shop's price fell.** The shops that buy it in asked, before the penny, 14.02p on the first day and 14.36p on the
  last, then 14.41p rising to 15.59p by day 29 (prices rose without players too: shops buying it as materials paid
  16.13p in those two weeks and 16.49p after). What moved was where residents bought: 3,139 pieces through the flood
  against 603 without players, from those shops at about 14p rather than from the butchers at their cost of making
  it, so the mean paid was 14.31p against 17.03p; in the 16 days after, 607 pieces at 17.08p against 816 at 18.31p
  (fewer, and still a little cheaper by the mix: no price came back, because none had gone). Residents' food as a
  whole: 0.997, 0.984, 0.993 and 0.990 of its cost without players, week by week.
- **The makers lost the fortnight's trade, then had it back.** The capital's two butchers sold 1,916 pieces of
  everything (28,024p) through the flood against 4,044 (62,189p), half; in the 16 days after 3,650 (53,863p)
  against 3,682 (57,846p), 1% fewer and 7% less taken, working 7% fewer hours (475 against 512).
- **The money left the shops for good**: the owners' profit 10% and 28% lower in the flood's two weeks, 60% lower the
  week after it and 6% the week after that (the pay days: the four weeks above), 34% lower over the 16 days after
  (177,268p against 270,101p); on day 30 households 100,818p poorer, the tills 21,717p richer (shops' money kept back
  from their owners), the players 106,465p. Gini 0.5325 against 0.5521: the owners, richer households, lost most.

`buyout` (buying days 2 to 8, on the hour from 6 to 20), the pace held:

- The five spent 150,470p (their 150,000p and grants) on 10,968 pieces of food over the week, about 21,000p a day:
  1,107 pieces the first day, then 2,032, 2,217, 2,700, 760, 949 and 1,203 (30p left), and held them all to the end.
- **The corner failed: the makers answered.** At each day's end the capital's shops held (`towns.ridgemere.food`,
  both sides) 3,474 pieces of food against 3,578 on day 2, the first; more than without players on every day from 3
  to 14 but day 8 (5,395 against 5,545), at most 7,424 against 6,210 on day 9; and 2% to 11% less from day 15 on
  (9,523 against 10,418 on day 20, 10,553 against 11,823 on day 27). The capital crafted 22,528 pieces of food in
  the buying week against 10,159, then as without players (15,792 against 15,663; 14,463 against 14,412).
- No one went hungry on any day; residents short of food 0 to 8 a day against 0 to 7. Residents' food cost 1.004,
  1.002, 0.999 and 1.000 of what it did without players, week by week; all they bought 1.000 to 1.004.
- The 150,000p came from the capital's treasury and stayed in the tills (152,061p richer on day 30: the shops', the
  farms' and the Houses'); households 1,119p richer.

`drain` (Frosthollow, 30 bodies at its 15 benches), the pace held:

- As round 4 found (question 8): the players were paid 52,136p by the beat in 30 days (3,810p the first day), from
  the purses of the wolves beside whom they stood, most of them keeping house. On day 31 Frosthollow's 56 residents
  held 47,879p against 91,853p, their median purse 422p against 1,677p; 4 short of food and 3 broke against none;
  none hungry.
- Its hires (slots summed, days 2 to 29) 815 against 953, taken 655 against 757; its residents' wages 828p a day
  against 771p. The realm's hours within 0.2% of the run without players.

`gift` (100p a day to each of the 50 poorest households, 150,000p in all), the pace held:

- All given by day 31: 1,500 gifts of 100p. **No inflation**: what residents bought cost 1.000, 0.997, 0.997 and 0.996
  of what it did without players, week by week (their food 1.000 to 0.991); they bought 3.2% more in the last week
  (24,787 pieces against 24,017) and spent 4.4% more.
- The poorest tenth's purse 160p on day 30 against 82p; the median 1,527p against 1,418p; Gini 0.4990 against
  0.5521, the poorer half 0.1856 against 0.1501. Households 91,688p richer, the treasuries 139,001p poorer (the
  giver's money came from the capital's). The mint minted 120p less than without players (5,000,000p against
  5,000,120p, as in round 4); money conserved in every record.

**For the user** (7, 10 and 11 reworded where they stand, on the runs with the pace held; 1 answered with what
it would take; 12 new):

**Question 1** (should a Hold's hands and a party's companions eat?), with what it would take (above, "why `strike`'s
hands starve"). Recommendation: a companion eats what it carries, and a Hold feeds its stationed hands from its
stores at dawn as the watch feeds its own, letting go one it can't feed; not their own purses, which a stationed hand
can't take to a shop and which its wage barely covers.

12. **Whether to keep the pace held in the long runs with players.** Held, a run with players is measured against the
    run without them by what they did; free, it shows what they would do to the realm's pace (`strike`'s starving
    hands held it at 0 a month). Recommendation: hold it to set a scenario beside the run without players, and run it
    free as well where the pace is the question; once question 1 is settled the two should mostly agree.

10. **A flood's money leaves the shops for good** (`flood`, and `hoard` before it): players who sell and never spend
    take it out of circulation, a third off the owners' profit two weeks after. Recommendation: nothing in the
    economy (it is what a player does); but the mint's rule (doc 64, Phase 11) is the place to answer it, if any.
11. A food corner fails at once (`buyout`): the makers make more, no one goes hungry, the price barely moves. Is that
    the intent (it is the model's), or should a buyout of a town's food make it dearer for a while?

### Phase 13: the roads and the walking, reviewed (2026-10-26)

What the reviews of the second review's mends and of the stage's found, mended: the roads, the old pace's going rate,
a missed piece's seller, and the walking on the stage.

**The roads** (`RatwTrade.cpp`):

- **What a buyer refused is counted** (`StandingOrder::unpaid`, saved with the orders, bounded; an older save's none).
  Goods refused for the buyer's want of money had stopped counting as the seller's shortfall, and nothing else counted
  them, so an order whose buyer never paid never ended: a smith whose till was empty every day kept an order of eight
  bars a week for 42 days and six caravans, nothing handed over, nothing short, looked at again each month, holding
  Frosthollow's bars and its town's want of iron (no other shop there could sign for it), the trader buying a load for
  it every week. Now two weeks' worth refused makes it due at once; agreed again, it is brought down to its share of
  what came that was paid for, and with none paid for it ends, its seller not blamed (no `RoadsState::failed`): it may
  sign again with the same town. (It did, the next day, still penniless, and every fortnight after: nothing at the
  signing looked at the buyer's money. Mended in "the roads and the walking, round 5", below.)
- **The seller is judged by what came**, handed over or refused: under half of what it was to send, the order halved;
  none in two weeks' worth, ended and the seller failed, as before. A raise wants three quarters of all it was to send
  handed over and paid for.
- **What was refused is worked out once every order is served:** what the wagon still holds of a good, among the orders
  whose buyers' money ran out (the hand-over says so), in the order they were signed. Counted as each order was
  served, it took in what the orders after it were still to have, and hid the seller's own shortfall: nine bars for
  sixteen, the first buyer penniless, the seller was three short where it was seven.
- `econ_watch`'s `orders.csv` has an `unpaid` column.

**The going rate at the seconds' pace, as it was:** a town whose unskilled shops have nobody working keeps its last
rate, as before the hours (it fell back to the realm's; "as before" above is so now).

**A missed piece's seller** (`wants`): by lot among the shops with a piece of the kind the household's fund runs to, a
shop once, told of that ware (a sword, the cheapest; finery, the dearest under the fund, as it buys). A 40p fund told
an armourer of a 90p steel sword, and a shop of three such wares had three lots.

**The walking** (`World::headFor`, `World::heldOff`; the stage review):

- **A pocket two steps ahead:** a way on from a part of the next cell is a seam to another cell, or a door to somewhere
  with a way out of its own (not only back into that part, or back here). A strip whose one door was a cottage's, the
  cottage's only way out, was taken for a way on: crossed onto, no way on found, crossed back, and again. (So a
  walker passing through took it for a pocket and recorded a dead end for everyone, and the cottage's own household,
  routed round it into the body of the cell, where the door can't be reached, stayed out all night: round 5, below.)
- **The best way on only:** a way into the next cell is best of all when it lands in a part the next leg leaves from
  best (into the goal's own part of its cell, before another part of it). Both counted, a crossing into the half of a
  cell whose way on lands in the wrong half of the goal's cell was as good, and the nearest was taken, and walked
  round.
- **A spot no way reaches:** a goal in a part of its cell that no door or seam leads into (Greatly House's room upstairs
  no stair reaches). Its cell is judged by its body on the way there (the way in was taken for one into a pocket, and
  she went out and in again at the street), and, in, she keeps to the nearest tile of the part she stands in, looked
  for again every five seconds at most. She doesn't lie down there (she isn't at her spot); the world's data is the
  user's question below.
- **Held only by a body, and only a resident:** a walker steps aside or drops a turn only when another body pressed on
  it in the last second (`Entity::pressedAt`, set by `separate()`, never saved). A player's path is never touched (its
  turn was dropped when it stood held). On the last leg of its path a resident steps aside only from one walking into
  it, face to face (`pressedByWalkerAt`): third in a queue behind those standing at a spot, more than a tile from it,
  it stepped aside every two seconds, from side to side; now it waits. (The review suggested no step aside at all on
  the last leg; but two face to face on one straight leg, each bound past the other, then hold each other for good, as
  `heldAtATurn` shows.)
- **A door out of sight only:** within reach of a door it can see, a resident tries it, as before; it walks up to it
  only when it can't see it (every failure walked up, a route searched where one try did).

**Measured** (30 days of DEV's export, `--deterministic --hours --threads 3`, against the merged Phase 13 at 3cec19a,
`m3`, its day 30):

| | merged | now |
|---|---|---|
| day 30: hungry, starving | 0, 0 | 0, 0 (and at the run's end) |
| household Gini; the poorer half | 0.5519; 0.1519 | 0.5515; 0.1521 |
| the going wage (realm, Ridgemere); the pace | 60p, 60p; 0.107 | 60p, 60p; 0.125 |
| hours credited, lost | 169,695; 80,735 | 169,297; 80,769 |
| great houses, day 2 to 30 | 1,494,143p to 1,238,233p | to 1,247,518p |
| treasuries; the church | 496,291p to 366,643p; 248,352p to 281,026p | to 368,562p; to 281,057p |
| residents; tills | 2,016,252p to 2,018,125p; 727,036p to 1,062,802p | to 2,011,832p; to 1,057,242p |
| days 15-29 a day: the owners' profit; investment | 12,725p; 16,336p | 12,600p; 16,056p |
| the keepers' wages; hands' odd jobs; service wages | 20,530p; 15,636p; 7,711p | 20,522p; 15,428p; 7,723p |
| standing orders | 8,096p | 7,678p |
| orders on day 30; refused by then | 378; none counted | 374; 596 goods, 38 orders |

- Money conserved every day; digest eb7e378c00fcb458; 0.064 ms a tick on a loaded machine. Eleven orders ended by day
  30 (nine in the merged run).
- **The stage** (10 days from day 2, `visit`'s bodies still and walking, as in "residents near players' bodies"): none
  hungry or starving on any day, money conserved every day; hours credited by day 10 60,590 and 60,730. A tick costs
  the same as the merged binary's, four days of still bodies side by side: 0.224 ms each (the ten-day runs' 0.29 and
  0.43 ms were three runs at once).

**Tests** (each fails with its mend undone; the mutation runner's results are in the round's report):

- `roads_tests`: `whatTheBuyerCantPayForIsntShort` (the eight refused), `aShortLoadIsTheSellersShortfall` (nine for
  sixteen: the second forge its eight, the first refuses the one left, the seller seven short),
  `aBuyerWithoutMoneyEndsItsOrder` (penniless three weeks with a month's review: ended, no failure kept, what was
  refused saved through the checkpoint; paid for a third, brought down to two a week, its refusals cleared; it
  stopped at the end, so couldn't see the order signed again the next day: round 5 runs it on),
  `aFailedSellerIsForgottenInAMonth` (two forges Frosthollow failed: one signs with the mid town, the other finds its
  bars promised; the day the month is out, Frosthollow signs both).
- `crafting_tests`: `aTownKeepsItsRateAtTheOldPace`, `aMissedPieceIsOneItCouldPayFor` (a sword, a dear charm),
  `missedMealsWhereFoodIsMade` (three hungry wolves at a brewery's counter with one meal: no missed meal; this is the
  counter race's only guard, not a second one behind the bare shops' as the second review's report had it).
- `world_tests`: `aQueueStandsStill` (behind one standing on its spot, and third in the queue; a player keeps its turn).
- `stream_tests`: `aStripWithAHouseIsNoWayOn`, `theBestWayOn` (home in two crossings, by the right half),
  `aSpotNoWayReaches` (and she stays in, at the nearest tile).
- Not guarded by a test: the "pressed" gate alone (no case in the tests holds a resident without a body: a wall clears
  its path) and the door's visible test against the stage's "any failure" (only its cost differs).

**For the user:**

- **An order its buyer can't pay for** ends after two weeks' worth refused, or comes down to what was paid for; the
  seller isn't blamed, so the same town may be signed again (a placeholder, as the cut is). (Not "when the buyer has
  money", as this said: nothing looked at its money, and it was signed again the next day. Round 5, below: only with
  a week of it in the till.)
- **The last leg's step aside** from a body walking into it, against the review's none (above).
- **A spot no way reaches** is settled for with the nearest tile; the world build could make sure every resident's spot
  is reachable from its cell's doors (recommended: Greatly House's room upstairs is the one known).

### Phase 13: the roads and the walking, round 5 (2026-10-26)

The roads and the walking (`p13-r4-roads`, above) merged into the price level and the players' third review (`83714a0`),
and its review mended: two majors and the doc's wording.

**The merge.** One conflict in code, the going rate (`Society::tendLabour`): the price level's review had the realm's
rate kept, with no maker to measure, only for a realm founded at its founding day (a village falls back to the
catalogue's); the roads' had every rate kept at the seconds' pace (a town keeps its last, as before the hours). Both
are kept: at the seconds' pace a town keeps its last, and the realm's own is as the price level has it at either pace.
(As merged, the whole table kept at that pace, a village's realm never fell back: `crafting_tests` failed.)

**A buyer that can't pay isn't signed again till it can** (`RoadsState::refused`, "buyer|good" -> the day; saved with
the roads, bounded, an older save's none). The order of a buyer that refused two weeks' worth ended, its seller not
blamed, and nothing at the signing looked at the buyer's money: the same order was signed again the next day, still
penniless, ran two weeks, ended, and was signed again, every fortnight (the review's probe: signed again on days 18,
32, 46 and 60; an order stood 59 days of 63; the seller's trader bought about 1,200p of bars nobody paid for). Now an
order cut or ended for what its buyer refused leaves a mark against the buyer and the good, never the seller; a
marked buyer signs for that good again only with a week of the new order (its price, so many a week) in its till, and
then the mark goes. It isn't forgotten in a month, as a seller's failure is: a month on, a buyer still penniless would
be signed again, and the fortnight's churn come back every six weeks. A mark goes with its till.

**A dead end found by a walker passing through keeps no household from its door** (`World::headFor`). The stricter
pocket (above) let a walker passing through, every way into the next cell landing on a strip whose one door is a
cottage's, record a dead end there for everyone; the cottage's own household, routed round it into the body of the
cell, where the door can't be reached, went out and round again all night (the review's probe: home in 6 s at
`3cec19a`, never after). The review asked for a dead end only for a bare pocket (no door, no seam on); but then the
walker passing through, with no dead end to route round, stood in the cell before the strip for good
(`aStripWithAHouseIsNoWayOn` failed: two crossings, never home). So the dead end is kept for those passing through,
and what lies in and beyond its pocket is kept with it (`deadEndPockets_`: the parts of the next cell its ways land
in, and the cells their doors open on). A walker bound there goes in by the cell the dead end was found from, and by
way of that cell from elsewhere; in the body of the cell beyond, with no way back in to the pocket from a neighbour,
out to a neighbour whose way to that cell doesn't come back through here.

On the review's fixture, Cot (who lives in the cottage) put down in each of six cells, home or not in 3,000 s:

| from | `83714a0`, alone | `83714a0`, after Pip | merged, after Pip | now, alone | now, after Pip |
|---|---|---|---|---|---|
| `c_0_0` (her work) | 6 s | 8 s | never | 6 s | 7 s |
| `c_0_1` | 17 s | 20 s | never | 17 s | 18 s |
| `c_1_1` | never | never | never | never | 20 s |
| `c_1_0`, its body | never | never | never | never | 20 s |
| `c_2_0` | never | never | never | never | 32 s |
| `c_2_1` | never | never | never | never | 32 s |

("After Pip": Pip, who passes through, has found the dead end; at `83714a0` Pip herself never got home round the
strip.) Where nobody has found the dead end, a resident in the body of the cell, or coming to it round the other side,
still can't reach the cottage: the routes are cell by cell and don't know the cell's parts until a walker finds them
out. That was so before the roads' review too.

**A lane whose ways on land on a strip is left for the body of a cell** (`World::headFor`). The visit run with walking
bodies, merged, went hungry from day 9: five children of Ridgemere's North West (Grantham, Lockwood, Vance and Wexley
Houses) visiting Greatly House in the North East came out of its door onto a lane walled off from the body of that
cell, whose ways west all land on a strip of the North West walled off from its body, where their doors are. The
strip has ways on (south, onto a corner of the next cell walled off too), so it is no pocket, and they crossed onto
it; there, with no way to their doors and none out landing well, they took the nearest way, back onto the lane, and
again, all night (2 hungry at day 9's end, 5 at day 10's). Not the roads' review's walking nor this round's: the
same boy put down at that door at `83714a0` does the same (a probe, not in the repo, loading DEV's export: below);
the merged roads only sent him there.
Now a walker in a part of its cell other than its body, whose ways into the next cell land neither where its way on
is nor in that cell's body, where another part of its cell has ways that land well, is in the wrong part of its
cell: out, and in again by that part, by a way landing in the body of a cell in memory that has a way back to it
(the lane's ways south, into the body of the cell below, from which its ways north land in the body of the North
East); with none, on by the way it had, as before (not out by the nearest way that lands well: into a house or a
shed whose only way on is back the way he came). Not from the body of its cell, and not out into a cell not in
memory: so first written, four in the body of Ridgemere's East (the gaol's keeper, a taproom's, a hand at Bridge
Stores and a field hand, bound for the South East) were sent out east into the hills, offstage, hopped back in at
once, and out again, for hours (4 hungry at day 4's end, 1 starving). The five children, each put down at Greatly
House's door at 20:00, are home in 166 to 183 s, by the East, the West and the North West, and up their stairs; at
`83714a0` and as merged, none in 900 s.

**Measured** (30 days of DEV's export, `--deterministic --hours --threads 3`; `83714a0` is p13 before this round, the
merge `ffc416e` is the roads' branch merged into it, unmended; `m3` is `3cec19a`'s, as the earlier rounds compared):

| day 30 | `m3` (`3cec19a`) | `83714a0` | merged | now |
|---|---|---|---|---|
| hungry, starving | 0, 0 | 0, 0 | 0, 0 | 0, 0 |
| household Gini; the poorer half | 0.5519; 0.1519 | 0.5521; 0.1501 | 0.5514; 0.1510 | 0.5514; 0.1510 |
| the going wage; the pace | 60p; 0.107 | 60p; 0.089 | 60p; 0.089 | 60p; 0.089 |
| hours credited, lost | 169,695; 80,735 | 168,826; 81,144 | 169,531; 81,117 | 169,523; 81,108 |
| great houses (1,494,143p at day 2) | 1,238,233p | 1,253,127p | 1,253,973p | 1,253,975p |
| treasuries; the church | 366,643p; 281,026p | 369,499p; 280,610p | 372,990p; 280,771p | 372,994p; 280,770p |
| residents; tills | 2,018,125p; 1,062,802p | 2,039,893p; 1,022,134p | 2,024,907p; 1,032,977p | 2,024,921p; 1,032,958p |
| days 15-29 a day: the owners' profit; investment | 12,725p; 16,336p | 13,968p; 15,334p | 13,977p; 15,691p | 13,977p; 15,691p |
| the keepers' wages; hands' odd jobs; service wages | 20,530p; 15,636p; 7,711p | 20,480p; 15,105p; 8,102p | 20,496p; 15,419p; 8,048p | 20,496p; 15,422p; 8,048p |
| standing orders; unsold, back to the stores | 8,096p; 1,076p | 7,446p; 609p | 7,850p; 421p | 7,851p; 421p |
| orders; refused by then | 378; none counted | 372; none counted | 371; 353 goods, 23 orders | 369; 327 goods, 21 orders |
| an order ended with refusals, signed again | (not counted) | (not counted) | 2, each the next day | none |

- Money conserved every day in each. Digests: `83714a0` `9c4c291d44a5f898`, merged `2fcc588618b372b7`, now
  `a799cb967686aa3b`. The round's own mends change little over 30 days (the merged and now columns): two orders
  signed again the next day, penniless, are not, and 26 fewer goods refused. Against `83714a0` the roads' branch
  leaves the residents about 15,000p (0.7%) poorer by day 30, the tills and treasuries that much richer, with 5% more
  in standing orders and a third less sent back unsold; the Gini and the poorer half barely move.
- **The seconds' pace** (5 days, no `--hours`): digest `d034a6316a8d0cb` at `83714a0`, merged, and now.
- **The stage** (10 days from day 2, `visit`'s bodies still and walking): still, none hungry or starving at any day's
  end, now as at `83714a0` (digest `5bce7f1650277c7a`; hours credited by day 10 60,591, 60,589 at `83714a0`);
  walking, none hungry or starving at any day's end now (digest `d7faa7c66318c280`; 60,368 hours by day 10, 60,323 at
  `83714a0`); merged, 2 hungry at day 9's end and 5 at day 10's (above); with the lane's rule as first written, 4 at
  day 4's and 1 at day 7's (above). Money conserved every day in each.
- A tick costs the same: four days of walking bodies, side by side with `83714a0`, 0.161 ms against 0.162 (day 4:
  0.324 ms each; route planning 0.0094 ms each).

**Tests** (each fails with its mend undone; the mutation runner's results in the round's report):

- `roads_tests`: `aBuyerWithoutMoneyEndsItsOrder` runs on: the refusal kept against the buyer and saved through the
  checkpoint; six weeks more penniless, no order on any day, no failure kept, and the seller's trader buying no load
  for it; with 40p (more than a bar's, under a week's) not signed; with the money, signed again and the mark gone.
- `stream_tests`: `aStripHouseholdGetsHome` (Pip home round the strip first; then Cot home from her work, from round
  the other side, and from the body of the cell beyond, each within 600 s), `aLaneToAStripIsLeftForTheBody` (the
  lane, the strip and the walled corner: Bram home from the lane in six crossings at most; without the mend, back and
  forth between the lane and the strip for 3,000 s). Not guarded by a test: that a walker with no way out leading
  back keeps to the way it had rather than the nearest way that lands well (no fixture told them apart: in the one
  tried, a shed, both went back and forth); and that the rule keeps out of the body of a cell and off a way into a
  cell not in memory (the walking run is their only guard: without both, the four in the East above).
- `crafting_tests`: the price level's village falling back, and the roads' town keeping its rate at the seconds' pace,
  both now (each fails with the other's form of the merged line).

**Left:**

- A pocket nobody has found: as above, a resident coming to it from the wrong side doesn't get in.
- The walking is still by cell, and a cell's parts only as far as the next cell: a walker in a part of a cell whose
  only ways out lead back to it (the shed above) goes back and forth. The lane's rule mends the one case the runs
  found; a search across cells and their parts (the stage's cells in memory) would mend the kind. Recommended next,
  if the stage runs find another.
- From the roads' review, unchanged: the "pressed" gate alone and the door's visible test have no behavioural test; a
  resident settled at the nearest tile stands rather than sleeps; the carrier's unreachable market point; moveOffstage's
  own pocket check.

**For the user:**

- **A buyer's refusal is kept till it can pay a week** of the order (recommended), against the review's other way, a
  cash check at every signing (a week's money for every order, a buyer that never refused included: it would change
  who signs across the realm, not only those who couldn't pay). A week is a placeholder, as the cut is.
- **The dead end kept for those passing through** (recommended), with what lies beyond it kept for those bound
  there, against the review's "only a bare pocket" (which strands the walker passing through, above).
- **The seconds' pace's going rate, merged:** a town keeps its last, the realm's falls back for a village (both
  reviews' rules; recommended). The other way is one rule for both.

### Phase 13: money by holder, and the wage bid (2026-10-26)

**The question.** At the 60p founding (with `costAtPace` off) the realm is fed and its money conserved, but far from
the bands above: the household Gini 0.56 (band 0.33–0.45), the poorer half 0.14 (0.18–0.28), and a wage that never
leaves 60p. The price level's review found money leaving the founding holders. Two things to find: where the money
goes, and why the wage doesn't float as the model's does.

**Where it goes** (the merged run `m3`: 60 days of DEV's export at `3cec19a`, `--deterministic --hours`):

| holder | day 2 | day 14 | day 28 | day 42 | day 60 | day 60 / day 2 |
|---|---|---|---|---|---|---|
| great houses | 1,494k | 1,362k | 1,285k | 1,186k | 1,180k | 0.79 |
| the capital's treasury | 309k | 240k | 196k | 167k | 81k | 0.26 |
| the towns' treasuries | 187k | 180k | 159k | 159k | 111k (154k on day 56) | 0.59 (0.82) |
| the church | 248k | 252k | 270k | 295k | 296k | 1.19 |
| farm tills | 183k | 300k | 343k | 291k | 287k | 1.57 |
| shop tills | 490k | 435k | 415k | 502k | 576k | 1.18 |
| House tills | 55k | 153k | 251k | 344k | 373k | 6.8 |
| owners (producers, merchants) | 833k | 925k | 981k | 997k | 1,014k | 1.22 |
| wage earners and the rest | 1,184k | 1,122k | 1,070k | 1,026k | 1,050k | 0.89 |

(The towns' day 60 is a caravan on the road with 38k of their money.) A day, days 15–29:

- **The Houses** (−6.2k a day): money for the road 4.0k against trade takings 2.9k; into their businesses' tills 3.3k
  against their profit 0.5k back; materials for their works 3.7k; their crews 1.5k; rent in 3.4k.
- **The capital** (−3.8k a day): its posts' wages 3.6k and the watch's 0.9k, its buyers' funds 2.8k (the Town Works,
  the watch, the docks, the hall), against its tax 3.6k. It pays about 95 posts as the payer of last resort (`payerOf`'s
  "the town": a post with no shop, House or household to pay it): 24 live-in servants "in service to" a household,
  10 gardeners, 11 keepers of this and that, 8 carriers, 5 pupils at their lessons, clerks, messengers, washers and
  cooks. At 12p its tax carried them (the base's capital holds level); at 60p they cost more than its tax.
- **The tills** fill toward their working money (a month of their running at 60p: 2.7M wanted on day 60, 1.24M held),
  from sales kept and their owners' top-ups (16.3k a day against 12.7k of profit out).

**The model, founded the same way** (`tools/econ_model/model.py` with its founding wage at 60p and 4,090,000p, the
game's money a head for its 885 people; `wage0` set to 60 in `World.found`, nothing else changed):

| day | wage | Gini | poorer half | homes | firms | Houses | church | treasury |
|---|---|---|---|---|---|---|---|---|
| 2 | 60p | 0.477 | 0.172 | 1,636k | 607k | 1,236k | 204k | 407k |
| 28 | 65p | 0.425 | 0.207 | 1,829k | 453k | 1,122k | 241k | 446k |
| 56 | 77p | 0.381 | 0.242 | 2,057k | 415k | 897k | 270k | 452k |
| 112 | 97p | 0.330 | 0.270 | 2,415k | 564k | 460k | 309k | 343k |
| 168 | 138p | 0.341 | 0.245 | 2,420k | 929k | 213k | 230k | 298k |
| 364 | 137p | 0.403 | 0.204 | 2,412k | 1,033k | 247k | 169k | 228k |
| 1092 | 122p | 0.412 | 0.203 | 2,574k | 847k | 291k | 161k | 217k |

(With all of 5,000,000p for its 885, the wage reaches 160p to 180p in the first year.)

- **The Houses' fall is the model's.** Its Houses spend by their rule (a share of what they hold over four months'
  income) and go from 30% of the money to 7% by year 3: 0.73 of their founding by day 56, 0.17 by day 168. The game's
  are at 0.78 on day 56, slower. Not a fault, and not to be held "steady": doc 64's band is each kind within 20% of its
  year-3 share, not its founding.
- **The tills' rise is the model's.** Its firms sit at a quarter of their working money, and swell from 15% of the
  money to about a quarter as the wage rises (1.53 of their founding by day 168, 25% by day 364); their owners put in
  more than comes out in the first months (2.1k to 5.3k a day to day 196, against 0.03k to 6.6k of profit). The game's
  owners' rule is the model's (`owner_tops_up`, `firm_month`); its tills swell sooner (the Houses' businesses from
  floats a tenth of their working money).
- **The capital's fall is not.** The model's treasury is up a tenth on day 56 and steady at 5% from year 1: it pays its
  watch and its works crew out of its week's budget (`staff_to`), and lets them go when the budget falls. The game's
  capital pays its posts every spell whatever its budget, and its buyers outside the budget too: it is dry by day 90
  (the 120-day run below: 18k). Then its buyers get nothing (their funds stop under its payroll reserve) and its posts
  take the week's tax (+0.2k a day, days 95–119). **Not mended here** (for the user, below).
- **The households' gain is the model's lever, and the game's misses it.** The model's homes gain 26% by day 56 and 48%
  by day 168, the Houses' fortunes paid out as wages (twelve retainers and an estate crew of up to a tenth of the
  realm's workers each, always to be had), and its Gini falls from 0.48 to 0.33. The game's households hold level: wage
  earners −11%, owners +22%. The Houses' crews (twelve retainers and twelve of their estate each, at the going rate) go
  about three-quarters untaken.

**Why the wage stood at 60p.** The model's wage floats on a shortage of hands: from day 28, 182 posts open against 410
workers (the institutions' crews, sized to their budgets, and the firms' hands at the calibrated hours). At the game's
pace (0.1 to 0.2) there is none: a day's hires are taken by some 240 hands whose own business has no work (their banks
full), 100 children, 60 drawn from their posts by better pay and 45 without a post, and every business's hire fills.
Only the land's far hires and the Houses' crews stand empty. So, rule by rule against `model.py`:

- A business's posts were its hires' untaken slots and its empty authored posts; the model's are the hands it wants
  less those it has. A business that couldn't post a hire (one at a time, eight slots, its till short) stood short with
  nothing open.
- The going rate is the model's (`market_wage`: the middle of the unskilled makers' wages, by their hands; the farms'
  harvest pay follows it). With most makers not short, the middle never moves, however short the land.
- A fishery's, a quarry's, a mine's or a logging camp's hires were for its own town's folk only (a farm's, for anyone
  from a city). The model has one market. Traced (40 days, `RATW_R4TRACE`, not kept): the land's shortfall was fishing
  (20 to 40 hands), quarries (20 to 45), logging and mines, not the fields.
- The pace held whenever the land's hands were 5% short, so it stopped at the first shortage, where a wage would begin
  to be bid. And on many of the other days it held for the week's food falling by a few hundredths: of 33 weighed days,
  9 held on the food alone, falls of 0.002 to 0.12 days a head (with 3.5 in hand); 6 on the land's hands alone and 3 on
  both; 11 went on and 4 stepped back.

**What changed** (with work by the hours only; the seconds' pace is as it was: 5 days of it, the same digest
93ddacf5c1f9ce40 and the same `days.jsonl` and `flows.csv` before and after):

- **Hands wanted and not had are posts open** (`tendLabour`): a business's hands wanted (two and twice its hands at
  most, as its review hires), less its hands and those on its hires, count as posts open though it couldn't post a hire
  for them; a day open counts toward its review's raise. Not a business with no work for more hands. (Corrected by
  the review: it counted a producer's hands wanted with the half again of a shelf near bare, which a producer's always
  is, so a producer stood half again short for good. Since round 5 a producer's are its sales as they are. And a hand
  is never drawn off from an employer with a post open, the model's rule, so with more employers short, fewer hands
  are drawn off: "better" moves are counted in round 5's section below.)
- **The land's hires for anyone from a city** (`farmHire`): any producer's, not a farm's only. One labour market, as
  the model's. (Corrected by the review: this was given as the answer to the land's shortage, but nothing here
  measured the land's hires after it. Round 5 measured them: alone, at the same pace, it raised the land's takes 2%
  and cut its hours 1%. The land's takes and hours fall against `m3` with the pace, not with this: see round 5.)
- **The week's food "didn't fall" within `PaceNoise`** (a third of `PaceSlide`, 0.05 days a head).
- **The land's hands no longer hold the pace** (a commit of its own, after the three above, to be kept or dropped
  alone). The land's shortage is its posts open now, which its weekly review answers by its wage; the food, falling
  as the land falls behind, steps the pace back, and hunger faster, as before. The daily count of the land's hands
  wanted goes (`PaceShortHands`). (Round 5: a loose brake in the hold's place, the land's hours worked week on week;
  see below. The review: with no brake but the food, the pace climbed to 0.64 while the land's hours fell.)

**Not changed:** the fields' wages stay out of the going rate, their harvest pay a quarter over their own (the model's
farms are left out of `market_wage` too, their pay following it).

**Tried, not kept** (switches in a trial build, gone):

- The institutions' crews ranked with the businesses' hires by pay, ties by lot: no change, the crews still went
  untaken.
- The crews first, before any business's hire, and a House's estate crew up to 48: the crews' pay 3.6k to 5.9k a day,
  the Houses 0.73 of their founding by day 60, the wage floating to 62p; but the Gini 0.566 and the poorer half 0.132,
  no better.
- The capital's own posts kept to its week's budget, half the excess let go a week: its treasury 114k on day 60
  against 81k, 38 posts let go by day 56 and more each week. Its budget falls with its cash while its buyers' funds stay
  outside it, so it doesn't settle.

**Measured** (60 days of DEV's export, `--deterministic --hours --threads 3`; every run conserved every day; every one,
the merged run's too, has its one hungry day on day 46, none starving; one run's end record has one more):

| day 60 | `m3` | posts open, one market, the noise | and the land's hold lifted |
|---|---|---|---|
| household Gini; the poorer half | 0.562; 0.136 | 0.558; 0.139 | 0.574; 0.132 |
| the wage (the realm's, Ridgemere's) | 60p | 60.4p (60.8p on day 56) | 63.3p |
| pace | 0.196 | 0.214 | 0.375 |
| hungry on day 46 | 18 | 9 | 14 |
| bread, a bronze sword, a rapier (LD) | 0.072, 2.50, 25.0 | 0.077, 2.49, 25.0 | 0.073, 2.42, 25.0 |
| great houses; the capital; tills | 1,180k; 81k; 1,236k | 1,178k; 85k; 1,217k | 1,175k; 84k; 1,229k |
| the owners' profit against six kinds of wages, days 45–59 | 0.23 | 0.16 | 0.13 |
| a tick (three runs at once on a shared machine) | | 0.050 ms | 0.049 ms |

By week (the household Gini, the poorer half, the wage, the pace):

| day | `m3` | posts open, one market, the noise | and the land's hold lifted |
|---|---|---|---|
| 14 | 0.535, 0.151, 60p, 0.02 | 0.536, 0.149, 60p, 0.02 | 0.536, 0.149, 60p, 0.02 |
| 28 | 0.545, 0.154, 60p, 0.09 | 0.543, 0.154, 60p, 0.18 | 0.543, 0.156, 60p, 0.23 |
| 42 | 0.557, 0.140, 60p, 0.20 | 0.553, 0.148, 60p, 0.30 | 0.562, 0.142, 60p, 0.43 |
| 49 | 0.565, 0.133, 60p, 0.14 | 0.567, 0.136, 60p, 0.21 | 0.574, 0.132, 60p, 0.27 |
| 56 | 0.562, 0.137, 60p, 0.14 | 0.559, 0.139, 60.8p, 0.21 | 0.576, 0.131, 61.8p, 0.30 |
| 60 | 0.562, 0.136, 60p, 0.20 | 0.558, 0.139, 60.4p, 0.21 | 0.574, 0.132, 63.3p, 0.38 |

And with all four, 120 days: the wage 65p on day 70, 70p on day 84, 73p on day 98, 78p on day 120 (the model's: 77p
by day 56, 97p by day 112); the pace 0.29 to 0.64; nobody hungry after day 46; the Gini 0.582, 0.577, 0.577, 0.586 and
the poorer half 0.123, 0.121, 0.114, 0.105; the Houses 0.69 of their founding by day 120; the capital dry from day 90;
shop tills 911k (1.86). (Corrected by the review, which found the run's other tills left out: the farm tills 295k on
day 90 and 184k on day 120, down 38% in a month (`m3`'s 287k on day 60), the House tills 342k; the owners' profit
6.5k a day over days 45–59 against `m3`'s 11.1k, with the six kinds of wages barely moved (49.1k against 47.7k), so the
float raised the tills' share, not the households' pay. The land's hours a week (its producers' but the fields')
16.8k at their lowest on day 49, 20.5k on day 60 and 23.6k by day 120, against `m3`'s 18.9k and 21.7k on days 49 and
60.)

- **The wage floats once the pace may rise past the land's first shortage**, slower than the model's (63p on day 60
  against its 77p by day 56; 78p on day 120 against its 97p by day 112), and without the hold the realm is fed as
  before (each run's day-46 dip, then nobody). With the hold, in every run it stood at 60p to day 49, and reached 62p
  at most by day 60 (with the crews first).
- **The Gini goes the wrong way while it does.** As the pace and the wage rise, every till's working money rises (more
  hands wanted, at more a day), and the tills keep what the households spend on food (the wage earners' food 10% dearer
  by days 45–59, their pay from the land's hires 14% lower) until they hold it; the Houses' money, which in the model is
  paid out as wages, goes here into their businesses' tills, caravans and materials. The poorer half falls from 0.15 to
  0.11 in 120 days.
- **Still out of band:** the Gini and the poorer half (above); bread in band (0.07); the bronze sword (2.4 to 2.5) and
  the rapier (25) in band by construction (store-priced, and no rapier sold); the capital's treasury; and (the review)
  the farm tills, draining from day 90, a risk of lifting the hold that only a year's run (a winter and a harvest)
  can clear.

**What a year would need to show:** the wage settling (the model's: 130p to 140p by day 168, then 120p to 135p for two
years), the pace reaching 1 and staying fed through a winter and a harvest, the tills' share turning down once the wage
stops rising (the model's firms: about a quarter at the float's top, 14% to 21% from year 2), the Houses at their rule's
level, and the Gini turning: which, by the reading above, needs the Houses' and the church's budgets to reach the
households as wages, as the model's estate crews do. And the capital: what it pays, and for whom (for the user).

**The smoke** (30 days, `build/econ_watch` at the second commit, `--deterministic --hours --threads 3`): conserved every
day, nobody hungry; digest 22c5e0263d62fc15; its `days.jsonl` the 60-day run's day for day. Day 30: the Gini 0.548
(`m3` 0.552), the poorer half 0.156 (0.152), the wage 60p, the pace 0.268 (0.107); 0.034 ms a tick alone.

**Saves:** nothing new. A business's days open were saved already (`memory.employers`); `PaceShortHands` was never
saved.

**Tests** (each fails with its change undone, checked one at a time):

- `crafting_tests` `aShortBusinessBidsUp`: a bakery whose bread sells for more hands than its baker, its till and its
  baker's purse emptied each evening so it posts no hire: its posts are open on all 15 days, and its review raises
  its wage (before: none open, no raise).
- `crafting_tests` `theLandsHiresForAnyone`: a fishery's hire is the land's with work by the hours (a farm's at both
  paces, a bakery's at neither).
- `roads_tests` `thePaceIsSteadied`: the week's food down a fortieth of a day a head, the pace goes on; down a tenth,
  it holds.
- `roads_tests` `thePaceStepsUpWithTheLand`: its sales three hands' days with the farm's one, the pace goes on and the
  farm's posts stand open (before: it held). (The review: its case of no work for more hands checked only that the
  pace went on, which with the hold gone it always does; the exclusion of such a business from posts open, the
  subtraction of those on its hires and the cap of its hands and two were each untested. Round 5's tests cover them.)

**For the user:**

1. **Lift the land's hold on the pace?** Recommended: yes (its own commit). Without it the wage can't float, and
   nothing in these runs went hungrier for it. But in 60 to 120 days the Gini rises with the wage, as the tills fill;
   the next step is the institutions' spending reaching wages (2). Dropping the commit brings the hold back as it was.
   (The review: not before a year's run shows the land's hours, its tills and the food holding without it. Round 5
   puts a loose brake in its place; the decision is restated there.)
2. **The Houses' crews.** The model's Houses keep twelve retainers and an estate crew of up to a tenth of the realm's
   workers each, paid out of their budget, and that is where their fortunes go. The game's keep twelve of each at the
   going rate, and they go mostly untaken (the businesses' hires come first, at the same or better pay). Recommended: an
   estate crew sized as the model's, its posts taken as posts are (not after every hire), as the next balancing step.
3. **The capital's posts.** It pays about 95 posts as the payer of last resort, at 60p more than its tax, and runs dry
   in three months. The model's treasury keeps its watch and works crew to its budget. Options: (a) the model's: its
   posts kept to its week's budget and its buyers' funds inside it too, so posts are let go; (b) payers for them: the
   live-in servants paid by their households, the gardeners and keepers by the Houses whose grounds they keep, the
   pupils by nobody; (c) a higher tax (the Dungeon Master's lever: about 16% would carry them). Recommended: (b) for the
   servants and keepers, and (a) for the rest.
4. **The founding wage against the money.** The model founded at 60p with this money floats to 135p to 180p in the first
   year and comes back toward 60p to 75p only over decades (the sixth pass: 13p to 15p at 1,000,000p from year 8). At
   60p the realm holds about twice the money the model's wage would; the float is the realm finding its level. Either
   let it (and the 60p "labourer's day" of decision 1 is the founding's, not the years'), or found at about 120p.

### Phase 13: the wage bid, the review mended (2026-10-26)

The review of "money by holder, and the wage bid" (`p13-r4-balance`), mended on `p13-r5-wagebid`: that branch merged
onto `83714a0` (round 4's price and harness branches; `costAtPace` off, the founding at 60p), then `809d57c` (a
producer's posts by its sales; the tests the review found missing; the harness's count of hires worked and taken from
afar), `99dcd27` (a loose brake on the pace) and `7797bc0` (the harness). The seconds' pace is as it was: 5 days of it
without `--hours`, the same digest d034a6316a8d0cb from `83714a0`'s build and this branch's, and the same
`days.jsonl`, `flows.csv`, `work.csv`, `tills.csv` and `books.csv`.

**The land's hires, measured** (the review's first finding: the land's hires taken down a quarter and its hours down
a seventh against `m3`, put down to city hands walking to distant hires). Days 21–44, the review's land: 85 tills of
fisheries, quarries, logging camps, mines, charcoal burners, clay pits, salt pans, woodcutters and eel traps.
Slot-days are a hire's slots on each day it runs; "worked", takers who had done the day's spells by 23:00 (and been
paid); "from away", takers living in another town than the hire's (both counted since `809d57c`, so only in this
round's runs). `e_o` to `f_omng` are round 4's (`3cec19a` with its changes one at a time: o the posts open, m the one
market, n the noise, g the hold lifted); "merge" is round 4's branch on `83714a0`; the last three are this round's, all
without the brake:

| days 21–44 | slot-days | taken | worked | from away (worked) | taken by the idle, jobless, drawn off, children | hours credited |
|---|---|---|---|---|---|---|
| `m3` | 5,353 | 4,458 (83%) | — | — | 2,451, 369, 611, 992 | 36,256 |
| `e_o` | 5,353 | 4,239 (79%) | — | — | 2,329, 379, 620, 881 | 35,403 |
| `e_om` | 5,263 | 4,337 (82%) | — | — | 2,288, 644, 419, 966 | 35,080 |
| `f_omn` | 4,950 | 3,914 (79%) | — | — | 2,113, 479, 375, 929 | 33,518 |
| `f_omng` | 4,852 | 3,426 (71%) | — | — | 1,772, 477, 363, 786 | 31,480 |
| the merge | 4,538 | 3,367 (74%) | 1,914 | 713 (490) | 1,633, 522, 376, 810 | 32,199 |
| posts by sales | 4,646 | 3,612 (78%) | 2,079 | 653 (450) | 1,782, 580, 421, 800 | 33,371 |
| and lodging | 4,198 | 2,874 (68%) | 1,634 | 829 (518) | 1,384, 141, 437, 907 | 29,712 |
| and a walking range | 4,483 | 3,253 (73%) | 1,746 | 728 (448) | 1,690, 169, 508, 871 | 30,666 |

The mean pace over those days: `m3` 0.12; `e_o` and `e_om` 0.16, `f_omn` 0.21, `f_omng` 0.29; the merge 0.31, posts by
sales 0.27, lodging 0.31, the range 0.28. By town (slot-days, taken, hours credited):

| town | `m3` | `e_o` | `e_om` | `f_omn` | `f_omng` | the merge | posts by sales | lodging | a range |
|---|---|---|---|---|---|---|---|---|---|
| frost_valley | 367, 284, 2,886 | 328, 274, 2,800 | 303, 289, 3,034 | 255, 223, 2,852 | 272, 235, 2,709 | 323, 266, 2,902 | 297, 265, 2,898 | 304, 239, 2,804 | 218, 198, 2,582 |
| frosthollow | 0, 0, 652 | 0, 0, 625 | 0, 0, 490 | 0, 0, 473 | 0, 0, 388 | 0, 0, 510 | 0, 0, 547 | 0, 0, 562 | 0, 0, 524 |
| glasswater | 428, 418, 3,630 | 466, 450, 3,674 | 421, 413, 3,737 | 407, 400, 3,448 | 417, 388, 3,389 | 405, 380, 3,450 | 388, 380, 3,433 | 420, 385, 3,456 | 382, 372, 3,139 |
| greyteeth | 128, 70, 996 | 36, 36, 808 | 6, 6, 642 | 6, 6, 641 | 0, 0, 641 | 12, 12, 714 | 13, 12, 692 | 37, 25, 867 | 3, 1, 637 |
| rainward_hills | 1,941, 1,571, 9,831 | 1,949, 1,479, 9,489 | 1,944, 1,532, 9,283 | 1,961, 1,466, 9,193 | 1,999, 1,275, 8,496 | 1,938, 1,351, 9,013 | 1,715, 1,199, 8,572 | 1,643, 961, 7,521 | 1,636, 1,071, 8,100 |
| ridgemere | 868, 735, 5,696 | 932, 581, 5,026 | 840, 627, 5,294 | 738, 500, 4,747 | 738, 448, 4,475 | 479, 242, 3,717 | 795, 549, 5,224 | 554, 295, 3,946 | 594, 326, 3,789 |
| ridgemere_sound | 1,083, 847, 4,549 | 1,127, 904, 5,190 | 1,245, 975, 5,003 | 1,089, 834, 4,802 | 898, 576, 3,847 | 919, 654, 4,292 | 940, 721, 4,289 | 603, 346, 2,553 | 1,032, 671, 3,890 |
| sallow_fens | 464, 459, 4,768 | 462, 462, 4,780 | 479, 470, 4,835 | 475, 466, 4,635 | 493, 469, 4,691 | 415, 415, 4,727 | 462, 450, 4,840 | 582, 568, 5,027 | 562, 558, 4,986 |
| warmwood | 74, 74, 3,248 | 53, 53, 3,011 | 25, 25, 2,762 | 19, 19, 2,727 | 35, 35, 2,844 | 47, 47, 2,874 | 36, 36, 2,876 | 55, 55, 2,976 | 56, 56, 3,019 |

- **The review was right that the land's hires and hours fell against `m3`** (posts by sales: takes 19% fewer, hours
  8% fewer), **but the fall is the pace's, not the road's.** The runs fall in the order of their pace. Over half the
  land's takers are idle hands, whose own business has no work (its work in hand full): 2,451 of `m3`'s 4,458. As the
  pace rises their own businesses have work again, and the idle take 1,633 to 1,782; that is most of the fall (`m3` to
  posts by sales: 846 fewer takes, 669 of them the idle's). It is the labour market tightening, where a wage is to be
  bid, not hands lost on the way.
- **The one market alone** (`e_o` to `e_om`, about the same pace): takes up 2%, hours down 1%. The land's jobless
  takers rose (379 to 644) and those drawn off their posts fell (620 to 419): the review's "city hands displace local
  ones", within a percent of the same hours.
- **Hands from away work their hires more, not less.** Of the slot-days taken by hands from another town, 62% to 69%
  were worked by the day's end, against 55% to 58% of the rest. (Corrected by the review, round 6: the rest's own are
  51% to 55%, the merge 1,424 of 2,654, posts by sales 1,629 of 2,959, lodging 1,116 of 2,045, the range 1,298 of
  2,525; 55% to 58% is everyone's. And the two aren't the same takers: those from away are mostly the jobless and the
  idle, the locals children from noon and hands drawn off their posts as well, so the gap may be who takes them, not
  the walk. `hires.csv` now finds a taker's town as the hire's own is found, at the sampling, not from where it began.)
  (Why little more than half of the slot-days taken are
  worked by 23:00 anywhere, the farms' too: hands who take a hire late in the day, children from noon, and those whose
  own post has work and who hold the hire meanwhile. Not mended here: the next labour-market measure.)
- **Lodging and a walking range, tried and not kept** (switches in a trial build, gone). Lodging: every producer of the
  land given a bunkhouse as a farm is (its site's, "... at" or "... off <site>", or its town's nearest its work: the
  export has them at the quarries, the mines, the clay pit, the logging camps and the fisheries off Greywater,
  Glasswater and Eelwater, but only farms' were found). At the merge's pace (0.31) it worked the land 8% fewer hours
  than the merge, and 11% fewer than posts by sales; Ridgemere Sound's fisheries took half the hires (346 against
  721), their tills feeding the lodgers (a day's food each, their own fish first). The range (a producer's hire open to
  a city's hands only within 700 tiles of the city's homes: the Rainward Hills, Glasswater, the Sound and the Warmwood
  in; the Frost Valley, the Greyteeth and the Sallow Fens out): 8% fewer hours than posts by sales. Both took the
  city's jobless off the land (141 and 169 slot-days taken, against 580). The land stays one market, as the model's
  (which has no distance), and a city hand on a distant hire walks there from home.
- The end-to-end test the review asked for: a city's idle wolf takes a distant fishery's hire, walks there, works the
  day and is paid, its hours credited to the fishery's work (`aCityHandAtTheFishery`).

**What changed:**

- **A producer's posts open by its sales** (the review's second finding; `tendLabour`): its hands wanted without the
  half again of a shelf near bare (`handsWanted(key, false)`, as the old hold measured them). Its goods go on to the
  shops, so its shelf always is near bare, and it stood half again short for good, bidding its wage up for hands its
  sales never wanted. A shop's are as they were (the model's half again near bare is graded,
  `1 + 0.5 (low - days) / low`; the game's isn't, for the shops either: not changed here).
- **A loose brake on the pace** (the review of a7500b4; `tendLabour`): each turn of the day reads the land's hours
  worked since the last (its producers' but the seasonal fields', whose work is the calendar's; their hands' are
  credited to them) into `paceLand`, a fortnight's readings each with its day. The pace holds, the food holding, while
  the last seven turns' hours fell by over `PaceLandSlack` (5%) against the seven before; under a fortnight of
  readings, or a gap (a restart, an older save), and the food alone decides. The model has no pace to borrow a rule
  from: its hours are the calibrated from its first day. It isn't the old hold, which stopped the pace at the first
  shortage of hands, wanted against had, every day: this holds it only while the land's work actually falls. In the
  120-day run the land's week was down by over 5% on 24 of the 107 days it could be weighed (about days 18–19,
  29–35, 46–52 after the day-46 dip, 64, 78–80 and 90–93); on those the pace couldn't go on.
- **Poaching never draws from an employer with a post open** (the review's fourth finding: documented, not changed).
  `lookingFor` lets a paid hand look for a better-paid hire now and then only if its employer has no post open and
  other hands besides it (the model's `poach` skips employers short of hands too). With posts open counted for every
  short business since round 4, fewer employers can be drawn from. Hires' slot-days taken by hands drawn off their
  posts ("better" in `hires.csv`), by 30 days: `m3` 1,596 and 2,050 (days 1–30, 31–60); round 4's 120-day run 1,498,
  1,726, 2,408, 2,362; the merge 1,626, 1,818, 2,555, 2,264; posts by sales 1,493, 1,832, 2,461, 2,344; with the brake
  1,468, 1,776, 2,647, 2,264. Fewer than `m3`'s in the second month (by an eighth or so), then more as the wage
  floats.

**120 days** (DEV's export, `--deterministic --hours --threads 3`, three at once on a loaded machine; every run
conserved every day). Round 4's is `e_omgn120` (its four changes on `3cec19a`); the merge, round 4's branch on
`83714a0`; then posts by sales, and with the brake: the branch as committed. (The last two were run with a harness that
also asked every resident's payer each day, since taken out: it fills `payerOf`'s day's cache of a household's
richest at another moment, and the runs part from the same code's run without it on day 2, as a run with another
seed would. The smoke below is the committed harness.) (Corrected by the review, round 6: so the merge against posts
by sales and the brake are single runs on two harnesses, and their differences of the size two seeds would give; the
one the code makes is the brake's lower pace in the second month. The cache is mended in round 6: asking leaves a run
as it was.)

| day: Gini, poorer half, wage, pace | `m3` | round 4 | the merge | posts by sales | and the brake |
|---|---|---|---|---|---|
| 14 | 0.535, 0.151, 60.0p, 0.02 | 0.536, 0.149, 60.0p, 0.02 | 0.536, 0.148, 60.0p, 0.02 | 0.536, 0.149, 60.0p, 0.02 | 0.536, 0.149, 60.0p, 0.02 |
| 28 | 0.545, 0.154, 60.0p, 0.09 | 0.543, 0.156, 60.0p, 0.23 | 0.545, 0.154, 60.0p, 0.25 | 0.543, 0.155, 60.0p, 0.25 | 0.545, 0.153, 60.0p, 0.21 |
| 42 | 0.557, 0.140, 60.0p, 0.20 | 0.562, 0.142, 60.0p, 0.43 | 0.559, 0.143, 60.0p, 0.43 | 0.556, 0.145, 60.0p, 0.46 | 0.555, 0.144, 60.0p, 0.36 |
| 49 | 0.565, 0.133, 60.0p, 0.14 | 0.574, 0.132, 60.0p, 0.27 | 0.570, 0.134, 60.0p, 0.25 | 0.572, 0.132, 60.0p, 0.32 | 0.566, 0.136, 60.0p, 0.29 |
| 56 | 0.562, 0.137, 60.0p, 0.14 | 0.576, 0.131, 61.8p, 0.30 | 0.568, 0.133, 61.8p, 0.32 | 0.573, 0.129, 62.1p, 0.34 | 0.570, 0.132, 60.9p, 0.29 |
| 60 | 0.562, 0.136, 60.0p, 0.20 | 0.574, 0.132, 63.3p, 0.38 | 0.566, 0.135, 63.1p, 0.39 | 0.569, 0.133, 64.4p, 0.41 | 0.566, 0.135, 61.8p, 0.36 |
| 70 | | 0.582, 0.123, 65.4p, 0.29 | 0.584, 0.120, 66.1p, 0.32 | 0.587, 0.120, 65.5p, 0.30 | 0.582, 0.121, 65.0p, 0.32 |
| 84 | | 0.577, 0.121, 69.6p, 0.30 | 0.576, 0.123, 70.4p, 0.39 | 0.589, 0.117, 67.9p, 0.30 | 0.584, 0.115, 66.8p, 0.41 |
| 98 | | 0.577, 0.114, 73.1p, 0.52 | 0.587, 0.110, 72.9p, 0.38 | 0.588, 0.108, 72.0p, 0.48 | 0.584, 0.109, 71.3p, 0.45 |
| 112 | | 0.582, 0.107, 76.5p, 0.52 | 0.602, 0.097, 78.2p, 0.52 | 0.598, 0.098, 78.4p, 0.50 | 0.589, 0.102, 78.2p, 0.52 |
| 120 | | 0.586, 0.105, 77.9p, 0.64 | 0.615, 0.092, 80.7p, 0.57 | 0.615, 0.089, 81.5p, 0.62 | 0.599, 0.097, 80.5p, 0.66 |

| day: the land's hours the week; farm, shop, House tills | `m3` | round 4 | the merge | posts by sales | and the brake |
|---|---|---|---|---|---|
| 14 | 22.6k; 300k, 435k, 153k | 22.5k; 314k, 431k, 149k | 22.8k; 297k, 441k, 147k | 22.8k; 301k, 436k, 147k | 22.8k; 301k, 436k, 147k |
| 28 | 21.9k; 343k, 415k, 251k | 21.1k; 299k, 429k, 257k | 20.6k; 322k, 416k, 246k | 21.5k; 302k, 423k, 253k | 21.5k; 315k, 408k, 253k |
| 42 | 20.5k; 291k, 502k, 344k | 18.7k; 248k, 558k, 330k | 19.4k; 234k, 564k, 325k | 19.6k; 248k, 543k, 314k | 20.1k; 254k, 526k, 316k |
| 49 | 18.9k; 287k, 558k, 375k | 16.8k; 258k, 611k, 346k | 17.1k; 243k, 617k, 352k | 17.2k; 239k, 604k, 346k | 16.7k; 270k, 577k, 349k |
| 56 | 21.1k; 317k, 552k, 347k | 18.8k; 258k, 627k, 339k | 19.7k; 261k, 627k, 327k | 19.8k; 229k, 627k, 325k | 20.8k; 250k, 607k, 328k |
| 60 | 21.7k; 287k, 576k, 373k | 20.5k; 235k, 663k, 331k | 21.4k; 237k, 669k, 325k | 20.7k; 216k, 655k, 328k | 20.9k; 225k, 634k, 345k |
| 70 | | 20.1k; 276k, 682k, 383k | 21.3k; 267k, 663k, 331k | 19.9k; 247k, 663k, 371k | 21.0k; 255k, 645k, 359k |
| 84 | | 19.9k; 274k, 723k, 365k | 21.0k; 278k, 692k, 321k | 20.5k; 279k, 657k, 373k | 20.5k; 263k, 649k, 353k |
| 98 | | 22.9k; 263k, 804k, 387k | 21.9k; 308k, 747k, 372k | 21.0k; 280k, 749k, 375k | 21.0k; 235k, 730k, 332k |
| 112 | | 22.1k; 214k, 871k, 342k | 22.8k; 290k, 810k, 387k | 22.4k; 251k, 824k, 402k | 22.4k; 220k, 838k, 331k |
| 120 | | 23.6k; 184k, 911k, 342k | 24.1k; 227k, 840k, 398k | 24.1k; 208k, 851k, 377k | 23.6k; 182k, 881k, 342k |

| day: great houses, the capital, the towns, the church; owners, earners | `m3` | round 4 | the merge | posts by sales | and the brake |
|---|---|---|---|---|---|
| 14 | 1,362k, 240k, 180k, 252k; 925k, 1,122k | 1,361k, 240k, 179k, 250k; 921k, 1,123k | 1,360k, 240k, 178k, 251k; 927k, 1,126k | 1,362k, 241k, 178k, 251k; 926k, 1,125k | 1,362k, 241k, 178k, 251k; 926k, 1,125k |
| 42 | 1,186k, 167k, 159k, 295k; 997k, 1,026k | 1,190k, 167k, 167k, 296k; 971k, 1,039k | 1,190k, 166k, 160k, 298k; 982k, 1,048k | 1,201k, 171k, 172k, 293k; 967k, 1,055k | 1,204k, 173k, 166k, 294k; 977k, 1,056k |
| 60 | 1,180k, 81k, 111k, 296k; 1,014k, 1,050k | 1,175k, 84k, 150k, 299k; 1,006k, 1,022k | 1,198k, 81k, 132k, 297k; 986k, 1,036k | 1,199k, 84k, 137k, 293k; 992k, 1,056k | 1,192k, 90k, 136k, 294k; 1,002k, 1,043k |
| 84 | | 1,099k, 52k, 201k, 305k; 957k, 1,002k | 1,114k, 50k, 210k, 311k; 969k, 1,035k | 1,099k, 51k, 207k, 297k; 967k, 1,053k | 1,120k, 51k, 225k, 304k; 974k, 1,045k |
| 98 | | 1,045k, 34k, 216k, 304k; 928k, 998k | 1,036k, 26k, 220k, 305k; 946k, 1,024k | 1,044k, 32k, 234k, 296k; 932k, 1,041k | 1,091k, 31k, 253k, 305k; 960k, 1,049k |
| 120 | | 1,031k, 25k, 243k, 309k; 946k, 990k | 965k, 25k, 251k, 302k; 981k, 992k | 994k, 26k, 256k, 299k; 961k, 1,010k | 1,032k, 21k, 299k, 298k; 933k, 991k |

The owners' profit against the six kinds of wages, a day: days 15–29, the merge 0.25, posts by sales 0.26, the brake
0.27 (`m3` 0.27); days 45–59, 0.19, 0.16, 0.16 (`m3` 0.23); days 105–119, 0.26, 0.26, 0.19 (round 4 0.29), the six
wages 56.9k, 57.1k, 55.8k a day and the owners' profit 14.9k, 15.1k, 10.5k. Hungry: each run's day-46 dip, the merge
16, posts by sales 41 with 9 starving, the brake 18 with none; nobody hungry on any other day (corrected, round 6: but
one, in the merge's end record, day 120's last hours). Bread 0.070, 0.070,
0.067 LD on day 120 (band 0.05–0.09); a bronze sword 2.26, 2.27, 2.25 LD, an iron one 4.4 to 4.5, a steel one 9.4 (the
store's and the smiths' pennies over a wage a third up); no rapier sold (25). The going rate (the realm's) 68p on
day 120 in all three, the wage (the realm's posts') 80.5p to 81.5p. The review's land (its 85 tills) over days
61–120: hours credited 96.4k in the merge, 100.1k with posts by sales, 98.5k with the brake (round 4 97.3k); its hires'
slot-days taken 80%, 83%, 84%.

- **Worse:** the trajectory is round 4's, and the merge onto `83714a0` made it worse: by day 120 the Gini 0.599 to
  0.615 and the poorer half 0.089 to 0.097, against round 4's own 0.586 and 0.105 and the bands' 0.33–0.45 and
  0.18–0.28. The farm tills drain from their top (308k on day 98 in the merge, 280k on day 98 with posts by sales, 276k
  on day 77 with the brake) to 182k to 227k by day 120, a quarter to a third gone in three to six weeks, while the
  shop tills keep rising (840k to 881k). The capital is dry from about day 98 in all three (21k to 26k). The Houses fall to 0.65 to
  0.69 of their founding.
- **Posts by sales** (no brake): the land worked 4% more hours than the merge over days 61–120 and took more of its
  hires (83% against 80%); the wage and the Gini as the merge's. Not the lever the Gini wants.
- **The brake:** it held the pace back in the second month (mean pace days 21–60: 0.26 against 0.32 without it; 0.36
  on day 60 against 0.41) and not after (0.44 against 0.44 over days 61–120; 0.66 on day 120, the highest); the wage
  floated a little later (61.8p on day 60 against 64.4p) and caught up (80.5p). On day 120 its Gini is the lowest of
  the three (0.599 against 0.615) and its poorer half the highest (0.097 against 0.089), the Houses kept more (1,032k
  against 994k) and the towns more; but its farm tills are the lowest (182k against 208k) and its owners' profit in
  the last fortnight a third under (10.5k against 15.1k a day). Its day-46 dip had no starving where the run without
  it had 9. Single runs: the differences are of the size two seeds would give, but for the second month's pace.
  (Round 6: and on two harnesses, above. Of all this only the second month's pace is the brake's for certain; the
  Gini, the poorer half, the Houses, the farm tills, the profit and the dip's starving are not established either
  way.)
- **What still needs a year** (the review's condition for keeping a7500b4, not met here: no run past 120 days): the
  land's hours, its tills and the food through a winter and a harvest, with the hold gone and the brake in its place.

**What the two waiting decisions would take** (from the code; nothing here is changed):

- **(a) The Houses' estate crews sized as the model's, their places taken as posts are.** The model (`house_week`)
  keeps an estate crew of what's left of a House's week's budget after its retainers (three tenths, twelve at most),
  its pieces mended, its finery and its works, up to a tenth of the realm's workers (`len(self.workers) // 10`), at the
  going rate; its crew is posts (`staff_to`: vacancies, filled in the labour market like any employer's, let go when
  the budget falls). The game's (`institutionsWeek`, `Core/RatwInstitutions.cpp`) budgets as the model (`k` 4, `a2` 0.3)
  and posts what's left as a week's hire, "building work" at its head's work spot. What differs:
  1. `hireForWeek` cuts every hire to 12 slots (`j.slots = std::min(slots, 12)`): the estate crew wants its own most,
     a tenth of the realm's grown wolves of working age (about 80 of today's 830), so a cap passed in, or several hires.
  2. Its places are taken after every business's: `oddJobFor` (`Core/RatwOddJobs.cpp`) has the jobless and the idle
     (an idle hand is asked as jobless) take the best-paid "a hand" or "a hand at the shop" first (`bestOf`), and a
     "building work" or "about the home" hire only in the first-open pass after, by its place in the list. Taken as
     posts: `bestOf` to weigh an institution's week's hire (`until >= 0`, its payer a House, a town's own or the
     church) with the businesses', by its pay a slot. (Round 4's trial ranked them by pay, ties by lot, and they still
     went untaken; with them first, the Gini was no better in 60 days, 0.566.)
  3. Its untaken places aren't posts open: `tendLabour` counts only a `till:` payer's hires in `vacancies_`, so a House
     short of crew shows in no measure (the realm's `vacant`). Counting them under the House's key would; the House's
     wage needs no review (it pays the going rate, as the model's).
  4. A poached hand: `lookingFor` already draws from posts to any hire paying better, the crews' too.
  5. Hires aren't saved: a restart mid-week drops the week's crew until the next week's budget (as now).
  6. Its size: four Houses, up to about 320 places at the going rate, near 19k a day at the full size against the
     Houses' 1.5k a day of crews now. Their fortunes would go as the model's (0.17 of the founding by day 168), into
     households' pay: the lever the Gini wants, by the model's reading. A year's run is its measure.
- **(b) Real payers for the capital's posts of last resort.** Asked of every resident on day 3 (a run with the payers'
  columns, since taken out), the capital's treasury pays 85 paid civilians as "the town" (`payerOf`'s last line,
  `Core/RatwWages.cpp`), besides the watch: 27 live-in servants (23 "in service to" one of 16 households, 4 "keeping
  the chambers" at Mossgrave Court), 3 cooks of Grayrock Hall, 10 gardeners (in the Rainward Hills' west cell), 8
  carriers, 4 washers, 4 messengers, 3 timber haulers at the docks, 3 gate keepers, a lamplighter, and some 21 of the
  city's own (the City Office, the Counting House's clerks, the customs, the records, the court and the gaol, the
  harbour, the Rain Hall, the school, the infirmary). (Corrected, round 6, from the code and the export: 31 live in,
  27 "in service to" one of 18 households and 4 keepers of the chambers, one at each House's seat; the 3 cooks are the
  Houses of Greatly's, Grayrock's and Mossgrave's, in the kitchens below their seats; the 3 gate keepers are the Houses'
  own, by their descriptions ("the gatekeeper of House Fell"), not the city's; the city's own are 18; and besides the
  85 the capital paid 9 unpaid posts, which draw a wage too (doc 42): 6 at their lessons at home (5 of the Heights'
  households, one of Grayrock Hall) and 3 of the Houses' families at the Houses' business, the Lord of Greatly at the
  counting house among them. Round 6's section lists every one and its payer now.) What it would take:
  1. **Live-in servants** (27): the household-servant rule skips a servant whose home is the room it works in
     (`own->second.homeCell != job.work.cell`), so every live-in servant falls through to the town. Without that test
     `richestAt` picks the household's richest, which can be a fellow servant: it needs the richest who isn't in service
     itself (whose own paid post isn't worked at home), and `richestAt` to see a house's "_down" rooms as it does its
     "_up" (Grayrock Hall's kitchen). The household pays at the going rate from its purse; one that can't pays short,
     and its servant leaves after a week under 40% (`LookBelow`), as from any post.
  2. **A House's seat staff** (the chambers at Mossgrave Court, the cooks of Grayrock Hall): `indexEmployers` lists a
     House's head at its work room only, so its seat's other rooms find no employer. Listing the seat's rooms (its
     "_up" and "_down") under the head; better, paid from the House's account (`house:`), as its retainers are, not its
     head's purse.
  3. **Gardeners and gate keepers** (13): their work is an outdoor cell nobody owns. As the hands at a trade's works
     already are (`trades`: "or else by its town's richest great house"), a title rule ("the gardens") paid by the
     town's richest House; the gates are the city's, as the watch.
  4. **Carriers, washers, messengers, haulers, the lamplighter** (20): work "for hire". The household rule
     (`titleWages`' `household`: they live by odd jobs, no wage), or the docks' account for the haulers (it is a
     buyer's fund now) and the Town Works for the lamplighter (doc 59).
  5. **The city's own** (about 21): rightly the capital's. With (1) to (4) the capital pays them, the gates and the watch,
     about 2.3k a day at 60p against its 3.6k of tax; its buyers' funds outside its budget (2.8k a day) still drain it: that
     is decision 3's option (a).

**Saves:** `paceLand` in the society's memory (`RatwWire.cpp`, so in checkpoints): at most `PaceLandDays` readings, each
day 0 to 365,000,000 and each value 0 to `PaceLandMost`, in order; a bad one drops them all, and an older save reads
none (the brake waits a fortnight). `landWorkSeen_` (the land's hours at the last turn) isn't saved: after a restart the
first turn reads none.

**The tick:** the brake sums 1,083 residents' credited hours once a day; the posts open ask a producer's hands wanted
once more a day. The smoke's 0.0665 ms a tick, the society's share 0.032 ms, was run beside three 120-day runs and
other sessions' on a loaded machine (round 4's smoke: 0.034 ms alone), so not comparable; the daily passes' extra
work is two loops over the residents and the businesses once a day, nothing per tick.

**Tests** (each fails with its change undone, checked one at a time: `py/mutate.py` in the round's scratch):

- `crafting_tests` `aProducersPostsBySales`: a fishery selling for a hand's day, planning two for its bare shelf, has no
  post open (with the half again: one); for three hands' days, two; for ten and more, three, its hand and two more
  (the cap undone: ten).
- `crafting_tests` `aHireTakenIsNoPostOpen`: three idle wolves take every slot of a fishery's hire, and nothing stands
  open (those on its hires not subtracted: two).
- `roads_tests` `thePaceStepsUpWithTheLand`: with no work for more hands, no post open (the exclusion undone: three).
- `roads_tests` `aCityHandAtTheFishery`: above (the one market undone: its hire isn't the land's).
- `roads_tests` `thePaceIsSteadied`: the land's hours down a tenth on the week, the pace holds with the food rising
  (the brake undone: it goes on); down a fortieth, it goes on (no slack: it holds); a day missing, it goes on; the
  readings saved (not saved: none read back), a bad one dropping them, an older save reading none.
- All 67 suites pass (`pg_tests` skipped: no database).

**The smoke** (30 days, `build/econ_watch` at `7797bc0`, `--deterministic --hours --threads 3`): conserved every day,
nobody hungry, none starving; digest 1f8d4fae1c99ee3c. Day 30 against `m3`: the Gini 0.551 (0.552), the poorer half
0.151 (0.152), the wage and the going rate 60p (60p), the pace 0.161 (0.107), bread 0.074 LD (0.074), a bronze, an
iron and a steel sword 2.50, 5.07 and 10.14 LD (2.50, 5.04, 10.12); the land's hours the week 20.4k (21.3k); the
Houses 1,225k (1,238k), the capital 207k (205k), the farm tills 304k (328k); the owners' profit against wages, days
15–29, 0.26 (0.27).

**For the user:**

1. **The brake in the hold's place?** Recommended: keep it (`99dcd27`). It is loose (24 of 107 days), it
   held the pace back in the second month while the land's hours fell, and its day-46 dip starved nobody; but it does
   not mend the trajectory, and its farm tills drained the most. Dropping it leaves the food as the pace's only check,
   as round 4 left it. A year's run is still the test of either. (Corrected by the review, round 6: the case for it
   rests on the second month's pace alone; the dip's starving and the day-120 Gini are single runs on two harnesses,
   not established. Two or three runs a variant on one harness, or the year, before keeping either.)
2. **The Houses' estate crews** (round 4's decision 2): what it would take is above. Recommended as before: the next
   balancing step, measured over a year; it is the one lever here aimed at the households' share, which every 120-day
   run shows falling.
3. **The capital's posts** (round 4's decision 3): what each kind would take is above. Recommended: (b) for the
   servants, the seat staff, the gardeners and the work "for hire" (60 of the 85), and (a), the budget, for the rest
   and the buyers' funds.
4. **Lodging for the land's distant hires:** measured worse (above) and not kept. If the user wants city hands to live
   at the land's bunkhouses for the look of it (they are in the export), it costs the land's producers their lodgers'
   food and, in this run, a tenth of the land's hours.
5. Round 4's decision 4 (the founding wage against the money) stands as it was: by day 120 the wage is 80p and rising,
   the model's 97p by day 112.

### Phase 13: kept hands' meals (the user, 2026-10-26)

A Hold's hands and a party's companions never ate: off their schedules (a leader, `LifeBody::companion`) they decided
nothing (`Society::decideAuthored`'s companion branch, and the demo roster's), while their hunger rose as anyone's, so
they starved at their posts within two days (`strike`: 26 starving Hold hands; question 1 of "players who push"). The
user: they eat from the Hold's or the party's stores, so a Chapter has to keep food in.

**What the game has.** A Chapter's stores are its treasury's purse (`chapter:<id>`): members put any good in and take
it out ("store", "take"; `Game::estateCommand`), reached from a place it holds or a camp with a storage pile built
(`Game::campHere`), and a keep's workshops and field make from them into them (`Game::keepWork`). A party has no
stores: each player carries its own pack (its account), and the one who asked a companion along pays its wage purse to
purse (`Game::companionTick`). So:

- **A Hold's hand** (`camp::Staff`, not one still on its way to make a home there) eats from its Chapter's stores, when
  its site has a storage pile built: the same reach a member has.
- **A companion** eats from the pack of whoever asked it along (`Companion::by`; the leader, once the asker has gone),
  while the two are in the same place: a companion told to wait, or left behind, has only what it carries.

**How they eat** (`Society::keptHandDecides`, `keptHandDoes`, in `RatwDemand.cpp`; both rosters): as a resident does,
on the same marks. What it carries first; carrying nothing, at hunger 55 (the rations' mark: a watch's mess, a mine's
bread) one piece of food is moved from its mess to it by the ledger (`Society::shift`, "rations" from a Hold, "shared
from the pack" from a player: moved, never made), what spoils soonest there (`eatFirst`); at 60 it eats, eight
seconds, its hunger down by the food's catalogue nourishment and a tenth (a meal 55, porridge 22). The draw changes
what others see, so it is made in the resident's turn (`DecideInTurn`), never on the threads. Its task stays
"companion" (the game moves it; its reason says it is eating). The game says where each eats (`Society::setMesses`,
hand to account), from `Game::feedKeptHands` every half second before the world's tick (only when it changes); not
saved: the camps and parties it is made from are, and the first tick after a restart sets it again. A few dozen map
entries, looked up only for kept hands: no measurable cost.

**When the mess is bare** (the fallback; a decision for the user, below): `Society::unfed(id, hunger)` is a kept hand
as hungry as that, carrying nothing that feeds, its mess bare or out of reach.

- At `Society::KeptToldHunger` (70) the Chapter is told, "The Chapter's stores have nothing to eat, and the hands at
  <site> are going hungry." (or, with no storage pile, to build one), or the hirer, "<name> is going hungry, and you
  carry nothing to eat." (or "away from the food you carry"): once a game day each Chapter or hirer
  (`Game::hungerTold_`, not saved: a restart may tell once more).
- At `KeptLeaveHunger` (90) the hand goes back to its own life, as one unpaid does: "<name> leaves <site>: there was
  nothing to eat." (the staff entry gone, `leaderId` cleared), or a companion leaves the party, "I can't go on with
  nothing to eat. I'm going home." ("you had nothing to feed them."). Its own again, it eats as any resident: its pack,
  its larder, a shop with its wage, alms. Nobody starves in a Chapter's service.
- What a companion takes from a player's pack is told ("<name> eats 1 prepared meal from your pack."), and the
  player's character saved (`record(Economy, player)`).

**The harness** (`Tests/econ_players.{h,cpp}`): `poach` and `strike` now feed their hands as the game does. Each hand's
mess is its Hold's treasury (`keepMesses`); every quarter of an hour a hand unfed is noted "told hungry" (once a day)
or, at 90, "left hungry" and let go (`keepFed`); and the Hold keeps food in, as the user's rule wants of a Chapter: a
quartermaster, a player of the Chapter ("player-<scenario>-1"), buys at the capital's food shops (in their order,
what each sells that feeds) what tops the stores up to `ProvisionDays` (2) of a wolf's day's food
(`Society::DayNourishment`, 47) a hand, at the start and each dawn, and puts it in the stores as "store" does; its
purse topped up from the capital's treasury to twice that food at a meal's price, as the hands' pay is (a stand-in for
the Chapter's dues). The scenarios' players block counts the quartermaster (`players` 1) and what it holds.

**Tests** (each seen to fail with its change taken back: the draw and the meal, and the going home):

- `econ_players_tests` `keptHandsEat` (the authored roster): a Hold's hand eats a jerky from the Hold's stores (3 to 2),
  a companion a meal from its hirer's pack, one with no mess its own meal, and one with neither stays hungry and is
  `unfed`; the meals drawn are told; money conserved. `strikeKeepsItsHandsFed`: two days of real ticks, the hand
  never at 70, the stores topped up at dawn. `strikeHandsGoHomeHungry`: the stores emptied mid-morning, the hand at
  88 is told of once, leaves at 90.0 to 90.5, and its own again eats within the quarter day (never above 91).
- `camp_tests` (the game, the demo roster): Bracken, stationed at Juniper Camp with its storage pile, eats a meal from
  the Chapter's stores; after a restart, the stores bare, the Chapter is told once, and at 90.5 he leaves the camp.
- `party_tests` `aCompanionEatsFromThePack`: hired Bracken eats a meal from Ada's pack and she is told; her pack bare,
  she is told he goes hungry; unfed, he goes home.
- Every suite passes (`pg_tests` and the ones without a count print nothing; no `scenes_tests` in this build).

**The strike run** (DEV's export, 30 days, `--deterministic --hours --threads 3 --players strike --players-from 2`,
this branch's binary; `RATW_STOCK` for the stores; against the same binary without players). Money conserved in every
record. The 26 hands, held from day 2 to day 30's dawn, were never hungry (`staff_hungry`, 70 or more: 0 every day,
against 25 to 26 hungry and starving before); nobody in the realm hungry or starving on any day, with or without the
strike. None told of, none left hungry. The stores (pieces of food, every three game hours; mostly porridge, some
meals and raw meat) and what the hands drew:

| Day | stores, fewest to most | drawn | bought at dawn | Day | stores | drawn | bought |
|---|---|---|---|---|---|---|---|
| 2 | 25 to 73 | 58 | 83 (985p) | 16 | 77 to 112 | 41 | 102 (703p) |
| 3 | 55 to 73 | 18 | 48 (786p) | 17 | 24 to 66 | 51 | 86 (516p) |
| 4 | 30 to 39 | 23 | 17 (102p) | 18 | 29 to 64 | 40 | 50 (300p) |
| 5 | 62 to 80 | 20 | 50 (876p) | 19 | 77 to 113 | 40 | 107 (657p) |
| 6 | 47 to 81 | 37 | 40 (312p) | 20 | 27 to 67 | 43 | 97 (582p) |
| 7 | 41 to 77 | 43 | 66 (734p) | 21 | 25 to 60 | 42 | 49 (294p) |
| 8 | 35 to 67 | 35 | 68 (718p) | 22 | 68 to 110 | 52 | 98 (588p) |
| 9 | 32 to 73 | 49 | 74 (574p) | 23 | 28 to 68 | 45 | 100 (600p) |
| 10 | 58 to 100 | 48 | 78 (468p) | 24 | 19 to 63 | 51 | 61 (366p) |
| 11 | 26 to 54 | 31 | 57 (342p) | 25 | 68 to 113 | 51 | 100 (600p) |
| 12 | 35 to 77 | 48 | 94 (733p) | 26 | 29 to 66 | 47 | 110 (660p) |
| 13 | 78 to 112 | 40 | 91 (546p) | 27 | 20 to 62 | 45 | 60 (360p) |
| 14 | 18 to 50 | 39 | 49 (294p) | 28 | 74 to 110 | 47 | 102 (612p) |
| 15 | 38 to 75 | 38 | 102 (703p) | 29 | 26 to 67 | 47 | 104 (624p) |

(Drawn: "rations" from the Hold, `flows.csv`; bought: the quartermaster's, by the day its record is labelled.) About
45 pieces a day for 26 hands; the quartermaster bought 2,097 pieces for 15,268p in the month; the stores never below 18 pieces. Day 30 against no
players: Gini 0.5026 against 0.5048, the poorer half 0.1552 against 0.1564; the week before, within 0.004 and 0.003.
The ticks, 0.062 against 0.035 ms, were measured beside other sessions' runs: not comparable.

**The same Hold keeping no food** (6 days, the quartermaster switched off by a temporary build, not committed): 14
hands left hungry the first day and 11 the second, each Chapter told once a day; one, with food enough of its own,
stayed to day 6. The realm's record had 10 hungry (the Hold's, before they left) on the second day and none starving
on any. So `strike` must stock its Hold, or it is a two-day strike: it now does.

**For the user:**

12. **The fallback when a Hold's stores are bare** (built: told at 70, home at 90). Recommendation: keep it. The
    alternative, a hand stepping away from the Hold to buy a meal with its wage and coming back, wants a walk the
    game doesn't give a stationed hand (the game moves it, and a Hold may be far from any shop), and would let a
    Chapter keep hands it never feeds.
13. **A companion eats from its hirer's pack only**, not the rest of the party's. Recommendation: keep it (it is who
    pays it, and nobody else's food goes without their say); the party's other players can give it food
    (`Game::giveRefusal`'s gift) if they like.
14. **A Hold's hands need a storage pile at their site** to reach the stores (the members' own reach). Recommendation:
    keep it: a Hold has one long before it has hands, and the Chapter is told to build one.
15. **`strike`'s and `poach`'s quartermaster** buys the Hold's food at the capital's shops (2 days' worth, topped up at
    dawn), funded from the capital's treasury. Recommendation: keep it; the hands' food comes from the same shops their
    households would have bought it at. The alternative, the Hold's own field (`keepWork`), brings in materials (wheat,
    oats, vegetables) that feed nobody till a kitchen makes them into food, and the harness has no Hold.

### Phase 13: real payers, and the capital's week (the user, 2026-10-26)

The user's decisions of 2026-10-26, this round's: (2) the posts the capital paid because nobody else would get real
payers (households their live-in servants, the Houses their gardeners, keepers and cooks), and what it still pays,
with its buyers' funds, kept within its week's budget, letting posts go if it must. And the round-5 wage-bid review's
labour fixes. Branch `p13-r6-payers` on `4a08125`: `2cb3276` (asking who pays leaves the run as it was), `2bc3c81` (real
payers), `8b777af` (the capital's week, unpaid posts, the review's labour fixes). With work by the hours only, but the
first, a fix at both paces that changes no unasked run.

**Asking who pays moved the run** (the review's major, pre-existing). Two caches were filled by whoever asked first:

- `payerOf`'s household richest (`richestAt`) for the day. The society's own first asking is fixed (the day's turn,
  in the labour market; else the day's first decision), but a talk scene (`GameTalk`'s `paidBy`) or a harness asking
  before it (after a restart, or a run's first day) kept its moment's purses for the day.
- The great houses (`houses()`), with their towns: asked before the world had told the society its towns (the day's
  `communityOf`, set at the first tick), a house's town was its stores' guess, kept for good. (Found this round: with
  the first fixed, a 5-day run with work by the hours, asked before its first tick, still parted.)

Measured with `econ_watch --ask-payers` (new: every resident's payer asked before the first tick and every game hour,
and `payers.csv`), 5 days of DEV's export at `4a08125`: without `--hours` d034a6316a8d0cb unasked, 5d6bc19c3322e93b
asked; with it 3f23224e73efa000 and a7820b57427871ac; parting on day 1 (`flows.csv`'s day-2 lines). Now the day's
richest is filled at those fixed points only (`Society::rollRichest`: before the labour market at the day's turn, and
at a day's first decision with none yet; `reset` forgets it, and with `2bc3c81` `restore` too); asked before then, it is
worked out for that asking alone, never kept. The houses are built again once the world gives its towns. After: the
same four runs give d034a6316a8d0cb, d034a6316a8d0cb, 3f23224e73efa000, 3f23224e73efa000: an unasked run is exactly as
it was at both paces, and asking changes nothing. So round 5's `b_brake` and `b_nobrake` (a harness that asked) are
runs of their code as another seed's would be (corrected in round 5's section).

**Who pays the capital's posts now.** Re-derived from the code and DEV's export (`payers.csv`, day 3; the export's
descriptions): the capital paid 85 paid civilians as "the town" and 9 unpaid posts (which draw a wage, doc 42, Phase 3),
besides its watch. Round 5's counts were partly wrong (corrected there). Every one, and its payer with work by the
hours:

| post | n | who (resident ids, "rm_" off) | paid by now |
|---|---|---|---|
| at her lessons in the Hall (unpaid) | 1 | ingrid_grayrock_ | nobody (lives on the household) |
| at lessons (unpaid) | 5 | gerrit_ashby, hanne_thornbury, maren_ravensworth, tage_pemberton, thyra_calloway | nobody (lives on the household) |
| carries loads for hire | 8 | dagfinn_thornham, dunstan_plankham, frida_netherby, ivo_wainwright, kjell_coalham, ottilie_dripdale, sigrun_mudworth, theodora_pitchwick | nobody (lives on the household) |
| cooking for the House | 1 | corra_ropewell | house:grayrock_hall |
| cooking for the House | 1 | frederick_foghand | house:greatly_house |
| cooking for the House | 1 | lotte_greyley | house:mossgrave_court |
| counting the House's money (unpaid) | 1 | ambrose_greatly | house:greatly_house |
| hauling timber at the docks | 3 | agnes_wetley, percival_rustford, runa_nailton | the till of Pennock & Sons (joiners) |
| in service to the Aldwychs | 2 | ilse_dripwell, knut_rainstack | its household (ada_aldwych) |
| in service to the Ashbys | 1 | sigrun_salthand | its household (josephine_ashby) |
| in service to the Beaumonts | 2 | godfrey_bilgewick, silas_rustley | its household (pim_beaumont) |
| in service to the Calloways | 1 | yrsa_wetton | its household (thyra_calloway) |
| in service to the Carlisles | 1 | ulf_pitchham | its household (orm_carlisle) |
| in service to the Dorringtons | 2 | ambrose_wetwell, lavinia_slagridge | its household (kari_dorrington) |
| in service to the Everards | 1 | thyra_plankstack | its household (berit_everard) |
| in service to the Fairfaxs | 1 | ada_raindale | its household (ulf_fairfax) |
| in service to the Granthams | 2 | erland_plankwell, oskar_wickmarsh | its household (egil_grantham) |
| in service to the Harrowgates | 2 | emmerich_greyridge, hollis_slagwick | its household (oda_harrowgate) |
| in service to the Hawthornes | 2 | runa_nailstack, septimus_plankdale | its household (jorunn_hawthorne) |
| in service to the Lockwoods | 2 | imogen_coalstack, ulf_ashmarsh | its household (kari_lockwood) |
| in service to the Pembertons | 1 | mogens_brinestack | its household (tage_pemberton) |
| in service to the Ravensworths | 2 | lavinia_millridge, septimus_brinehand | its household (maren_ravensworth) |
| in service to the Stanmores | 1 | wynn_pitchwell | its household (karin_stanmore) |
| in service to the Thornburys | 2 | hollis_slagford, silas_brinemore | its household (hanne_thornbury) |
| in service to the Vances | 1 | roald_wickstack | its household (eleanor_vance) |
| in service to the Wexleys | 1 | ivar_millby | its household (lars_wexley) |
| keeping the chambers | 1 | arne_wetworth | house:mossgrave_court |
| keeping the chambers | 1 | edmund_saltham | house:greatly_house |
| keeping the chambers | 1 | maren_tarworth__ | house:grayrock_hall |
| keeping the chambers | 1 | wystan_tarstack | house:house_fell |
| keeping the gate | 1 | frederick_nailgate | house:mossgrave_court |
| keeping the gate | 1 | oskar_sootwick | house:grayrock_hall |
| keeping the gate | 1 | philippa_fogwick | house:house_fell |
| keeping the wharf's books (unpaid) | 1 | julian_greatly | house:greatly_house |
| runs messages about the city | 4 | dunstan_sallow, ottilie_slagley, philippa_coldby, rika_gullwell | nobody (lives on the household) |
| tending the gardens | 3 | agna_dripridge, disa_raincot, sybil_bilgeby | house:grayrock_hall |
| tending the gardens | 4 | berit_coalworth, bertil_slagley, gudrun_rustwick, signe_ashstack | house:mossgrave_court |
| tending the gardens | 3 | ivo_wickgate, oda_kelpworth, wynn_sootridge | house:house_fell |
| travelling on House business (unpaid) | 1 | aldric_grayrock_ | house:grayrock_hall |
| washes linen for hire | 4 | dagfinn_wainwright, delphine_brackwick, delphine_redfern, sigrun_mudmore | nobody (lives on the household) |
| boiling rags | 1 | gorm_plankgate | the capital |
| clerking at the City Office | 2 | edmund_sootcot, olaf_pitchmore | the capital |
| clerking at the Counting House | 2 | ivar_pitchwick, rune_gullwell | the capital |
| copying the rolls | 1 | eleanor_mudworth | the capital |
| hearing cases | 1 | theodora_driphand | the capital |
| issuing permits | 1 | gwen_cinderdale | the capital |
| keeping order in court | 1 | erland_ashworth | the capital |
| keeping the Rain Hall | 1 | gerrit_mudhand | the capital |
| keeping the gaol | 1 | brand_mudridge | the capital |
| keeping the harbour | 1 | dagny_rustcot | the capital |
| keeping the records | 1 | soren_kelpcot | the capital |
| lights the lamps at dusk | 1 | desmond_toft | the capital |
| running the City Office | 1 | oskar_gullworth | the capital |
| taking the tolls and rents | 1 | ilse_cindercot | the capital |
| taxing the cargoes | 1 | yrsa_tarwick | the capital |
| teaching letters | 1 | imogen_ashby | the capital |
| weighing bales | 1 | jasper_bilgeford | the capital |

Besides the 94, with the same rules: 13 of the Houses' seat staff at their heads' own rooms, paid from the head's purse
before, are paid from the House's (8 "serving in the hall", 4 stewards, Fell's cook); and 45 unpaid posts worked at
their holders' own homes ("at home in the Heights", "at leisure", "between voyages"), which their household's richest
paid, a fellow servant as often as not, no longer draw a wage. 30 more "for hire" whose work is at a shop's place stay
the shop's, as before.

The rules (`Society::payerOf`, `Core/RatwWages.cpp`):

- **Live-in servants:** a household's servant is paid by the household's richest not in service itself (whose own paid
  post isn't worked at home), live-in or not; `richestAt` sees a house's room below ("_down", a kitchen) as its rooms
  above. A household with none but its servants at home: the town, as before. One that can't pay pays short, and its
  servant leaves after a week under 40%, as from any post.
- **A House's seat:** its head's work room and the floors above and below it (`seats_`); every post there but the
  head's is paid from the House's own purse, as its retainers are.
- **A House's grounds** ("tending the gardens", "keeping the gate"): the House whose family's name is a word of the
  holder's home ("the_fell_gatehouse", "grayrock_cottages"), else its town's richest House (the Greatly gardener's home,
  "drayman_house", names none: Mossgrave, the richest, pays it).
- **The docks' timber haulers:** a trade's hands ("hauling timber": the town's sawmill or joiners, else its richest
  House), as the forge's and the saws' are.
- **Work for hire** (titles "for hire", "runs messages") that no shop or household pays: nobody's post; they live on
  the household and take hires as the jobless do (`RatwResidents.cpp`). Their pay is the hires', not a wage.
- **Unpaid posts:** one worked at its holder's own home is the household's own life, no wage; one of a House's family
  away from its seat and any shop, the House's.
- **The city's own** (18 and the lamplighter) stay the capital's. (The Counting House's two clerks work at the Greatly
  counting house, where the Lord of Greatly counts the House's money: arguably the House's. For the user, below.)

**The capital's week** (the model's `treasury_week`; `Society::institutionsWeek`, `keepPostsTo`): its week's budget as
before (`k` 2, `a2` 0.4, its relief set aside first), then

- **its posts, to half of it** (the model's watch's share): the watch first, kept whatever (the watch keeps its posts),
  then its own in an order by lot that is the same week to week, a week of each at its wage (six days, the watch
  seven), as many as fit. Past that a post's holder is let go ("laid off") and the post kept empty (`memory.closed`, as a
  business's help); a post it let go comes back ("taken on again") when the budget has room, for succession to fill: the
  model's `staff_to`, its posts let go and opened again as its budget falls and rises, in a fixed order where the
  model's are by chance. Its posts are those whose wage it pays, an unpaid one too.
- **its buyers' funds, inside what's left:** a week's allowance, what they drew last week and a third over, and a
  twentieth of the rest besides (its first week: all the rest), drawn day by day by `townBuyers` as before but never past
  what's left of it (`memory.fundsLeft`, `fundsWeek`). Before, its buyers were funded day by day from its purse above
  its payroll reserve, whatever its budget.
- **its works** with what remains, as a town's (most of it a crew, the rest materials).

The towns' treasuries are as they were (they hold, round 5's runs).

**The wage-bid review, mended:**

- **A producer's own hire by its sales** (major): its hands wanted are `handsWanted(key, false)` for its hire as for its
  posts open and the going rate's weights, so a slot nobody takes is a hand its sales want. (Corrected: round 5 fixed
  only the posts open past its hire.)
- **The brake's reading** (minor): the land's hours are counted as they are credited (`Society::produce`, a hand's
  hire), into `memory.landToday`, saved; the day's turn reads it and starts again. A roster's change no longer takes a
  producer's life's hours out of a day's reading, and a restart no longer turns the brake off for a fortnight (only a
  save from before this, or a new world, starts counting at its next turn). A producer of the seasons with no work out
  of them (the apiaries) is left out as the fields are: its season's end would read as the land falling.
- **`aCityHandAtTheFishery`**: the hand's hours on their own, the tick it finishes its spells (8 at once, and the
  brake's count with them); a mutation that drops that credit fails it.
- **`hires.csv`**: a taker is "away" by the society's own rule for a hire's town (`Society::townOfResident`), asked at
  the sampling, not from where it began; `econ_players_tests` checks the file's columns.
- **The doc** (round 5's section): the rest's share worked is 51% to 55%, not 55% to 58%; one hungry in the merge's end
  record; the brake's case rests on the second month's pace alone; the payers' counts.

**120 days** (DEV's export, `--deterministic --hours --threads 3`, three at once beside other sessions' runs; every run
conserved money every day). "This branch" is `8b777af`; "no capital" is it with the capital's payers and week switched
off (trial switches in a build otherwise the same: its 5-day digest without them, 19d916fd323767da, is the branch's),
so the labour fixes alone; "bare hire" is this branch with a producer's hire by its bare shelf again; "the merge" is
round 5's `b_merge` (`83714a0` with round 4's branch, before round 5's fixes). One run each, one seed: a difference of
a few hundredths in the Gini, a few hungry on day 46, is what another seed would give. The scripts and runs:
`py/weeks.py`, `r6_full`, `r6_nocap`, `r6_barehire` in the round's scratch.

| day: the capital's treasury | this branch | no capital | bare hire | the merge |
|---|---|---|---|---|
| 2 | 314k | 309k | 314k | 309k |
| 14 | 283k | 238k | 283k | 240k |
| 28 | 278k | 197k | 277k | 196k |
| 42 | 287k | 165k | 285k | 166k |
| 56 | 273k | 109k | 269k | 110k |
| 70 | 263k | 68k | 263k | 70k |
| 84 | 255k | 46k | 257k | 50k |
| 98 | 249k | 26k | 252k | 26k |
| 112 | 247k | 27k | 235k | 29k |
| 120 | 247k | 24k | 227k | 25k |

- **The capital holds.** Its week's budget (after relief) falls from 34.5k on day 7 to 29.1k by day 105 and turns up
  (29.5k on day 119) as its cash comes down to its rule's level, about 2 months' income; its cash 312k, 303k, 283k,
  271k, 262k, 265k at the reckonings of days 7, 28, 56, 84, 105, 119: 5% of the money, the model's treasury's share
  from year 1. Its posts cost 13.3k to 13.7k a week (its own 19 and its watch), under the half of its
  budget they may take (14.6k to 17.2k); its buyers' allowance 15.4k to 21.3k, all of it drawn. The capital a day,
  days 105–119: in 5.7k, out 5.0k (the merge: in 6.3k, out 4.7k, from a dry purse); service wages 0.8k (the merge 3.2k
  and 0.25k to unpaid posts), the watch 0.9k, its buyers' funds 3.0k (the merge none: they stopped under its payroll
  reserve from about day 90). The city's upkeep (`condition`) 98.9 on day 120 against 54 in the merge and with no
  capital.
- **No post was let go in 120 days** on this branch: the half never ran short. With bare hire, 2 on day 119, when
  its budget had fallen to 27.6k and its posts cost 13.9k against a share of 13.8k. So the layoffs are tested but
  barely exercised; in a year, as the wage rises and the budget settles at its income, they would start.

| day: Gini, poorer half, wage, pace | this branch | no capital | bare hire | the merge |
|---|---|---|---|---|
| 14 | 0.527, 0.156, 60.0p, 0.02 | 0.534, 0.151, 60.0p, 0.02 | 0.533, 0.152, 60.0p, 0.02 | 0.536, 0.148, 60.0p, 0.02 |
| 28 | 0.526, 0.166, 60.0p, 0.21 | 0.537, 0.159, 60.0p, 0.27 | 0.536, 0.159, 60.0p, 0.18 | 0.545, 0.154, 60.0p, 0.25 |
| 42 | 0.546, 0.151, 60.0p, 0.36 | 0.553, 0.145, 60.0p, 0.39 | 0.552, 0.149, 60.0p, 0.36 | 0.559, 0.143, 60.0p, 0.43 |
| 56 | 0.564, 0.137, 60.0p, 0.30 | 0.558, 0.137, 60.0p, 0.30 | 0.563, 0.137, 60.0p, 0.36 | 0.568, 0.133, 61.8p, 0.32 |
| 70 | 0.581, 0.123, 60.0p, 0.27 | 0.579, 0.119, 61.7p, 0.32 | 0.579, 0.124, 63.7p, 0.32 | 0.584, 0.120, 66.1p, 0.32 |
| 84 | 0.580, 0.116, 63.1p, 0.30 | 0.586, 0.110, 65.5p, 0.38 | 0.580, 0.118, 66.7p, 0.32 | 0.576, 0.123, 70.4p, 0.39 |
| 98 | 0.581, 0.108, 64.6p, 0.46 | 0.597, 0.101, 66.0p, 0.39 | 0.585, 0.109, 69.6p, 0.48 | 0.587, 0.110, 72.9p, 0.38 |
| 112 | 0.582, 0.102, 65.9p, 0.48 | 0.612, 0.091, 67.4p, 0.46 | 0.600, 0.100, 73.2p, 0.59 | 0.602, 0.097, 78.2p, 0.52 |
| 120 | 0.587, 0.099, 67.7p, 0.61 | 0.624, 0.086, 67.4p, 0.55 | 0.611, 0.092, 73.4p, 0.64 | 0.615, 0.092, 80.7p, 0.57 |

| day 120 (day 60) | this branch | no capital | bare hire | the merge |
|---|---|---|---|---|
| great houses | 813k (1,064k) | 877k (1,137k) | 828k (1,102k) | 965k (1,181k) |
| the towns; the church | 280k; 293k | 267k; 295k | 259k; 307k | 251k; 302k |
| farm, shop, House tills | 184k, 835k, 361k | 212k, 846k, 412k | 208k, 798k, 379k | 227k, 840k, 398k |
| owners (producers, merchants) | 949k | 1,029k | 907k | 981k |
| paid civilians; unpaid; without work | 779k; 78k; 19k (20) | 712k; 106k; 98k (54) | 835k; 73k; 27k (25) | 716k; 86k; 87k (51) |
| guards; children and apprentices | 117k; 24k | 94k; 11k | 116k; 14k | 93k; 10k |
| the land's hours, the week | 23.4k (16.5k on day 49) | 23.5k (16.7k) | 23.5k (17.6k) | 24.1k (17.1k) |
| hungry on day 46 (and starving) | 18 (0) | 40 (8) | 20 (0) | 16 (0) |
| owners' profit against six wages, days 105–119 | 0.21 | 0.24 | 0.20 | 0.26 |
| bread (LD); the going rate | 0.076; 62p | 0.073; 63p | 0.070; 64p | 0.070; 68p |

(Hungry: the day-46 dip only, in every run, but for the merge's one in its end record. "Without work": its holders'
money and, in brackets, how many.)

- **The capital's changes** (this branch against no capital): the treasury holds (247k against 24k), its buyers are
  funded and the city kept up; the Gini 0.587 against 0.624 and the poorer half 0.099 against 0.086 on day 120, and
  lower from day 84 on (but not on days 56 and 70); paid civilians hold 779k against 712k and the guards 117k against
  94k. The Houses pay for it (813k against 877k: their seats' staff, grounds and family), and the owners (949k against
  1,029k), the farm tills (184k against 212k) and the House tills hold less. Without the capital's changes some 30 more
  residents are without work from the third week (54 against 20 on day 120; in the merge too, 51): most of them had
  sat and watched the world or minded a shop's door until then. Not traced further.
- **A producer's hire by its sales** (this branch against bare hire): the wage floats less (67.7p against 73.4p on day
  120; 65.9p against 73.2p on day 112), as the review expected: the hires it no longer posts were posts open bidding it
  up; the land's hours the same by day 120 (23.4k against 23.5k), a little lower mid-run; the Gini 0.587 against 0.611.
- **The labour fixes together** (no capital against the merge): the wage 67.4p against 80.7p. The model's: 97p by day
  112. The wage floats more slowly than it did, and further from the model's.
- **Worse, plainly:** the farm tills, lowest in this branch (184k, down from 356k on day 21), as in every run they drain
  from about day 80; the Houses fall faster (0.54 of their founding by day 120, against 0.65 in the merge; the model's
  0.73 by day 56 and 0.17 by day 168, so not out of the model's way); the wage stays near 60p to day 77 and reaches only
  67.7p; the owners' profit is a fifth of the wages against a quarter; the day-46 dip is as before (18 hungry). The Gini
  and the poorer half are still far out of their bands (0.33–0.45, 0.18–0.28).
- **What a year still needs:** a winter and a harvest with the brake as now (the apiaries left out), the capital's
  layoffs once its budget is its income, and the wage, the Gini and the farm tills past day 120.

**Saves** (`RatwWire.cpp`, so checkpoints too): `memory.fundsLeft` and `fundsWeek` (account to pennies, each 0 to
1e12, a bad one dropped; an older save has none: its buyers are funded as before until its next week); `memory.landToday`
(0 to `PaceLandMost`, out of bounds or missing: -1, not counting until the next turn). The day's richest, the seats and
the houses are worked out, not saved.

**The tick:** three 120-day runs at once beside other sessions': 0.0650 ms a tick on this branch, 0.0653 ms with no capital, 0.0656 ms with bare hire, so nothing measurable. The new work: `payerOf`'s seat lookup (a hash), the household's richest not in service filled with the day's richest (one pass over the residents at the day's turn), the capital's posts weighed once a week, the land's hours added where they are credited (instead of a pass over the residents each day).

**Tests** (each fails with its change undone, one at a time: `py/mutate.py` with `m_fix.py`, `m_payers.py`,
`m_budget.py`, `m_budget2.py`, `m_labour.py` in the round's scratch, results in `mutate.out`: 34 tried, 33 caught; the one that survived, unpaid posts left out of the capital's budget when its rule counted paid posts only, showed the rule short, and with the rule counting every post that draws a wage it is caught):

- `roads_tests` `askingWhoPaysLeavesTheRun` (both paces): two worlds the same, one asked every resident's payer before
  its first tick, after a restart and every 37 seconds; each morning's end every purse and good, everyone's place and
  activity the same; and a restart with other purses reads them (caught: the richest kept from the first asking, the
  houses kept from before the towns, a restart keeping the day's richest, no roll at the day's turn).
- `crafting_tests` `theCapitalsPostsOfLastResort` (both paces): every rule above, and at the seconds' pace every post
  as before (caught: each rule undone, the haulers' trade at both paces).
- `crafting_tests` `aHireTakenIsNoPostOpen("carries loads for hire")`: those for hire take every slot of a fishery's
  hire (caught: their flag undone, or the town paying them).
- `roads_tests` `theCapitalKeepsToItsBudget`: a lean week lets three or four of its five go (an unpaid one among them),
  keeps its watch, and the same the next week; nothing to spare, all five, the watch kept; money again, all five open
  again; its buyers draw no more than the allowance left; the allowance saved, a bad one dropped, an older save none
  (caught: the posts unbudgeted, the watch not first, the watch let go, not opened again, the order changing weekly,
  the buyers past their allowance, the allowance not saved, unpaid posts left out).
- `crafting_tests` `aProducersHireBySales`: a funded fishery selling for a hand's day posts no hire and stands nothing
  open; for three, a hire of two at most, no more open (caught: the bare shelf's half again).
- `crafting_tests` `theBrakeReadsTheLandsHours`: the fishers' hours counted as credited and the beekeeper's not; saved,
  bounded, an older save not counting; after a restart and a fisher leaving the roster, the next turn reads every hour
  credited (caught: the seasons counted, not saved, the bounds, the producer's own hours, a hand's).
- All 67 suites pass at each commit (`pg_tests` skipped, no database; newcomer_tests 2,652 checks, troubles_tests 75).

**For the user:**

1. **The Houses' gatekeepers** are the Houses' by their descriptions ("the gatekeeper of House Fell"), and are paid by
   them now; the brief had them stay the city's. Recommended: keep them the Houses'.
2. **The Counting House's two clerks** (at the Greatly counting house, where the Lord of Greatly counts the House's
   money) stay the city's here. Recommended: the House's, if the Counting House is Greatly's business and not the
   city's (the export doesn't say).
3. **Work for hire** (16 carriers, washers and messengers) draw no wage now and take hires as the jobless do; their pay
   is what the hires pay. The model's nearest is home help (idle hands paid by households with a help budget).
   Recommended: keep it; the realm has hires enough (the land is short of hands).
4. **Unpaid posts at home draw no wage** (51: masters "at home in the Heights", pupils at lessons): before, their
   household's richest paid them, often a servant, or the town. Recommended: keep it; it was money round a household and
   the capital's 9 unpaid posts.
5. **The capital's week:** its posts to half its budget (the model's watch's share), the watch never let go, its own let
   go and opened again in a fixed order by lot (the model's are by chance), its buyers inside what's left. In 120 days
   it held at 5% of the money and let nobody go. Recommended: keep it, and judge the layoffs over a year. The towns'
   treasuries keep their old rule (they hold); to give them the same, say so.
6. **A producer's hire by its sales** (the review's major): the wage floats less (67.7p against 73.4p by day 120) and
   the land works about the same. Recommended: keep it; it is the review's fix, and the wage's float was partly the
   bare shelf's half again, a shortage that wasn't there. The wage now floats more slowly than the model's (97p by day
   112) on every measure: the next balancing question, with the Houses' estate crews (decision 1 of 2026-10-26).
7. **The brake** counts the land's hours as they are credited and leaves out the apiaries; whether to keep it at all is
   round 5's decision 1, still open (corrected there: its case rests on the second month's pace alone).
8. **Without work from the third week**, some 30 residents who sat and watched the world or minded a shop's door
   lose their posts in the runs without the capital's changes (and round 5's) but not with them. Not traced: worth a
   look before the year's run.
