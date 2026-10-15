# 63. A stable economy: research (2026-10-14)

The user, 2026-10-14: despite the tools built for it (docs 42, 46, 61), the economy has never settled on its own; it
runs only with ham-fisted intervention. Design a legitimate economy plan. Start with research: what we have, how games
have managed lifelike economies without inflation, and what would make a closed economy reach equilibrium by itself.
No money flowing in from outside; rich and poor must remain; direct intervention (taking, redistributing,
reintroducing money) only if it truly has to be. The goal is an NPC-only economy that stabilises before players
arrive. Players will be able to crash markets, and that's fine.

The research came first; the user's answers and the model follow at the end ("Decisions", "How cost works", "The model").

## What we have

### Holders and flows

| Holder | Money in | Money out | Does it spend more when it holds more? |
|---|---|---|---|
| Residents and households | wages per spell (town wage table), owner's share, stipends, dole, alms, estates | food and household needs, wants, commissions, feasts, home help, the plate, deposits, levy, wealth tithe | **Yes**, through about eight overlapping rules keyed on a 140p "wealth line" |
| Shop, farm and works tills (`till:`) | sales | help's and keeper's wages, materials, rent, tax, tithe, market dues, hires, repairs; a third of the week's profit to the keeper; a House's till sends everything above its float to its House each night | **No**: outflows follow need; the rest is left for the orchestrator |
| Great Houses | their tills' takings, rent, caravan trade | servants, trade hands, topping up their trade's businesses, buying failing ones, tax, tithe | **No** (orchestrator only) |
| Town treasuries, the capital | weekly tax on profit × tax level, dues, levy, estates | guards' and labourers' wages, town buyers' funds, a tenth to the capital | **No**, and wages run above tax, so they drain |
| The church | tithe, wealth tithe, the plate | clergy, food alms, a 2p dole (capped at 1/30 of its purse a day), candles | Only the dole cap |
| Town banks | deposits (a share of savings above a line) | withdrawals; everything above the reserve via orchestrator orders and grants | By design, no |
| Orchestrator funds (`fund:<town>:<channel>`) | the orchestrator's orders | a quarter a day on works, hires, commissions, granary, trade, price and wage support, rescue loans | Yes, they exist to empty |

**Prices.** The orchestrator sets one price per good per town:

- **Target:** the catalogue price × a shelf factor (0.8 to 1.2) × scarcity × the town's `priceLift`.
- **Limits:** clamped to 0.6 to 1.6 × the catalogue, and a price may move at most 25% a week.
- **Staples:** capped and subsidised from the price-support fund.
- **Producers** are paid the town price × a margin (0.4 to 0.75).

**No price is built from what the thing cost to make.**

**Wages.** A town table holds the starting rates (help 16, guard 14, labour 12, and so on).

- **Floor:** two days' food.
- **Weekly moves:** up when posts go unfilled or the payers are gaining; down 5% when the payers are draining.
- **The bill is headcount × table.** It does not depend on what the payer takes in.

**Size.** About 10,300 lines across 19 economy files, plus 4,800 more in shared files. On top of that:

- about 60 orchestrator dials and 53 economy constants;
- about 63 kinds of money flow;
- 9 kinds of holder and 9 channels;
- an estimated 45 to 50 live money rules.

### What the runs show

- **Doc 61, 56 days (2026-10-13).**
  - Gini 0.40; median purse 206p; nobody hungry.
  - Residents' money fell from 244k to 208k. Shops' tills rose from 56k to 81k, and the Houses' businesses' tills from 4.8k to 11.8k.
  - Rescue loans came to 86k.
  - In doc 61's own words: "the money isn't lost; it isn't paid back out as wages."
- **Doc 46, year run (2026-10-07).**
  - The poorer half's share levelled at about 0.17.
  - The church pooled to 53k and the banks to 88k.
  - **The orchestrator's own pressure sat at its maximum from day 140 to day 330.** A controller pinned at its limit for half a year isn't steering. It's fighting something structural.
- **Before the orchestrator (2026-10-05).** Gini went from 0.49 to 0.67 in 14 days. Tills gained 2k a day while treasuries drained.

### The books aren't quite closed

Money is meant to be minted once, at the founding. These break that:

- **Four mints mid-game:**
  - a House newly recognised by `sortHouses` gets 3,000p (`Core/RatwHouses.cpp:208`);
  - a town buyer or church account opened later gets starting money, for instance when a town grows past a basket's minimum (`Core/RatwDemand.cpp:844`);
  - newcomers adopted into a save bring their own purses (`Core/RatwResidents.cpp:155`, `Core/RatwSociety.cpp:1052`).
- **The bank's claims can exceed its coins.** `bankWorths` quietly gives savers a haircut (`Core/RatwChannels.cpp:496`).
- **Bandit hoards have no NPC outlet.** Their money comes back only when a player clears the camp.
- **Goods from nothing.** The capital's daily carter brings meals and herbs (`Core/RatwSociety.cpp:610`).

## What games have done

**No shipped game runs a strictly closed money supply for its NPCs and stays stable on its own.** Every one that works
does one of four things:

1. **Faucets and sinks, tuned by hand.** Money is created and destroyed on purpose. This is Ultima Online after
   launch, Star Wars Galaxies, EVE, RuneScape, WoW and Albion. EVE's economist called his team "janitors of this
   world": they meter the money supply rather than close it.
2. **The simulation quietly mints or melts money where supply and demand don't meet.**
   - **Victoria 3:** "the difference between the two is created or destroyed by the simulation".
   - **Merchants with purses that refill on a timer:** Mount & Blade, Kenshi and RimWorld.
3. **NPCs don't use money.** X4's factions have, in effect, unlimited credits, and Foxhole has no currency. The loop
   is closed in goods, not money.
4. **They closed the loop, and money (or stuff) stuck somewhere.**
   - **Ultima Online's original resource bank.** Every object drew from a central bank and returned to it when
     destroyed. Players hoarded, the bank ran dry and sheep stopped spawning. In Simpson's words: "No matter how much
     they added, the resources always flowed into inventory and just sat there." UO's NPC shopkeepers ran a full supply
     and demand simulation, went broke ("piles of things no one wanted and no cash"), and were left to print gold.
     Simpson's verdict: "central planning doesn't work."
   - **Victoria 2.** It had a fixed world money supply, and the money got trapped in national banks and capitalists'
     savings. Paradox dropped the fixed supply for Victoria 3.
   - **Dwarf Fortress.** Its economy had wages, rent and coin, and was switched off. Idle dwarves went broke, so
     players built rooms of levers to give them jobs.
   - **Armageddon MUD.** Its shops ran out of coin as players sold them more than they bought.

Two things from these games carry over directly:

- **Victoria 3's pop wealth.** Wealth rises while income exceeds spending, but spending rises with wealth, so "that
  Pop's Wealth will over time drift towards exactly the level of consumption they can afford to sustain." It's a
  level, not a hoard. Owners also pay a fixed share of their dividends into an investment pool that funds new
  building: capitalists 20%, aristocrats 10%, shopkeepers and farmers 5%.
- **Doran and Parberry, "Emergent Economies for RPGs" (open-source port: BazaarBot).**
  - **Price beliefs:** each agent holds a range of prices it believes in, and narrows or shifts it as its trades
    succeed or fail.
  - **Fixed overhead:** the authors found it helps to charge each agent a steady upkeep (required consumption or
    taxes) "to pressure each agent to be productive".
  - **Turnover:** agents that go broke are replaced in a profitable role, so the market decides how many of each trade
    there are.

The MUD-era writers say the same in plainer words. Hastings: "make sure value drains at the same rate as it trickles
in", using rent, tax and wear. UO also found that carrying costs on stocks broke up hoards: a vendor's fee as a share
of its asking price, and house decay.

So what we're attempting hasn't been shipped. That doesn't make it impossible: the economics below says exactly what
makes it work. It does mean we can't copy a game's solution.

## What economics says

### 1. The one rule: spend more when you hold more

The simplest stock-flow-consistent model, Godley and Lavoie's SIM, has every household spend

    spending = a1 × income + a2 × money held      (SIM: a1 = 0.6, a2 = 0.4)

so the change in its money each period is

    change = (1 − a1) × income − a2 × money held

That change is zero at **money held = ((1 − a1) / a2) × income**, and the money moves toward that level by itself: the
gap closes by the fraction a2 each period. The government's budget also balances without anyone setting it to.

With a2 = 0 there's no resting point, and every surplus just piles up.

**This is the whole theory of a closed economy that settles.** A holder's balance can stand still only if its spending
rises with what it holds. In a closed system the surpluses of all holders sum to zero every period, so one holder
running a steady surplus forces another into a steady deficit. That is our "tills pool, treasuries drain".

### 2. Targets in proportion to flows, never fixed sums

If each holder's resting balance is a multiple of its own turnover (k weeks of what passes through it), all the
resting balances add up to the money supply at exactly one level of prices and wages. Prices and wages then settle at
that level, whatever the money supply is.

If a target is a fixed sum ("keep 500p", "a 140p wealth line", "a float of 80p"), nothing makes the targets add up to
the money there is. Some holder can then never reach its target, and it drains or hoards forever.

### 3. Profit kept is money stopped (Kalecki)

In a closed economy, owners' profits come only from owners' own spending and investment ("workers spend what they get;
capitalists get what they spend"). A till that keeps its profit doesn't make its owners richer as a class: it takes
sales away from other businesses, and the money lies still wherever selling is best.

Profit circulates only if it's paid out and the owners spend it: building, hiring, commissioning, consuming.

### 4. Rich and poor arise without help, and the spread can be bounded

- **Random trade alone gives a Gini of about 0.5 (Dragulescu and Yakovenko).** With money conserved in every trade,
  the distribution settles to an exponential with that Gini. The rich-and-poor the user wants comes for free.
- **Saving propensity shapes the spread.** If every agent saves the same fraction of each trade, the spread is
  narrower than that. If the fraction varies widely between agents, a Pareto tail grows, made of the near-misers.
  **Bound everyone's spending propensity well away from zero.**
- **Condensation comes from wealth-proportional gains.** Interest, rents or shares paid by size of wealth, or a
  richer trader who wins more often, concentrate money:
  - **Yard-sale model:** everything goes to one agent, even with fair coin flips.
  - **Bouchaud and Mézard:** a few agents end up holding a finite share of all wealth.

  A modest pull back toward the mean prevents it. Boghosian's rule: a stable distribution exists when redistribution
  outweighs wealth-attained advantage. **Taxes on income help; taxes on wealth that aren't paid back out make it
  worse.**
- **Our 0.67 (2026-10-05) is past what random trade gives.** So something had a wealth-proportional or near-miser
  term. Today's 0.40 is a healthy figure; the aim is a band of about 0.35 to 0.5.

### 5. A closed model that runs for 500 years: Lengnick (2013)

Lengnick's "Agent-based macroeconomics: a baseline model" uses 1,000 households and 100 firms with fixed money, no
auctioneer and no central planner. It ran 500 simulated years and stayed stable, with realistic unemployment and
price stickiness. It's the best template for us.

- **Wage.** A firm raises its wage by up to 1.9% if it had a post unfilled last month. It lowers it after a long run
  of every post filled.
- **Stock band.** A firm keeps between a quarter of and one month's sales in stock.
  - Below the band, it opens a post and considers a price rise.
  - Above it, it lets one worker go and considers a cut.
- **Price.** Steps are up to 2%, taken only three times in four, and always kept between 1.025 and 1.15 × the cost of
  making the thing (the wage divided by output). **Prices are built from cost.**
- **Profit.** At the month's end the firm pays wages and keeps a buffer of a tenth of its wage bill. **It pays
  everything above the buffer out to its owners.**
  - Short on money, it skips the payout, then uses up the buffer, then cuts wages.
- **Households.** They spend more as they hold more, a little less than in proportion. They compare a shop with a
  random other now and then and switch to the cheaper. The jobless search for work, and their asking wage falls after
  a month idle.

**Our adaptation.** Lengnick pays dividends in proportion to households' current money. That's the one condensation
term in the model. For us, profit goes to the actual owner (keeper or House), and owners spend at least in proportion
to what they hold.

### 6. Many small local decisions, not one big one (Gintis)

Price adjustment driven by one shared, public signal that every agent reacts to at the same moment is unstable. With
private, local prices and decisions at staggered times, the same economy settles.

Our orchestrator is the opposite: one weekly plan, one price per good per town, every holder moved on the same evening.

### 7. How medieval economies kept money moving

All of these fit the fiction, and each is a spend-more-when-you-hold-more rule in costume:

- **Largesse.** A lord's surplus "had to be passed on". Generosity was an obligation, and the feast was its public
  proof.
- **Building funds.** The church's fabric fund, chantries, and town works.
- **Periodic recoinage.** In twelfth to fourteenth century Europe, coins were recalled once or twice a year, four old
  for three new. It worked as a holding tax that went to the issuer.
- **Demurrage.** Wörgl's scrip lost 1% a month, circulated many times faster than ordinary money, and paid for town
  works that cut unemployment.
- **Jubilees** cleared debt.
- **Mortuaries and heriots.** These were dues paid to the church or the lord at a death.

## Why ours doesn't settle

1. **The big holders' spending doesn't rise with what they hold.** Tills, Houses, the church and the treasuries
   spend by need. Their income grows with trade, so each pools or drains (§1).

   The orchestrator was built to supply the missing "spend what's above your target" from outside, as a weekly
   central plan with about 60 dials. That is Simpson's central planner and Gintis's single shared signal. It held
   things up by force, and in the year run it spent half the year at full pressure.
2. **Prices aren't built from costs, and wages aren't tied to what the payer takes in.** A business whose takings
   exceed headcount × table keeps the difference. A treasury whose tax is below its wage bill drains. Nothing in the
   rules makes those two meet.

   In Lengnick, a price is a markup on the wage cost, and a wage moves on unfilled posts. Wages and prices find each
   other.
3. **Profit is kept, not paid out (§3).** Every owner-run till keeps two-thirds of its weekly profit. Houses take
   their tills' surplus but have no outflow that grows with it. The latest run shows exactly this: 25k more in shop
   tills, residents 36k poorer.
4. **Targets are fixed sums.** A 140p wealth line, 80p keeper floats, a 3,000p House fortune, 7 days' payroll.
   Against a fixed money supply they can't all be met at once (§2).
5. **Wealth-proportional gains and near-misers.** Deposits are taken as a share of savings with a haircut. Caravans
   sell at 40% over what the selling town paid, and that goes to the Houses and treasuries that run the trade. Guards
   saved from 27k to 64k in one run (doc 46).
6. **Leaks in the books** (above). Each is small, but every one hides a stock that flows can't explain.
7. **The rules pile up.** Each fix adds a channel aimed at the latest pool, and the money finds the next. The doc 42
   to 46 history is that pattern, repeated about 40 times.

None of this is a coding failure: every rule did what it said. What's missing is one rule at the bottom of everything.

## The direction (for the plan, if the user agrees)

### One rule for every purse

Every holder spends its needs plus a share of what it holds above its target:

    out = needs + a2 × (held − target)          target = k × its own turnover

Within each holder type, a2 and k fall within a bounded range. Every holder spends this way, each at its own time of
day or week.

What changes is only what the excess buys. That is where the fiction comes in:

| Holder | Target | What the excess buys |
|---|---|---|
| Household | a few weeks of its own spending | better food, wants, home help, commissions, feasts, the plate: what residents already do, folded into one rule |
| Business till | a buffer of about a month of its wage and materials bill | **everything above it goes to the owner** at the reckoning (the keeper, or the House). Losses: skip the payout, then the buffer, then wage cuts |
| Great House | k × its income (so a rich House has a rich resting fortune) | largesse: retainers' wages, feasts, commissions, church gifts, **founding and expanding businesses** (Victoria 3's investment pool). Rents as a share of the tenant's takings |
| Church | a few months of its outgoings | clergy, alms to the poorest by shortfall, and a fabric fund that *must* hire masons when it passes its own target |
| Town treasury | a few months of its outgoings | the watch, lamps and roads; **the town hires the idle for works when it's above target** (Wörgl, employer of last resort). Its spending follows its revenue, so it can't drain |
| Bank | – | custody with no interest, or no banks at all (the user's call) |

### Prices and wages

- **Prices are built from cost and moved by the shelf, each shop deciding for itself** (Lengnick).
  - The cost is the wage bill plus materials, divided by output.
  - The price moves in small steps, from time to time, within a band over cost, following its own stock.
  - The catalogue price becomes the starting point and an outer bound, not the target.
- **Wages move by each employer's unfilled or idle posts.** They don't follow the payers' books, and nobody sets one
  table for a whole town.
- **The jobless ask for less as time passes.**
- **The floor for the poor is alms and town works, not a wage floor.** A wage floor in a closed system pushes the
  imbalance somewhere else.

### Rich and poor stay, and stay put

Because each holder's resting balance is k × its income, differences in income become stable differences in wealth:

- a House with large takings sits at a large fortune;
- a guildmaster sits at a comfortable purse;
- a labourer sits at a small one.

None of them drifts. The rich are rich because they earn more, not because their wealth compounds. Keeping a2 bounded
away from zero for everyone stops the miser's Pareto tail. The church's alms and the towns' works are the modest pull
toward the mean that stops condensation (§4).

### The orchestrator steps back

- **What it becomes:** a monitor, and the DM's steering wheel.
- **What it stops doing:** acting as the economy's engine. Its channels go once the rule above has taken over what
  each one was for.
- **A new measure:** a monthly money-flow table by holder type. Each column must sum to zero, so it doubles as the
  audit that money is conserved. The column with a steady sign is the leak, and it shows the day it appears.

### Close the books

Each of these is funded from someone who already holds the money, or the money stays put:

- a new House's fortune;
- a new account's starting funds;
- a newcomer's purse;
- the bank's shortfall;
- the bandit hoard;
- the carter.

### The fallback, only if it proves necessary

- A small income tax, paid straight back out as town wages and alms (the χ in §4), never left lying in a treasury.
- If even that isn't enough: an occasional recoinage, a small fee on coin held at a set date, paid to the treasury,
  which must spend it.

Neither mints or melts money.

### Prove it small first

Before any C++, build a toy model (Python, a few hundred lines) with:

- one holder per kind;
- the flows between them;
- the rule above.

Check that a resting state exists and that it's reached from a bad start: all money in one House, or the treasuries
empty. This is the Machinations step the game designers recommend, and it costs a day instead of a month of 56-day
runs. Then the game: shadow mode first, as doc 46 did, then rule by rule.

## Questions for the user

1. **The orchestrator.** Should the one rule replace its channels (it stays as a monitor and the DM's steering), or be
   layered under them? Recommended: replace, step by step.
2. **Prices.** Should a loaf's price follow what it cost to bake, drifting from the catalogue as wages settle, and from
   town to town, within a band? Recommended: yes, with the catalogue as the bounds.
3. **Wages.** May a wage fall when a trade has more hands than work, with the floor under the poor coming from alms
   and town works rather than a minimum wage? Recommended: yes.
4. **Banks.** Custody only (no interest, no loans), or remove them? Recommended: remove, unless players will need them.
5. **The leaks.** Fund new Houses, new accounts and newcomers from existing holders: a new House from its founder's
   household, a newcomer's purse from the town's alms. Recommended: yes.
6. **Toy model first.** Recommended: yes.
7. **The spread.** Is a Gini of about 0.35 to 0.5 among households the right band, with a House's fortune several
   hundred times a labourer's? Or should the top be narrower or wider?

## Sources

- **Games and designer writing**
  - Simpson, "The In-game Economics of Ultima Online": https://gwern.net/doc/economics/1999-04-07-simpson-theingameconomicsofultimaonline.html
  - Koster, "UO's resource system": https://www.raphkoster.com/?p=517
  - Koster, "The evolution of UO's economy": https://www.raphkoster.com/?p=16263
  - Koster, SWG economy statistics: https://www.raphkoster.com/games/essays/astromech-stats-economy-stats/
  - Hastings, "The Model Economy": https://mud.co.uk/dvw/themodeleconomy.html
  - Wong, "A Working MUD Economy": https://mud.co.uk/dvw/workingmudeconomy.html
  - EVE's economist (interview): https://www.engadget.com/2009-02-19-massively-interviews-eve-online-lead-economist-dr-eyjog.html
  - Victoria 3, the market: https://vic3.paradoxwikis.com/Market
  - Victoria 3, Dev Diary 71 (investment pool): https://paradoxinteractive.com/games/victoria-3/news/dev-diary-71-autonomous-investment-in-1-2
  - Dwarf Fortress economy: https://dwarffortresswiki.org/index.php/DF2014:Economy
  - Armageddon MUD, shop coin: https://gdb.armageddon.org/index.php/topic,45278.0/all.html
  - Doran and Parberry, "Emergent Economies for RPGs": https://ianparberry.com/pubs/econ.pdf
  - BazaarBot: https://github.com/larsiusprime/bazaarBot
- **Economics**
  - Godley and Lavoie, model SIM: https://macrostat.readthedocs.io/en/stable/models/GL06/GL06SIM.html
  - Caiani et al. 2016 (agent-based stock-flow consistent model): https://doi.org/10.1016/j.jedc.2016.06.001
  - Lengnick 2013: https://legacy.econ.tuwien.ac.at/lva/compeco.se/artikel/jebo_2013_agent_based_macroeconomics_a_baseline_model.pdf
  - Gintis 2007: https://catalog.comses.net/publications/1574
  - Kalecki's profit equation: https://heteconomist.com/some-intuition-on-the-profit-equation-courtesy-of-kalecki/
- **Wealth distribution**
  - Dragulescu and Yakovenko 2000: https://arxiv.org/abs/cond-mat/0001432
  - Yakovenko and Rosser 2009: https://arxiv.org/abs/0905.1518
  - Chatterjee, Chakrabarti and Manna 2004: https://arxiv.org/abs/cond-mat/0402200
  - Boghosian et al.: https://arxiv.org/pdf/1511.00770
  - Bouchaud and Mézard 2000: https://arxiv.org/abs/cond-mat/0002374
- **History**
  - Recoinage: https://ehs.org.uk/article/periodic-recoinage-as-a-monetary-tax-conditions-for-the-rise-and-fall-of-the-bracteate-economy/
  - Wörgl: https://tronda.folk.ntnu.no/articles/worgl-money.pdf
  - Medieval largesse: https://ore.exeter.ac.uk/repository/handle/10871/30723

## Decisions (the user, 2026-10-14)

- **The orchestrator is replaced** by the one rule, step by step. It stays as the monitor and the DM's steering.
- **Prices are built from cost, all the way down.** The seller of a sword covers its wages and its materials and makes
  a profit. Those materials cost what it took to produce them and carry them, including the labour and every other
  cost that went into them.

  How cost works and develops is to be redesigned (below). The user also asked that every step of a chain pays
  someone. Wheat is grown and threshed, ground into flour, carried to the baker and baked into bread, and each step's
  hands and owner are paid. That has to work without making the people tilling the fields destitute.
- **Wages float** with the supply of hands, prices, and so on.
- **Banks are removed.** If an entity is needed to put money back or shave it off directly, it is the church or the
  towns.
- **No mid-game mints.** The great Houses get those funds at the founding.
- **A Python model first**, until we have something we like.
- **The Gini band:** to be decided from the model (explained below).

### What the Gini is

A single number for how evenly money is spread among households.

- **0 is complete equality:** every household holds the same.
- **1 is complete inequality:** one household holds everything.
- **About 0.5** is the spread that random trade alone produces when money is conserved. That's a "natural" level of
  rich and poor.
- **For scale, from real economies:**
  - Wealth Gini in Scandinavia is about 0.7, in the US about 0.85. Wealth is always more unequal than income.
  - Income Gini runs from about 0.25 (Denmark) to about 0.4 (US).
  - Medieval England's taxed households were probably at about 0.6 to 0.75 for wealth.

  Our households' purses are wealth, so somewhere in **0.4 to 0.6** is believable. The Houses sit outside the
  measure, far above it.

The other figure the model reports is **"poor½"**: the share of all household money held by the poorer half. With
equality it would be 0.5. Below about 0.05, half the realm is effectively penniless.

## How cost works (the design)

1. **Every lot of goods carries its cost.** A stock is valued at the average of what each unit cost its holder.
   - **Made:** the hours of work at the maker's wage, plus its inputs at what they cost the maker delivered, plus
     the tools worn.
   - **Bought:** the price paid, plus the carriage.
2. **Carriage is work too.** A porter or carter's hours at their wage, divided by the load, are added to the unit cost
   of what's carried.
3. **A seller's price is its normal cost plus a margin.**
   - **Normal cost** is the cost at full work: the wage over output per hour, plus the inputs, plus the wear. It is
     not this month's spending over this month's sales. Pricing on that average makes a business selling less price
     higher, and so sell less again: a death spiral the model fell into at once.
   - **The margin moves with the shelf.** A thin shelf raises it, a full shelf lowers it, in small steps, by each
     seller on its own day, between -30% and +60%.
4. **Profit is what's above a month's working money, paid out to the owner each month.** The owner is the keeper's
   household or the House. A business short of a week's working money is topped up by its owner from what the owner
   holds above its own needs. Owners investing is how profit comes back as others' sales (Kalecki).
5. **Short trade credit.** A business may take up to a week of its turnover in inputs on credit, paid back first from
   its takings and written off after 8 weeks. A mill with no money can still buy grain, grind it and sell the flour.
   (Medieval mills took a share of the grain as their fee for the same reason.)
6. **Rent.** Land the Houses own (farms, woods, pastures, mines) pays the House a share of its takings. That share,
   and their works' profit, is the Houses' income, and a House rests at k × its income: rich, but not growing.
7. **Wages float.**
   - Up a little each week a post stays unfilled.
   - Down a little after six calm weeks, but only while hands stand idle at the gate.
   - Down at once when the employer can't pay them.
   - The jobless ask for less as time passes.

### Production per end result

The user's worry: "a higher-production-per-end-result solution without making everyone tilling the field
destitute".

**Built prices protect the farmer by construction.** The price of bread contains the farmer's wage, the miller's, the
carter's and the baker's. Whatever productivity is, a day's farm work is worth a day's wage in bread.

**What productivity decides is how many hands each trade needs.** That in turn decides what everyone else does: crafts,
service, building, the watch.

**The game's own rates are far too productive** (`tools/econ_model/costs.py` on the live catalogue):

- **The daily basket:** a resident's daily food and household goods embody about **9 minutes** of work. A paid day is
  8 hours.
- **The whole catalogue:** at a 12p day, everything built from labour costs **3% to 30%** of its catalogue price.
  - A steel sword: about 3p of work against 90p.
  - A loaf: 0.04p against 1p.
- **What that means:** one farmhand feeds about fifty people. Almost everyone else must be paid for something other
  than making things.

This is a root of the pooling. Catalogue prices are far above what anything costs to make, so every sale leaves most
of its price as a margin in a till.

**The model's rates** are set to medieval levels instead:

- **Bread:** about 0.28 hours of work a loaf, two loaves a head a day.
- **Necessities:** bread, wood and clothes take about half the workforce. The rest work at finery, building, the watch,
  the church, the Houses' service and help about the home.

A labourer's day buys about eight loaves, and a family needs two each.

The game's production times (crafts.json, producers) will need the same calibration before cost-built prices can work.
This is a labour budget: the realm's needs should take roughly its working hours.

## The model (tools/econ_model, 2026-10-14)

`python3 tools/econ_model/model.py --start normal|one_house|empty_treasury|equal --days 1095` runs three years in
about 15 seconds. `diag.py` shows where the money sits by kind of household. `costs.py` gives the catalogue's built
costs.

**The realm:**

- 260 households, 887 people, 348 workers.
- 150,000p, minted once, and checked every day: nothing is made or lost.
- **The chains:**
  - Wheat from farms is ground at mills and carted to the city, where bakers bake it into bread with wood.
  - Wool goes to cloth, and cloth to finery.
  - Ore and wood go to iron, iron to tools. Farms and mines wear their tools out.
  - Wood and iron go to building works.
- **The holders:**
  - Three Houses own the mines, the smelters, the mills, the builders, the wool runs and the carter, and get rent
    from the land.
  - Keepers own the rest.
  - The church lives on a 6% tithe; the town on an 8% income tax.

**The rule for each:**

- **Households** buy bread, wood and clothes. They spend a share (30% to 70% a month) of what they hold above a month's
  income on finery and on help about the home, which hires the idle by the day.
- **Institutions** spend their month's income, plus 30-40% a month of what they hold over two months' income:
  - **The church:** alms to the short first, then clergy, then building.
  - **The town:** the watch, then town works, which hire whoever the budget pays (under the market wage while hands
    are idle, over it when none are), then building.
  - **The Houses:** retainers, finery, building.

### What it has shown so far

1. **The totals by kind of holder settle**, whatever the start: households, businesses, Houses, the church and the
   town each level off at a share and stay. The one rule does at the top level what it promises. In the normal start,
   year 3: homes ~95k, businesses ~36k, Houses 5-9k, church ~6k, town ~8.5k.
2. **The spread within households is now in a believable band** in the normal start:
   - Gini 0.44, poor½ 0.19, median 314p by year 3.
   - Wages level at about 5p a day and prices level with them. A fixed money supply finds its own price level, as the
     theory says.

   The empty-treasury and equal starts head the same way (Gini 0.54, wage ~5).
3. **Each failure on the way was a design rule.**
   - **Average-cost pricing spirals.** Price from normal cost.
   - **A business that pays out everything has no working money.** Keep a month back, and let owners top it up.
   - **Money with nothing to buy stops.**
     - Households need an outlet that always exists: help about the home, the plainest service.
     - The town's money must always be able to buy work: works that compete for hands.
   - **Shortages must raise wages, not margins.** Margins are capped. Hiring follows what buyers asked for, sold or
     not.
   - **A chain dies if one link has no money.** Short trade credit.
   - **Houses with no income drain.** Rent makes them rich.
   - **Asking for the same unmet want every day inflates demand.** Count each want once.
4. **Still wrong:**
   - **Bread runs out on some days in every start**: the whole town hungry on a sampled day. The firewood and flour
     links can't take on hands fast enough.
   - **The all-money-in-one-House start doesn't recover** (Gini 0.35, but hungry most days).
   - **Households without an earner (8%)** live on alms alone.
   - **Businesses hold about a quarter of all money** as working money. That may be right, or the buffer may be too
     big.

### Next

- **Supply:** businesses short of hands for a needed good take them on faster; bakers keep a few days of wood.
- **The one-House start:** see why the Houses' businesses don't restart.
- **The rates:** tune household and institution a2/k, and see how the Gini moves with them. That gives the user a
  real choice of spread.
- **Seasons:** harvests, winter wood.
- **Two towns with roads,** to see money and goods move between places.
- **Players:** a "player shock" test that dumps or drains goods or money and checks the economy returns.
- **Then the game:** the labour-budget calibration of the game's production times, and the order to bring the rules
  across.

## The worth ladder and production times (2026-10-14, second pass)

The user: production times must differ by material.

- **Fields take time.** They grow between sowing and harvest, but yield a lot for the work.
- **Mining is slow.** It's heavy work for little output.
- **There should be targets.**
  - Bread stays cheap and affordable to every wolf.
  - A bronze sword is harder to buy.
  - A rapier is a specialty sword, always expensive by comparison, and never a day's wages.
- **For the item editors.** The ladder must be in recognisable steps, from common to very rare. Atlas and the Dungeon
  Master will later let users create and edit items, place them on the ladder, and build their recipes.

### The ladder (tools/econ_model/ladder.json, ladder.py)

**Worth** is measured in a labourer's day-wages (**LD**). Money is closed and the price level floats, so worth is
relative: it stays put while pennies drift.

**Eight named rungs**, each about three times the one below:

| Rung | LD | For example |
|---|---|---|
| Staple | 0.01–0.1 | bread 0.06, firewood 0.04, wheat 0.025 |
| Cheap | 0.1–0.5 | iron ore 0.15, copper ore 0.2 |
| Everyday | 0.5–2 | plain clothes, tools, a bar of iron |
| Fine | 2–6 | the bronze sword 3, the iron sword 5 |
| Costly | 6–15 | the steel sword 10 |
| Rare | 15–40 | the rapier 25 |
| Precious | 40–120 | jewelled pieces, siege engines |
| Legendary | 120–400 | one of a kind |

**How an item is placed.** An item names its `rung` and an `at` from 0 to 1 within it, on a log scale. That's an
editor's slider. `ladder.py` converts both ways: rung and position to LD, and LD to rung and position. An item not
placed sits where its catalogue price puts it (price / 12 LD).

**Why "worth".** The word is chosen so as not to clash with two things already in items.json: `tier` (gear tiers:
basic, professional, exceptional) and quality ("Common" quality). A common-quality rapier is still Rare.

**How the editors will use it.** An editor places an item on a rung and builds its recipe. The calibration then gives
its work time, or says the recipe's inputs already cost more than the rung allows.

### Production times from the ladder (tools/econ_model/calibrate.py)

A good's built price is made up of:

- its work hours at its trade's wage;
- its inputs, at their own built prices;
- the carting, by weight;
- the tools worn;
- a 15% margin at each step.

Raw goods are worked out first, then everything made from them. The work time is what's left of the target once the
inputs are paid for. `--write proposed_rates.json` writes the proposal next to the script. **The game's data is
untouched.**

**Results on the live catalogue:**

- **Times rise sharply.** Producers' spells become 4–12 times longer, crafts 3–160 times.
  - Mining stays slow: an iron-ore spell yields less for more work than a farm spell.
  - A bronze sword becomes about 3.4 hours of a skilled smith; a rapier about 67 game hours of a master armourer.
- **The labour budget.** A head's daily bread and household needs come to **1.7 labourer's hours** (0.19 today).
  That's about half the realm's working day, the medieval balance.
- **110 of 361 recipes conflict.** Their inputs alone already cost more than the catalogue's price for the product
  (the iron chain especially). The calibration gives such work its usual share and raises the price. That list is
  the catalogue's fixing list.

**The skill premium is kept modest (`skillWage` 150: a skill-60 trade pays 1.4×).** The first model run showed why.
With plenty of skilled wolves, the market wears a big premium down to a labourer's wage, and the rapier fell from 25
LD to 12. A rare good's worth has to be carried by its **hours and its inputs**, which no market can wear away.

### The model, second version

`model.py` was rebuilt around the ladder.

**Its goods and chains:**

- **The grain chain:** wheat, ground to flour, baked into bread with wood.
- **The metal chains:**
  - Iron ore is smelted into iron. Iron goes to steel, tools, finery's fittings and building works.
  - Copper ore is smelted into bronze, and bronze worked into swords.
  - Steel is worked into rapiers by a master.
- **Wool,** woven to cloth, and cloth made into finery.

Every good's hours are calibrated from the ladder at the founding, and the goods the game shares read their rungs
from ladder.json.

**What's new:**

- **Seasonal fields.**
  - Sowing is in spring and the harvest at summer's end.
  - A farm sows for a year's sales until the next harvest is in. It allows for the store's spoiling (about a seventh
    of a year's grain) and a tenth over.
  - At sowing and harvest, a farm hires day labour at a quarter over the market. Hands in steady jobs don't leave
    them for it; that pulled every baker into the fields at harvest and the town went hungry.
  - Between seasons a farm keeps one hand.
- **Slow mines.**
- **Skilled trades.**
  - Workers have a skill. Trades need it, and skill grows with skilled work: the trades train their own.
  - Artisans are paid for the hours they work.
- **Whole items.** Swords, rapiers and finery are bought one at a time: no more buying 40% of a rapier.
- **The wage rule:**
  - A wage rises only after a post stands open most of the week. The first version raised it whenever a post was
    advertised, so every growing trade ratcheted its wage up.
  - It falls slowly after calm weeks while hands are idle.
  - It never falls under the going rate for the trade's skill. An employer that can't pay that lets hands go.
- **The carter** counts the loads it had to turn away as demand, and hires for them. Before, it only ever saw what it
  managed to carry, and it starved the city of flour and wood.
- **Relief comes first.** The church relieves the short first. The town covers what the church can't, before its
  watch.

### What it shows

1. **Every start converges on the same economy.** By year 3, from an even split, all money in one House, an empty
   treasury or equal purses, each settles at about:
   - homes 85–90k, businesses 40–50k, Houses 3–8k, church ~5k, town ~8k;
   - Gini 0.56–0.61, the poorer half holding 12–15%, a labourer's wage ~4.2p.

   **The economy now finds its own equilibrium.**
2. **The ladder holds in cost.** Built costs sit within about 20% of their rungs: bread 0.053 LD, the bronze sword 2.1,
   the rapier 19.6 (the targets less the 15% margin: 0.052, 2.6, 21.7).

   **Prices move around cost by their margins**, as the shelves tell them:
   - Staples sell dear (bread at 0.08–0.10 LD) while their chain is short of hands.
   - Luxuries sell cheap (sword 1.4, rapier 12–16) while their makers clear stock.

   The margins are the market's signals. They are capped at -30% and +60%.
3. **Still to settle:**
   - **Labour moves between trades too slowly.** The realm reaches full employment. The flour and bread trades then
     stay short while the finery trade holds its hands. Wages in short trades rise slowly; prices hit their cap.
   - **Luxury makers over-make in the first year.** Swords, steel and rapiers then have shelves of hundreds of days,
     which take years to clear.
   - **The occasional day with no bread** (the whole town short).
   - **Households with no earner** still depend wholly on relief.

### Questions for the user

1. **Players' crafting time.** recipes.json's times are the times players craft at too. Calibrated, a rapier is about
   67 game hours of work (a game day is 4 real hours). Options:
   - **(a)** Players work at the same times: a rapier is a long job, done over sessions.
   - **(b)** A player's crafting is quick, but uses up the hours another way (fatigue, or the game day passes).
   - **(c)** Players keep today's times. They'd then be ~50–150 times more productive than NPCs, and could undercut
     every market.

   Recommended: (a) for NPC-made goods; (b) where a quick craft matters to play.
2. **The margin cap.** Should prices be free to go higher than cost + 60% in a shortage, so price rations rather than
   empty shelves? Recommended: up to +100% for staples in a famine.
3. **The ladder's rungs and placings.** Are these the steps and names you want for the editors?

## Third pass (2026-10-14): decisions, mending, moving between trades

### The user's decisions

- **Players' craft time:** set aside for now. Atlas and the DM must let each item carry its own **player craft
  time**, set per item. It is **0 for every item for now**, and separate from the NPC work hours the ladder
  calibrates. (Noted in doc 34, "Later: item management".)
- **Staples' margin may reach +100%** in a shortage (others stay at +60%).
- **The rungs are fine.**
- **Trades short of hands outbid other day jobs** (the user's idea; built, below).
- **Mending** (the user):
  - Armour, swords and clothes shouldn't wear out and vanish. NPCs take them back to a maker to be mended, which
    costs materials and time but is cheaper than buying new.
  - Clothes and jewellery are the most disposable.
  - Every owned thing has a wear and mending cost, and some judgement in replacing it: price, an heirloom, wanting
    something better.
- **Founded goods start at random condition,** so the mending doesn't come in waves (the user; done from the start:
  everything founded starts between 0.35 and new).

### For the item editors (Atlas, the Dungeon Master)

What an item will carry, for the editors to set:

| Field | What it is |
|---|---|
| `worth`: rung + at | Its place on the ladder: rung name, 0–1 within it |
| recipe(s) | Inputs, outputs, trade, skill: what the editors build |
| work hours | NPC work for a batch, **proposed by the calibration** from the worth and the recipe; the editor sees a conflict when inputs alone cost more than the rung allows |
| `playerSeconds` | **Player craft time, set per item; 0 for every item for now** |
| wear | Days of use from whole to worn out |
| mending | The share of the making's hours and of its materials a full mend takes; the chance it's an heirloom; how readily owners replace it rather than mend it ("fashion") |
| better | What an owner may move up to (a bronze sword to a rapier) |

### Trades short of hands outbid other day jobs

A post that has stood open most of the week raises its wage by more the shorter it is: up to a tenth a week when half
its posts are empty.

Each day, a few of the wolves in other day jobs (4% a day) move to a post that pays a tenth more than their own, if
they have the skill for it. They come from wherever pay is lower. Nobody leaves a post they hold alone, and harvest
day labour draws only the idle.

**It helps:**

| At day 1000 | Without | With |
|---|---|---|
| Hungry in working households | 148 | 5 |
| Hungry in idle households | 48 | 4 |
| Idle | 19% | 0% |

The hands go where they're wanted, which is what the user expected.

**It wasn't the whole story for bread.** The rest of the bread shortage turned out to be two more things:

- **Staples need spare capacity.** Bakers and millers staff for a fifth over their sales. With exactly enough, whoever
  shops last goes short, every day.
- **Unmet demand has to reach every seller tried.** Before, only the cheapest heard of it, so the cheap bakeries hit
  their capacity while the others never learnt they were needed.

**And margins wear back.** A margin raised in a shortage drifts back toward the usual 15% once the shelf is neither
short nor glutted. Without this, bread's margin stuck at its cap: a bakery's shelf can never look overstocked,
because bread spoils.

With all three, bread, wood and flour sell at their rungs: bread 0.065–0.075 LD against 0.06.

**Two more fixes:**

- **Woodcutters lay in for winter,** from a month before it.
- **Rent is part of a landed business's cost.** Before, farms and mines with thin margins paid it out of nothing and
  their House propped them up.
- **Owners stop propping up a business** that has taken more than two months' working money in a quarter without its
  coming back.

The Houses' target is now 6 months of their income (a2 0.2), not 2. They settle rich (7–17k between the three) from
any start, including an even start that founds them with nothing.

### Mending (model)

**What wears.** Each household owns its clothes (a set a head), and may own a sword, a rapier and finery. The watch's
swords are the town's; each House has its rapier. All wear with use: a household's sword in about a year, a guard's in
four months, clothes in five, finery in eight.

**When a piece is worn to 0.35, its owner sees to it:**

1. **An heirloom is always mended.** That's 20% of swords, 40% of rapiers.
2. **A rich owner may move up.** One of the top sixth of earners, with cash for it, may move up to something better
   (a sword to a rapier).
3. **Sometimes the owner just wants a new one** ("fashion"): clothes 15%, finery 35%, blades 2%.
4. **Otherwise it's mended,** if the maker's quote is under 60% of a new one's price. Else it's replaced.

**The quote** is: how worn × (a share of the making's hours at the maker's wage, plus a share of its materials) × the
maker's margin.

| Good | Hours share | Materials share |
|---|---|---|
| Blades | 30% | 10% |
| Clothes | 50% | 30% |
| Finery | 60% | 40% |

**The maker's side.** It takes a mend only with hands to spare (half its day at most) and the materials in its own
store. The hours come out of the next day's making, and the mending it's asked for counts when it decides how many
hands it needs. A worn-out piece is kept, unusable, until it's mended or replaced: nothing vanishes. **Mending never
shortens a piece's life** (doc 35).

**Result (a month, year 3), mended against bought new, in pennies:**

| Good | Mended | Bought new |
|---|---|---|
| Swords | 68 | 0 |
| Rapiers | 55 | 102 |
| Clothes | 132 | 477 |
| Finery | 367 | 1,101 |

Blades are kept and mended. Clothes and finery are mostly replaced, as the user expected. 87% of working households own
a sword; 13% of households own a rapier.

**What households spend on.** "Rich" is now the top sixth by income, not a fixed sum: when it was a sum, households'
savings put 189 of 260 households into rapiers.

Households with nothing left to want now have work done on their homes (works from the builders). That's an outlet
that never runs out, so savings don't pile up when there are no idle hands to hire about the home.

### Where it stands (year 3, four starts)

- **Money settles by kind of holder from every start:**
  - homes 80–87k, businesses 34–52k, Houses 11–17k, church 3.3–5.4k, town 7–8k;
  - Gini 0.51–0.56, the poorer half holding 7–11%, wages rising slowly (about 7–8p).
- **The ladder holds:**
  - bread 0.07–0.09 LD (rung 0.06);
  - the sword 1.3–2.1 (3), dragged down by its glut;
  - the rapier 13–19 (25).

### Still wrong

1. **The bread chain breaks at the turn of the seasons.** On the eve of winter, and at the year's end, the whole town
   is short for a day or more. The wood and flour links, in a season change, still can't respond in time.
2. **Households with no earner** get more relief than they need in total, yet are short between Restdays: relief comes
   weekly and goes on other needs first. They want a daily dole, or relief in bread.
3. **The poorer half's share drifts down** in year 3 (to 0.07–0.11) as wages rise and keepers' and Houses' shares
   grow. It needs watching over 5–10 years.
4. **The sword maker's glut** keeps its price at the margin floor, about half its rung. Its trade is now mostly
   mending, and new swords are few.

## Fourth pass (2026-10-14): finding the causes

The user: swordsmiths at half their rung are fine, so long as they're employed and not hungry. They are: 1–3 at a
time, mostly mending, never hungry, often paid for part of a day.

For everything else, research what causes the problems and fix that. Two ideas from the user, both built:

- **Bakers give spoiling bread to poorer households,** the hungry first.
- **NPCs hunt for food,** and sell the hides.

Tools added: `trace_bread.py` shows every link of the bread chain on the days the town goes short. `who.py` shows
households' money by decile and what the poorer half's earners do.

### Causes found and fixed

| Symptom | Cause found | Fix |
|---|---|---|
| Whole town short of bread, at the founding and each winter's eve | **The carts.** One carter of 1–3 hands. When it lost a hand to a better-paying trade it waited for its weekly review to replace it. Wood sat at the woodcutters while the bakeries had none. | **A post left is open again at once.** A short employer can't be poached from. The carter hires the day it turns loads away. Staple makers keep a week of inputs. **When the carts are full, a business fetches what it needs with its own hands** (up to a third of their day, on foot at twice the carts' time). |
| Short at the end of each harvest | **The mills' wheat filled the carts,** and the bakeries' firewood waited behind it. | As above: the bakeries fetch their own wood. Carting keeps a fifth spare, like the staple trades. |
| Grain gone before harvest (all but the normal start) | **Sowing to last year's sales.** A realm whose spending was recovering out-ate the plan. | Sow by the year's rate or the last month's, whichever is higher. **After a year the store ran dry, sow up to half again.** |
| Grain gone before harvest (once everyone was employed) | **Harvest drew only the idle,** and there were none. Fields rotted. | **The harvest calls on everyone.** Hands from trades that aren't staples (cloth, building, journeymen) go to the fields for the season, and their posts reopen behind them. Masters' trades don't reap. |
| Hunger rising through each week, gone after Restday, with bread on the shelves | **Relief came weekly** and went on other needs first. | **Daily relief:** a day's food and fuel, from what the church (then the town) sets aside each week. The set-aside is sized by what relief took last week and a third over. A household keeps 10 days of food money back from its wants, and 8 from mending clothes. **Bakers give what would spoil overnight to the hungry, then the poor.** |
| Bread's margin stuck at its cap | **A margin raised in a shortage never fell.** A baker's shelf can never look overstocked (bread spoils). | **Margins wear back** toward 15% when the shelf is neither short nor glutted. |
| Bread short with bakers to spare | **Unmet demand reached only the cheapest seller.** Staple trades had no slack. | Every seller a buyer tried hears it was short. Staple trades staff a fifth over sales. |
| The poor were working households earning 2–6 days' wages a month | **Employers paid less than promised:** cloth makers broke, artisans paid by the hour with nothing to do. Their hands judged by the promise, not the pay. | **A wolf judges a post by what it paid last week.** Under 60% of the going rate, it looks for work daily. **Under 40%, it leaves.** |
| Finery and cloth shops holding 50–70 hands for 20 hands' work | **Mending turned away for want of materials was counted as want of hands.** Waiting owners asked again every day. | Materials-short mending is laid in as materials. A waiting piece is counted once a week. **A business short of materials hires only the hands its materials keep busy.** |
| Mass idleness with wages spiralling to 19–23p | **The harvest wage chased itself.** Farms paid 1.25× the going rate while holding most unskilled hands, so the going rate became theirs. | The going rate leaves out the farms. Farms pay 1.25× it at sowing and harvest and the going rate between. |
| The church at 15–19k and the Houses growing | **Their last outlet (building works) was often sold out.** Money with nothing to buy stops. | **The church's building fund keeps its own masons,** and **each House its estate crew.** Like the town's works, they take whatever the budget pays, so these purses always buy work. |
| Gini 0.64–0.68 by year 10 | **Builders' wages ran away to 20–28× the going rate.** Buyers' budgets are fixed, so a dearer builder still got the same pennies. Nothing brought a wage back once its posts were filled. | **No wage stays far over the going rate:** over 30% above it for the trade's skill, it slides back 5% a week whenever the employer has no open post. |
| Leather short in years of full employment | **Hides came only from hunters.** | **Sheep runs yield hides** (0.15 a measure of wool) and sell them to the tanneries too. |

### Hunting (model)

**The game.** The woods hold up to 3,000 beasts, breeding back at 2% a day (fastest at half that).

**Who hunts.** A wolf with no work that day may hunt. It brings in a beast with a chance of 0.5 × how full the woods
are.

**What a catch gives.**

- **The meat:** six loaves' worth of food, into the household's larder. The larder is eaten before any bread is bought,
  and half of what's left keeps another day.
- **The hide:** sold to a tannery at what the tannery offers. The offer rises 5% a week while it's short of hides and
  falls 3% while it isn't.

**Tanneries** make leather. Leather goes into swords (¼), rapiers (¼) and finery (⅕), as in the game's recipes.

**What it does.** Hunting is a safety net. It's busiest in lean years, when hands stand idle, and nearly unused at full
employment: about 1,900 hides in five years, the woods at full strength by year 5. Without it, in the normal start,
bread was a third dearer in year 5.

### Where it stands

**From every start, the totals by kind of holder converge within a few years and hold for ten:** homes 75–85k,
businesses 31–40k, Houses 17–25k, church 5–7k, town 7.5–9k.

**No shortages.** No day in years 2–10 has more than 300 hungry. The only hunger is the one-House start's second year:
households were still poor, and the shelves had bread.

**Prices:**

- Bread 0.06–0.08 LD (rung 0.06).
- The sword 2–3 (rung 3).
- The rapier slides to about 13 (rung 25) as the realm trains more masters and their wage falls toward the going
  rate.

Wages level off and then rise slowly (4–6p by year 10, 8.7p by year 20).

**Gini:** 0.40–0.45 in years 3–5, rising to about 0.50 by year 10. In a 20-year run it levels off at **0.58** from
year 16, with the poorer half holding 4–5%.

### Still to research

1. **The poorer half's 4–5% by year 20.** Who they are: households with few earners, and underemployed artisans. What
   would hold the share near 15%:
   - an income tax recycled as relief and town works, the χ of §4;
   - the masters' trades staffing nearer their real work;
   - more earners in poorer households (children grow up).
2. **Idleness rising late in the 20-year run** (80–100 idle), with wages rising.
3. **The rapier's rung.** Its worth should hold through scarce masters, not only hours. Options: masters trained
   slowly through apprenticeships with places; or the ladder raising a master trade's hours as skill becomes common.

## Fifth pass (2026-10-14): lives, masters, twenty years

**The user:**

- **Fewer masters.**
- **Simulate lives** on 20-year runs: death, and the birth of new NPCs shortly after, so the population stays steady
  and even, never growing much.
- **Age matters,** for every NPC, not just masters.
- **Succession:** the coming apprentice or a family member is the likely replacement for an old shop owner or master.

### Lives (model)

**Ages.** Every wolf has an age.

- **Children,** to 16, don't work.
- **At 16 a wolf comes of age.** It becomes a worker and looks first to a family member's trade, as its apprentice.
- **Workers retire at 58–66** and live on in the household as elders.

**Death.** The yearly chance of dying is 0.0004 × e^(0.085 × age), plus 3% under five: about 0.6% at 30, 7% at 60,
16% at 70.

**Birth.** Each death brings a birth 30–240 days later, into a household with a wolf of 18–40; the fewer children it
has, the likelier. **The population holds:** 881–895 over twenty years, from 895.

**Households grow and part.** A household of more than six sends a young adult (18–32) out to found its own, with a
fair share of the purse. The new household is kin to the old.

**When the last of a household dies:**

- Its money and goods go to its kin (households of the same family). With no kin, they go to the town.
- A business it kept goes to that business's best hand, the apprentice or journeyman. With none, it goes to the kin.

Households hold at 250–266.

**Founded with every age.** Earners are 18–56, the other members are children, and some households are elders.

### Skill and masters

**At the founding about one wolf in fifty is a master** (60–75). Before, it was one in twenty.

**Learning.** A wolf learns fast (8 a year) beside a better hand in its trade, and slowly alone (0.3 a year).

- It learns up to a few points short of the best hand.
- It always reaches its trade's own level where a hand there teaches: an apprenticeship ends.
- **It never goes further than its own trade's level and a little.** Only the master armourers' trade makes masters;
  a steelworker or a finery maker doesn't become one.

**Apprentices.** A skilled trade keeps one apprentice to each hand at its level, two while it can't fill its posts.

- A family's child comes first, then the young (under 24) without a post.
- An apprentice fills a post the trade is already offering, not one more.
- It's paid half until it reaches the trade's level.
- **Taking any other post ends an apprenticeship.** Before, a former apprentice kept half pay at the bakery: a bug.

**Masters hold at 4–5 for twenty years,** each replaced by an apprentice.

### Two more fixes the long runs needed

- **Mending counts as hands only for what was done,** plus one hand if mending was turned away that week. Counting
  every waiting piece every week had the finery shops posting 100–200 openings.
- **A short trade's wage stops at twice the going rate for its skill.** Before, a trade with posts it could never fill
  raised its wage 10% a week without end: swords at 9–17 LD, rapiers at 100–300. Past the cap, a shortage shows in
  its prices and its apprenticeships.

### What lives did to the poorer half

**Lives fixed what the economy rules couldn't.** Children grow into earners, elders live with their grown children,
and crowded households send young adults out with a share. Before lives, the poorer half held 4–5% by year 20 and the
Gini was 0.58. With them, through year 16, the Gini holds at **0.32–0.38** and the poorer half at **23–27%**.

### Twenty years, all four starts

**No shortage days.** In years 2–20 there is no day with more than 300 hungry, from any start.

| Start | Year 10 | Year 20 |
|---|---|---|
| Normal | Gini 0.38, poorer half 0.23, bread 0.080, sword 2.7, rapier 27 | Gini 0.35, 0.24, bread 0.074, sword 2.8, rapier 41 |
| All in one House | 0.37, 0.24, 0.065, 2.2, 30 | 0.37, 0.24, 0.064, 1.9, 49 |
| Empty treasury | 0.36, 0.25, 0.088, 2.7, 19 | 0.34, 0.25, 0.064, 2.6, 29 |
| Equal purses | 0.37, 0.23, 0.064, 2.2, 27 | 0.34, 0.25, 0.070, 2.6, 24 |

**Lives:** the population stays at 888–893 and households at 251–262. Masters run 2–5, apprentices 17–88, elders
31–46.

**Money by holder settles the same from every start:** homes 82–83k, businesses 29–32k, Houses 25–27k, church about
4.5–5k, town about 6–6.5k.

### Still to look at

1. **Wages drift slowly down,** from about 2.5p in year 10 to about 2p in year 20, with prices following. Worth stays
   on its rungs, but **the game counts in whole pennies**, so the price level the money supply settles at matters.
   Options:
   - size the money supply at the founding so the level sits where pennies work;
   - give prices a finer unit (farthings, or prices kept in hundredths);
   - find what still pools.
2. **The rapier rides at the top of Rare** (24–49 LD) where masters are fewest. This is the scarcity the user asked
   for, but sometimes past the rung.
3. **"Idle" in the tables** goes below zero. It counts days of home help against wolves, and is to be measured by
   wolves.

## Sixth pass (2026-10-14): the money supply

**The user:** wages drift slowly down. Would 1,000,000 pennies in all, rather than 150,000, crash an economy built
on daily wages? Simulate it for 20 years.

**It doesn't crash.** In a closed economy whose prices and wages float, the amount of money sets the price level
only. A founding wage of 12p against 1,000,000p is too low for so much money. In the first year wages rise to about
24p and prices with them, then settle. From year 3 on, everything real matches the 150,000p run:

| 20 years, normal start | 150,000p | 1,000,000p |
|---|---|---|
| Gini, years 3–20 | 0.34–0.37 | 0.33–0.37 |
| The poorer half's share | 23–26% | 23–27% |
| Bread / sword / rapier (LD) | 0.066–0.080 / 2.3–2.9 / 19–49 | 0.056–0.063 / 2.1–3.6 / 25–41 |
| Hungry, shortage days | none | none |
| A labourer's day, year 20 | about 2p | about 12p |

**1,000,000p puts the labourer's day where the catalogue assumes it (12p).** Pennies stay whole: bread at 0.06 LD is
about 0.7p, a sword about 30p, a rapier about 300p.

**The slow drift isn't the money supply.** Wages fell about 2.5% a year at either supply. The cause was the Houses'
target: they rested at 6 months of their income and kept gathering (173k to 209k), and money held still is money not
spent. **At 4 months** (a2 0.3), they hold steady at 110–137k, about 40k each, years of a labourer's wages: still the
rich. Wages run nearly flat from year 8 (15, 15, 13, 14p), and everything else is unchanged.

**The model's defaults are now 1,000,000p and the Houses at 4 months.** `--money` sets the supply.

**For the game:** found with about 1,000,000p for a realm of this size: about 1,100p a head, or about 90 days of a
labourer's wage. That is the money supply at which the catalogue's prices are the realm's own.

## Seventh pass (2026-10-14): players

**The user:**

- **Players will hoard.** Find when too much money is held unused, and inject money back, **but only if that hoarding
  is harming the economy.**
- **Players will crash markets on purpose.** That's fine in the short term; the markets must be flexible enough to
  rebound.
- **Large disruption is fine, so long as it normalizes over time.**

**The harness.** `tools/econ_model/players.py SCENARIO [--mint]` runs the model with players doing one thing from the
start of year 4. Players are accounts of their own. They earn from contracts (paid by the town and the Houses), buy,
sell from stalls, and hire.

The model also now gives the same results on every run: households' kin are visited in a fixed order.

### The mint (`--mint`): a thermostat on the money in use

**Hoarded money** is dormant money above the realm's own normal.

- **Dormant money** is what any purse holds beyond two months of its own spending.
- **The normal** is measured in year 2, before players come (in the game: before launch). It's 12–15% of all money,
  because every purse rightly keeps a reserve.
- It's averaged over four weeks.

**Money in use** is all money less hoarded money.

**Harm** is any of: wages falling more than 3% in 8 weeks, more than 10% of wolves idle, more than 1% hungry.

**Strike.** When money in use is more than 5% under what the realm was founded with **and there is harm**, the mint
strikes the difference, up to 1% a week. It goes into the town treasury, which pays it out as works and relief, so it
reaches wolves as wages.

**Melt.** When money in use is more than 5% over (hoards spent again) and prices aren't falling, it melts the
difference, up to 2% a week, from the treasury. **It never melts more than it struck.** That keeps it honest: it only
undoes its own coin.

Two earlier designs failed:

- **Chasing the hoard week to week** was noisy. It struck and melted in turn and let wages fall 40%.
- **Melting only as the hoard shrank** never caught up. Spent hoards land in NPC purses, so the hoard measure barely
  falls, and money ended 62% over.

### Hoarding: 20 players earn about two labourers' days a day each and keep 95%

**Without the mint.** By year 8 the players hold 686k, two-thirds of all money. Wages fall 64% (17p to 6p) and prices
with them. **Nothing real suffers:** Gini 0.33–0.37, the poorer half about 25%, no hunger, bread on its rung.

In this model hoarding's only harm is deflation, because prices bend. In the game deflation is harm all the same:

- prices fall below whole pennies;
- a hoard buys ever more.

The mint's harm test counts it.

**With the mint.** It strikes in step with the hoard: 810k struck against 868k hoarded, the players holding 1.02M
(over half of all money). Wages drift about 30% lower over five years rather than 64%. Everything else holds.

**Then the players spend it all in a year.**

- **The disruption:** the Gini goes to 0.39–0.40, the poorer half falls to 22%, wages jump about 70% (12p to 21p),
  and the rapier reaches 56–58 LD.
- **Then it normalizes:** the mint melts 550k of the 810k over the next three years, wages come back to 17p (the
  no-player level), and the Gini to 0.37.

Slower than the burst, but steady.

### Crashing markets (one player, each from the start of year 4)

| What the player does | Disruption | Back to normal |
|---|---|---|
| Sells a month of the realm's bread at half price, over 30 days | Bakeries shed 44% of their hands. One day with 374 hungry when the cheap bread ran out and the bakers were still taking hands back on. | **About 4 weeks** after the dump: hands back to ~50, bread on its rung |
| Sells 200 swords at a third of their price | Sword price down ~20% | **~18 months**, as the stall sells out (200 swords are years of the realm's demand). The swordsmiths stay employed, mending, back to 4–6 hands within a month |
| Buys 60% of the grain store in spring, holds 120 days, sells it back | Wheat up ~30%. Farms sow more. Bread hardly moves; no hunger | **A few months** after the sale |
| Buys 90% of the grain store just before harvest (the worst time) | Wheat up ~25%. The harvest refills the store; no hunger | **Weeks** |
| Hires 40 wolves away from their trades for 60 days at twice the going rate | Bakeries lose a few hands. Wages up ~10% | **2–4 weeks:** short trades refill from the idle and other trades |

**Why they rebound:**

- Every price and wage moves with its own shelf or post.
- Hands move to where they're wanted.
- Trades keep spare capacity and working money.
- Relief and hunting catch the poor in the gap.
- Spoiling goods are given away, not hoarded.

**The slowest recovery is a glut of durable goods** (swords): the market recovers only as fast as the realm needs
them.

### Still to look at

1. ~~The mint's melt is drawn from the town treasury.~~ Spread over every purse (below).
2. **Deflation under the mint, about 30%,** comes from its 5% dead band and its harm gate. Kept as it is, the user's
   call: the swings are for players to play with.
3. **The game's version** needs its own normal, measured on the live world before launch. It also needs the mint's
   strikes and melts recorded in the ledger, where the game already keeps "minted" and "sunk".

### The melt from every purse (the user, 2026-10-14)

**The user:** spread the melt across all institutions and purses, even the Houses, evenly. Leave the deflation alone:
big, dramatic swings are fun for players to work with.

**Built.** The mint now melts **the same share of what each purse holds**, like a recoinage's fee: the town, the
church, the Houses, every household, business and player. Households with under a week's food money are spared, so a
melt never leaves anyone hungry.

**Result**, in the hoard-then-spend run:

- Money in use is back to about 1.08M within a year of the hoard being spent. Melting from the town alone left it at
  1.26M after three.
- Wages settle at 17–19p, the no-player level.

The Gini stays at about 0.37 either way, so the town's melt wasn't the cause. It's where the players spent their
hoard: on finery and works, so it landed with keepers and builders. It's left to settle with time.

**The deflation under hoarding stays as it is**, the user's call: swings are part of the game.

## Eighth pass (2026-10-14): players' keeps

**The user:**

- **Players may found keeps,** a large-group effort and not every player's.
- **Each is effectively a small town,** smaller than a town, with 1–3 crafting slots of various sorts (smithy, baker,
  crafts), farming, and housing for NPCs (bunkhouses or family houses).
- **The concern is hiring NPC workers:** keeps' population needs must not affect the economy too much.
- **The test:** long-term runs where, from year 1 on, keeps pop up and fade away over time. Test 1–10 keeps, and watch
  the population and the economy.

**The harness.** `tools/econ_model/keeps.py KEEPS [--staffed] [--crowd] [--draw]`, 20 years.

- **When.** A keep is founded on a day in years 1–15 and fades 2–8 years later. When it fades its workshops close and
  their hands are let go.
- **What it has.** 1–3 workshops (bread, tools, swords, cloth, leather, finery, building works) of 3–6 hands each, a
  farm, and houses for 10–30.
- **How its workshops behave.** They're ordinary businesses in the realm's market: they hire from the realm's wolves,
  buy, sell, and price by the same rules.
- **Its money.** Its players' money is what they've earned, taken from the town and the Houses (nothing minted). They
  go on earning from contracts, put money back into short workshops, and spend a little.
- **`--staffed`:** a keep fills every place, paying a quarter over the going rate.
- **`--crowd`:** the stress test, all ten keeps at once from year 5 to 15, each with three workshops of six and 30
  houses: 180 places and 300 houses.

**Population, two ways:**

- **Grow** (the default). A keep's houses raise the realm's number. Births fill it, and when the keep fades the
  number falls back and deaths go unreplaced until the realm is back to it.
- **Draw** (`--draw`). The realm's number stays put, and the keeps' hands come out of the towns.

**The mint now counts only players' money** (players' purses and keeps'). Counting NPC purses had it striking about
10% over 20 years with no players at all: the NPC economy's own dormant share drifts, and seasonal dips count as harm.
With no players, money now stays at exactly 1.0M for 20 years.

In the hoard-then-spend run the change also improves the mint:

- wages dip 10–20% while the players hoard, rather than 30%;
- after the spending, money is back to 1.04M within about two years.

### Results

| Run (20 years) | Keeps at once (most) | Keeps' hands (most) | People | Gini / poorer half while keeps stand | Hunger | After they fade |
|---|---|---|---|---|---|---|
| No keeps | 0 | 0 | 885–894 | 0.33–0.39 / 0.22–0.26 | none | — |
| 10 keeps over the years | 7 | 26 | up to 1,031 (grow) | 0.36–0.40 / 0.20–0.23 | none | as before |
| 10, staffed (grow) | 7 | 27 | up to 1,027 | 0.36–0.38 / 0.23 | none | back to 0.35 / 0.25 |
| 10, staffed (draw) | 7 | 30 | 886–894 | 0.36–0.38 / 0.22–0.24 | none | back to 0.36 / 0.24 |
| **Crowd** (grow) | 10 | 98–164 | **889 → 1,193** | **0.40–0.42 / 0.21** | none | Gini back to 0.36 in about 2 years; **people still 1,123 at year 20** |
| **Crowd** (draw) | 10 | 108–149 | 885–889 | **0.40–0.42 / 0.20–0.21** | none | back to 0.34 / 0.26 in about 5 years |

**What it shows:**

1. **Ordinary keeps barely touch the economy.** Even fully staffed, seven at once hire only 26–30 wolves between
   them: their workshops share the realm's buyers, and the skilled hands they'd need are few. Bread, swords and
   rapiers stay on their rungs, and nobody goes hungry.
2. **The crowd is a real shock, and it normalizes.**
   - **While they stand:** 10 keeps employing 100–160 wolves (a quarter to a third of the workforce) at a quarter over
     the rate lift wages about 60% (17p to 26–29p) and push the Gini to 0.40–0.42. Nobody goes hungry, and staples stay
     on their rungs.
   - **When they fade:** wages are back within about a year, and the spread within 2–5 years.
3. **Draw keeps the population steady; grow leaves a bulge.**
   - Under **grow**, the realm swells by a third to fill the keeps' houses.
   - When they fade, the extra wolves are still there. With births only following deaths, the realm takes decades to
     come back to its number: 1,123 against 895 at year 20.
   - In the game those wolves would need homes in the towns.
   - **Draw** has the same economic effects with a steady population.

**Recommendation:**

- **Draw.** A keep's houses are filled by wolves moving from the towns, and its workshops hire from the realm's
  wolves.
- **The towns refill** through their usual births-after-deaths. The realm's number stays put.
- **When a keep fades,** its wolves move back into the towns' homes, as these runs had them.

**Decided (the user, 2026-10-14): draw.** A keep's houses are filled by wolves from the towns. The realm's number stays
put, and the towns refill by births after deaths. A faded keep's wolves go back to the towns. `keeps.py` now draws by
default; `--grow` keeps the alternative for comparison.

## Ninth pass (2026-10-14): a hundred years

**The user:** simulate 100 years of the economy and see if anything substantial stands out.

`tools/econ_model/century.py [--years 100] [--keeps N]` prints a line a decade, then what drifted from years 3–10 to
the last decade. It runs in about 9 minutes.

Two runs:

- the NPC economy alone;
- 30 players' keeps founded and fading at random through the century (fully staffed, drawing on the towns), with the
  mint on.

### The first century: a collapse at years 30–50

**For about 25 years everything held. Then, in both runs:**

- the Houses, the church, the town and the businesses drained to almost nothing;
- nearly all money ended up in a few households (Gini 0.67, the poorer half holding 0.1%);
- wages fell to about 0.1p (a 99% deflation);
- the last masters died out by year 40.

Nobody starved: bread stayed on its rung against the collapsed wages, and given bread, hunting and relief fed the
poor.

**The cause, a chain:**

1. **A short-handed business sized its working money by every post it offered.** The rapier maker, short of masters,
   offered 300+ posts it couldn't fill. So it sat on a growing pile for decades: the slow deflation from year 20.
2. **Around year 34 the business passed to an apprentice's household.** As its posts shrank, it paid the pile out to
   that household as profit, 25,000–63,000p at a time, against a usual household's 1,000p a month.
3. **A household couldn't spend a windfall.** It bought one thing a day, finery to a cap, and help only from the idle.
   The money stood still, prices fell, and everyone else's income followed.

**Fixed:**

- **Working money counts the hands a business has and at most two posts.**
- **Posts grow by steps:** at most twice its hands and two.
- **A household buys building works by the amount, not one a day.**
- **A household holding more than half a year's income keeps servants:** posts of its own at the going rate, as many
  as a tenth of its excess pays for a month.

### Masters, and the user's succession rule

**Masters still died out after that, for a run of reasons, each fixed:**

| Cause | Fix |
|---|---|
| Apprentices on half pay at shops that pay by the hour fell under 40% of the going rate, so they quit | **Bound to their master** until the apprenticeship ends (indentures) |
| A new hire's pay record still held its idle week, so it "quit for low pay" the next morning, every master the shop hired included | **A new post starts a fresh pay record** |
| Apprentices filled every post the rapier shop offered, so no master could get one | **Apprentices are hands over and above:** posts count full hands only |
| Idle masters sampled a few random posts and missed the one that needed them | **A masters' trade with a post open seeks out a master** (idle, or working below its level) |
| Skill-30 trades (finery, swords) with dozens of qualified hands each took dozens of apprentices out of the bakeries and fields: 90–120 of 500 workers on half pay | **Only masters' trades (skill 50+) take the young from a post.** Others take the idle young, and **at most a third of a shop's hands are apprentices** |
| Shops paying by the hour (floor 20% of a day) kept hands they had no work for, at no cost, and those hands lived on a fifth of a wage | **The artisans' floor is 60% of a day's wage:** overstaffing costs, so shops trim |

**The user's succession rule.** When a master dies or retires without a properly trained apprentice, the shop goes to
the most talented wolf in the field who isn't already in a master's place or an important position. That's a
qualified hand in another skilled trade, a keeper in its own shop, or the clergy. The successor inherits the workshop
and works toward mastership.

**As built:**

- **When a masters' trade (skill 30+) is left with no hand at its level:**
  - its best apprentice steps up;
  - with none, the best-trained wolf in the realm not in such a place takes the shop.
- **Ownership** passes to the heir's household, unless a great House owns the shop: then the House keeps it and the
  heir runs it.
- **The head of a shop learns at the bench, 4 skill a year,** with no better hand to learn from, until it reaches the
  trade's level. Alone it was 0.3.
- **A masters' trade with no hand at its level and no head** appoints one at its weekly review. It doesn't wait for a
  master who isn't coming.
- **It also covers an apprentice who dies before its master.**

### The last century (all of the above)

| 100 years | NPC alone | With 30 keeps |
|---|---|---|
| People / households | 877–895 / 248–268 | 875–895 / 252–271 |
| Masters (≥60) | 1–5 throughout; 20 successions to an untrained heir | 1–5; 34 successions |
| Bread / sword / rapier (LD) | 0.057–0.063 / 1.6–1.9 / 13–30 | 0.057–0.061 / 1.7–1.8 / 13–34 |
| Wages | 15–47p (no runaway either way) | 14–41p |
| Hunger | worst day 159; days over 50 hungry: 0–13 a decade | worst day 170; 0–14 a decade |
| Money by holder | steady: households, businesses, Houses, church and town each within ~20% of years 3–10 | the same |
| Gini / poorer half | 0.39 → 0.56 / 0.22 → 0.08 | 0.40 → 0.58 / 0.21 → 0.07 |

### What still stands out

**One slow trend.** Over the second half of the century the spread among households widens: the Gini from about 0.40
to 0.55–0.58, the poorer half from about 21% to 7–8%. It's within the 0.4–0.6 band set as believable, and it hasn't
run away. In earlier long runs it swung up and back over decades. But it's the one thing still moving.

The likely causes are long-run ones the short runs couldn't show:

- **inheritance gathering in lineages** (estates go to kin);
- **skill gathering in families** (apprentices come from the family's trade first);
- **households with few earners** in the long tail.

It needs a century-long look by lineage and by trade before any fix.
