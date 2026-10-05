# 42. Money in circulation: taxes, tithes, wages, work and trade between towns

Planned 2026-10-04. It follows doc 35's Part 7 (buyers and contracts for goods) and keeps doc 15's money rules: **no
money is ever made to keep the economy going.** Coins only move, so the work is keeping them moving: out of the purses
where they pile up and back to the wolves who spend them. All numbers are *placeholders* for the balance pass.

## The user's brief

- More buyers for goods.
- Every grown wolf has a job, or some way to make money.
- Contracts open to NPCs, not just players.
- The town takes a tenth of everyone's monthly **profit** (not revenue), and the church takes a tenth too: everyone
  tithes.
- Towns and churches spend what they take in: contracts that put money back into the system, and charity for poor
  wolves, who save money on free food or care.
- One day a week, part of each town goes to church for a few hours, and preachers have sermons to preach.
- Caravans carry goods and money from one town to the next, and some regions have goods others lack.
- Never inject money artificially.

## Decisions (2026-10-04)

1. **Each town keeps its own purse.** A town's tax goes to its own treasury, which pays its own wages and buyers. It
   sends the capital a tenth of what it collects. The weekly sweep of every town's cash to the capital ends.
2. **Church is on Restday**, the day nobody works.
3. **The month is 28 days** (four weeks), so the reckoning always falls on the same weekday (Dawnday). That is about
   4.7 real days.
4. **Profit is everything earned less everything spent**, living costs included: food and household goods count
   against it, as well as materials and stock. A month at a loss pays nothing, and a loss isn't carried over.

## The rule against hoarding (the user, 2026-10-04)

Every place money gathers has a rule that sends it back out. That covers the great houses, churches, cities, towns and
the capital. None of them may sit on more than it needs.

- **The reserve.** Each collector keeps a reserve: four weeks of its own regular outgoings (wages, its buyers'
  baskets, its businesses' floats). Above it is the **surplus**.
- **The surplus goes back out**, a tenth of it each day (*placeholder*), through whatever suits the collector:

  | Collector | How it spends a surplus |
  |---|---|
  | A town or city | Hires more day labourers; puts up work contracts for projects (mending, paving, walls, a new well); buys more of what its buyers use up, so the watch eats better and more is mended; funds alms through the church |
  | The capital | The same, and grants to poorer towns' treasuries |
  | A church | Alms and free meals; care for the sick poor; more candles, incense and bread; repairs to the church; feast days |
  | A great house | Takes on more help at its businesses; commissions from crafters (clothes, jewellery, furniture: fine goods bought and kept); feasts for its people; projects on its lands; charity in its own name |

- **Spending is real.** It pays wolves for work or goods, or buys goods that are used up. It never destroys money, and
  it never hands out money for nothing, except as alms to the poor.
- **Visible.** Each collector's reserve, surplus and what it spent the surplus on show in the Money view (Phase 8), and
  "Ridgemere hires twelve hands to pave the market" is an event players hear of.
- **Not a sink.** A collector short of its reserve just stops spending its surplus. It doesn't sell, borrow or tax
  harder.

**Built 2026-10-04** (`Core/RatwSurplus.cpp`, `Society::spendSurpluses`, once a game day after the houses):

- **The collectors:** every town treasury (the capital's too), every church and every great house.
- **The reserve:** 28 days of its usual spending, a slow daily average of what it pays out in the ordinary way (wages,
  funds, props; not surplus spending), kept in memory only. Never less than a floor:
  - a town: 30p a resident;
  - a church: 10p a resident;
  - a house: 200p, plus two floats for each business it owns.
- **The surplus:** a tenth of what is above the reserve, 10p at least, is spent each day:
  - **A town:**
    - Half goes to hands hired for its works, 4p each: its day labourers, out-of-town workers and those looking for
      work.
    - The rest buys its Town Works' materials from its shops (stone, timber, limestone, cord, nails, planks, bricks),
      used up in the works.
    - A town with nothing to buy and nobody to hire gives it to its church, for the poor.
  - **A church:**
    - Seven tenths buys food from its town's shops. The town's poorest (under 12p, with nothing to eat) get one each as
      `alms`, and the rest is eaten at the church's table.
    - The rest buys candles, burned in the church.
  - **A house:**
    - Four tenths goes on 4p bonuses to everyone working at its businesses.
    - Half of the rest goes on a feast, eaten.
    - The rest commissions fine goods (4p or more, not food), kept at the house and out of circulation as goods.
- **Never a shop's last few:** a collector buys at most half of what a shop has of anything, keeping one back.
- **Logged:** a `surplus spent` event for each collector each day it spends, saying how.
- **Not yet:**
  - work contracts and projects for players (Phase 4);
  - grants from the capital to poorer towns;
  - care for the sick poor.
- Tests: `roads_tests` `noHoarding` (a rich church feeds a penniless wolf).

## Where we stand (measured on DEV build 22's export, 2026-10-04)

`world_check` now prints where the money is, by kind of holder, at the start and end of a run.

| Holder | Money | How many | Median |
|---|---|---|---|
| The capital's treasury | 20,000p | 1 | |
| Merchants | 36,533p | 240 | 156p |
| Paid civilians | 43,830p | 906 | 38p |
| Guards | 3,668p | 97 | 36p |
| Unpaid civilians | 9,470p | 96 | 0p |
| Other towns' treasuries | 0p | 13 | |

What moves it today:

- **One faucet.** The capital's treasury pays every paid position 2p an hour of work, up to 6p a day, in every town.
  "Paid" covers nearly everyone: shop help, farmers, nobles receiving guests, beggars, and 103 children "playing about".
- **Two drains back.**
  - **Market dues:** any merchant over 150p pays 30p each time it trades. In the first three game minutes, merchants
    paid 3,417p into the treasury.
  - **The weekly sweep:** every other town sends all its cash to the capital.
- In between, residents buy food and goods from shops, shops buy materials from suppliers and producers, and the town's
  buyers spend treasury money in its shops.
- **The unpaid:** 72 children, and 24 adults: lords and ladies, heads of great houses, foremen, students and three
  beggars.

The work below replaces the faucet and the drains with real employers, a tax and a tithe on profit, and real spending
by towns and churches.

## Phase 1: town purses and the monthly reckoning

**Built 2026-10-04** (`Core/RatwReckoning.cpp`; `MonthBooks` in `RatwSociety.h`, saved with the society as `books`):

- Each town's treasury is its store; `Society::treasuryOf`, `churchOf` and `treasuryOfResident` find them. The
  capital's money is divided once by population (`roads.purses`), and market dues and the weekly sweep are gone.
- A town's buyers are funded by its own treasury. The church no longer is (`"funds": "tithes"` in crafts.json): when
  first opened, it gets a four-week foundation from its town (at most a tenth of the town's purse).
- The books open on the first moment of life (or the first new day of an older save) and at every reckoning.
  Inheritance and operator transfers are kept out of profit. A newcomer starts its books on its first day.
- The reckoning runs at midnight when `day / 28` changes. It is a `reckoning` event per town, with its line in the
  event log, and every payment is a ledger entry (`town tax`, `tithe`, `capital's share`).
- **The Dev Console's `/reckon`** runs it at once, and `world_check --reckon` does at the end of a run.
- Tests: `roads_tests` `townPursesAndTheReckoning` (purses, wages by town, the founded church, profit, an inheritance
  and a loss, the capital's share, the event, the books through a restart, the 28th day).

**Town purses**

- Each town's treasury is its store's account: the capital's is `treasury`, the others' are `stores:<town>`. In a
  world of one town, it is the treasury, as now.
- Wages are paid from the treasury of the town the work is in, and a town's buyers are funded from its own treasury.
- **The purses divided**, once: the capital's treasury shares its money out among the towns by how many live in each,
  keeping its own share. It is recorded as `town purse`.
- **Removed:** market dues and the weekly sweep.

**The books**

- Every resident has books for the month: its cash when the month began. At the reckoning,
  `profit = cash now − cash then − what came in that wasn't earned + what went out that wasn't spent`.
- Not earned: inheritance, gifts from the Dungeon Master (operator transfers), and last month's tax and tithe
  themselves.
- A resident new this month (a newcomer, a birth) starts their books when they arrive, and pays from the next month.

**The reckoning** (midnight at the start of every 28th day)

- The **town tax** is a tenth of profit, rounded down, paid to the treasury of the resident's town. A town other than
  the capital sends a tenth of what it collects on to the capital (`capital's share`).
- The **tithe** is another tenth, paid to the church of the resident's town (`town:<town>:church`).
- Each is an event in the log, with the town's totals: "Amberford's reckoning: 412p in tax from 61 wolves, 412p in
  tithes".
- **Players** are left out for now (open question 1).

**The church lives on its tithes**

- The treasury stops funding the church. Its basket (candles, and bread for alms) is paid from tithes.
- To start, each church receives a **foundation** from its town's treasury: four weeks of its basket. This is a
  one-time transfer.

## Phase 2: who pays wages

**Built 2026-10-04** (`Core/RatwWages.cpp`, `Society::payerOf`): as the table below. A guard is paid by its town; a
producer (crafts.json `producers`) and anyone under 16 by nobody; the clergy (chapel, priest, acolyte, cathedral,
choir...) by the church; anyone working where a shop or a great house's head works (a lord "ruling House Fell", a
keeper of a Hall or Manor) by that keeper; everyone else by the town. An employer who can't pay doesn't, and the
worker's reason says "Waiting on wages from Holly Ashwalker." Not yet: looking for other work after a week unpaid.
Tests: `roads_tests` `whoPaysWages`.

The treasury stops paying everyone. Each position is paid by whoever it works for:

| Position | Paid by |
|---|---|
| A shop's or workshop's help ("helping at The Gatehouse Inn", "works at the mill") | The keeper of that business, from its takings |
| The watch, lamplighters, sweepers, messengers, the Town Works' own, the court and council | The town's treasury |
| Priests, acolytes, chapel keepers, choirmasters | The church, from tithes |
| A great house's staff | The head of the house |
| Farmers, fishers, woodcutters, quarrymen, shepherds | Nobody: they live by selling what they bring in |
| Children | Nobody: under 16, no wages |

- Positions are sorted by where they work, and by their work label for those working in the open.
- An employer that can't pay this hour doesn't pay, and the worker says so ("Waiting on wages from Holly Ashwalker").
  After a week unpaid, the worker looks for other work.
- Wages stay 2p an hour, 6p a day, for now. A shop's help is paid by the shop, so a shop that sells well can afford its
  help, and a quiet one can't.

## Phase 3: a living for every grown wolf

**Built 2026-10-04** (`Core/RatwResidents.cpp`, with `payerOf`):

- **Day labour.** A wolf of 16 to 64 with no post and no apprenticeship is "labouring for the Town Works": in its
  usual hours at a spot on its town's square (or where it spends its days, without one), paid 2p a spell, 6p a day, by
  its town (`day labour`).
- **Unpaid posts are paid.** A grown wolf's unpaid post (a foreman "overseeing the ironworks", a clerk "keeping the Fell
  accounts") now earns from whoever it works for, as in Phase 2. Not a beggar's, nor a great house's head's.
- **Retired:** 65 or more with no post. One who holds a post keeps working.
- **Unpaid for a week**, a worker labours for the Town Works for a week, then goes back to try its employer again. (Kept
  in memory only: a restart forgets it.)
- **Not yet:** gathering for a living, carrying for caravans (Phase 7), alms for beggars (Phase 5).
- Tests: `roads_tests` `aLivingForEveryone`, and the broke shop in `whoPaysWages`.

Every wolf of 16 or more who has no position, or an unpaid one, has one of these, by its circumstances:

- **Day labour at the Town Works** (the commonest). Each morning, the works takes on as many labourers as it has money
  and work for: hauling, mending, sweeping. It pays from its own account, funded by the town. Its work is the
  buildings' upkeep (Phase 5).
- **Gathering**, out of town: firewood, herbs, berries, mushrooms, reeds. The gatherer sells them to the shops that
  buy them (`buys` in crafts.json), at the price a player gets.
- **Carrying** for a caravan, as a carter or guard, paid by the caravan's owner (Phase 6).
- **Contracts** (Phase 4).
- **Begging**: a beggar receives alms (Phase 5), and townsfolk with money to spare give a penny now and then.
- **Retired**: wolves of 65 or more may stop working and are kept by their household. One with no household can turn
  to the church.
- **Lords and great houses** live on their houses' rents and estates (open question 3).

## Phase 3b: working out of town (added 2026-10-04)

**Built 2026-10-04, first part** (`Core/RatwOutwork.cpp`):

- **The ground** (`World::findWorkGrounds`, once): for each town, the wild outdoor cells within two steps, up to 10
  of them, nearest first. Each gets up to six standing spots, five strides apart, each beside forage ground:
  - woods: woodcutting or gathering;
  - shallows and reeds: fishing;
  - anything else: gathering.

  Each also gets a hunter's spot at its heart, where the cell's mix of country suits any of doc 41's game. Each spot
  takes two wolves.
- **The choice:** a wolf of 16 to 64 without a post or an apprenticeship takes a ground of its town, by a steady
  per-wolf leaning. Only when every ground is taken does it labour for the Town Works.
- **The work:** each spell (about a game hour) at its spot brings in goods.
  - Gathering and woodcutting take a picking from the same forage patch players use. A picked-over patch gives
    nothing till it regrows. Woodcutting adds 2 firewood and sometimes timber.
  - Fishing: a fish half the time.
  - Hunting: a kill 40% of the time, less on hunted-out ground. It is chosen by the cell's country (no fierce
    animals), yields what doc 41's animal yields, and adds to the cell's hunting pressure, as a player's kill does.
- **Selling:** in the last hour and a half of its day, it goes back to an open shop of its town that buys what it
  carries (crafts.json `buys` and `supplies`). It sells at a little over half the price, as a player does, while the
  shop wants more (up to 40 of each) and has the money. Nobody pays it a wage.
- **Idle posts:** a wolf of 16 to 64 whose post is only idling ("idling in the square", "sits and watches") works
  out of town instead.
- **On DEV this reaches few** (measured 2026-10-04): DEV has no grown wolves without a post, and only a handful whose
  post is idling. Nearly all hold town work (shop help, porters, messengers, washers). Out-of-town work grows as
  newcomers arrive and posts fall empty. Emptying town centres further means giving more of the generated residents
  outdoor posts, in worldgen (open question 7).
- **Not yet:**
  - mining, quarrying and farm hands;
  - the Town Works' cap on day labourers;
  - changing trade when one stops paying;
  - a world of one town (its country isn't worked yet).
- Tests: `roads_tests` `workingOutOfTown`.

The user's brief: work that takes wolves out of the town centre. Self-employed hunting and gathering, mining, farming and
other work, so that not every NPC in a city spends all day, every day, at the market.

Day labour at the square (Phase 3) is the last resort, not the default. A wolf without a post first looks for a trade of
its own out of town, and lives by selling what it brings back. Nobody pays it a wage.

**The trades**

| Trade | Where it works | What it brings back | Who buys it |
|---|---|---|---|
| Gathering | Wild ground near its town (doc 41's forage ground: woods, heath, shore, reeds) | Firewood, herbs, berries, mushrooms, reeds, resin, oak bark | General stores, herbalists, apothecaries, chandlers (crafts.json `buys`) |
| Hunting | Wild cells with game (doc 41's animals by ground) | Raw meat, hides, small pelts, bones, sinew, antler | Butchers, tanners, provisioners, inns |
| Fishing and shore work | Rivers, lakes and shore | Fish, shellfish, reeds | Fishmongers, provisioners |
| Woodcutting and charcoal | Forest cells | Firewood, timber, oak bark, resin; charcoal | General stores, the Town Works, smiths |
| Mining and quarrying | A mine or quarry (worldgen's industry; ore where there is a mine) | Stone, limestone, clay; ore | The Town Works, potters; foundries and smiths (with Phase 7's metals) |
| Farm hand | A town's fields, beside its farmers | Its share of the yield (crafts.json `producers`) | Mills, stalls, provisioners |
| Peat, salt, eels and the like | Where the land has them (worldgen's outside workers) | Peat, salt, eels | Whoever buys them |

**How a wolf takes up a trade**

- It considers what its town's country offers within a walk (the wild cells along its roads and around it, and its
  fields, mines and quarries), and how much the town's shops will pay for each trade's goods now.
- Its own leanings come into it: skill carried over from past work (skill families), and a steady per-wolf liking.
- **A place takes so many.** Each patch of forage, stretch of hunting ground, mine face or field has room for a few,
  so the work spreads out instead of everyone crowding the nearest wood.
- It keeps its trade from day to day, getting more skilled. It changes trade only when its trade stops paying (a
  winter with nothing to gather, a hunted-out wood), or when a post opens and it takes it (careers' succession).

**The day**

- In the morning it walks out of town to its spot: a real tile on the right ground, which it reaches as anyone walks.
- It works its hours there. Every spell of work brings in goods by the same rules players use, from the same finite
  sources:
  - forage patches deplete and regrow, shared with players (doc 41);
  - each kill adds to a cell's hunting pressure, so a wood a pack of NPC hunters works is poorer for a player too;
  - mines and fields yield as producers do.
- In the afternoon it walks back, sells what it brought to the shops that buy it (at the price a player gets), and
  keeps some for its larder.
- **Weather and the week:** rain and storms keep it in (as outdoor work now), and Restday and festivals are days off.

**What it does for the town**

- The town centre empties in the day. Only shopkeepers, their help, the watch, customers and the day labourers are left
  in town, and the wild, roads, woods and shores have wolves in them.
- Real goods come in from the land, so makers have materials without carting them in.
- Money moves from shops to wolves for real goods, and back from wolves to shops for food: a loop with no treasury in
  it.
- Players meet hunters in the woods and gatherers on the heath, and compete with them for the same patches and game.

**Who it applies to**

- **First**, every grown wolf without a post (Phase 3's day labourers) chooses a trade. Day labour stays for those who
  find none: in a hard winter, or in a town with no wild country near.
- **Then** the paid posts that today stand in town doing little ("sits by the fountain", "idling in the square"). These
  become out-of-town trades, so a city's 900-odd paid civilians aren't all in its streets.
- The Town Works hires only as many day labourers as it has work and money for: a few per hundred residents
  (*placeholder*).

**Cost:** the wolves walk out and back as everyone does, so the simulation tiers keep it cheap far from players. A
gatherer working offstage is a timed yield, like producers now. The 20-player gate (`world_check --players 20`) must
still pass.

## Phase 3c: industry outside the gates (built 2026-10-04: DEV revision 14, build 23)

**What was built:**

- **Every chain has a source.** The user's rule (2026-10-04): every good the shops make must have its ingredients
  produced somewhere, at a rate that supplies the whole world (bakers everywhere need flour, so mills and farms enough).
  `tools/supply_balance.py EXPORT_DIR|PROJECT.json` estimates each good's demand a day (food by nourishment for the
  price, households, the town buyers, then down through the crafts) against what producers bring in. It lists who
  makes what and flags goods with no source or too little.
- **New trades** (crafts.json `producers`):
  - orchards, vineyards, apiaries, dairy herds, swineherds;
  - logging, mines (iron and copper ore), tin streams, clay pits, peat, reed beds, eel traps, herb gathering;
  - hay from the farms, which also thresh the barn's grain in winter (`offSeason`), so mills don't run dry each
    winter;
  - pasture takes goats.
- **New crafts:**
  - iron, copper and tin bars, and bronze (the foundries and ironworks);
  - lime, bricks and roof tiles (brickworks and masons);
  - planks (sawmills and carpenters);
  - cheese (dairies), wine (brewers), beeswax candles (chandlers);
  - smoked fish at smokehouses;
  - honey cakes (bakers).

  Swords and nails finally have a source.
- **The businesses that were never recognised** now are (businesses.json `match`): foundries ("bloomery",
  "smelter"…), ironworks, sawmills, brickworks ("tileworks", "lime kilns"), saltworks, smokehouses, dairies,
  cartwrights, papermills, dyeworks, arsenals, water mills. DEV's industrial keepers ("running The Grayrock Foundry")
  make things now.
- **New buyers:**
  - the Town Works: lime, planks, nails, bricks and roof tiles;
  - the watch: cheese;
  - the church: beeswax candles and wine.
- **The sites** (`python3 -m worldgen.industry`), 31 of them:
  - **Upper Accord:** four farms, an orchard, a mine, a dairy, a piggery, a quarry with lime kilns, a fold, a logging
    camp and hives (Western Approach, South Saddle, the peaks and slopes).
  - **Ser Ferro:** four farms, a vineyard and press, an orchard, two dairies, a piggery, a water mill, clay pits and a
    brickworks, a fishery and smokehouse, and hives (the Heart and South cells).
  - **Ridgemere:** a mine, a logging camp, a fishery and smokehouse, a hill farm, a fold and a dairy.
  - **The towns:** Cinderbrook's smelter, Accord Crossing's hay farm, Westmarch's dairy.

  Each site has its yard and its building with interiors, as near the settlement as it fits, on ground that suits it.
  Every door and work spot can be walked to from the cell's road. Nothing already built or paved was touched.
- **The rebalance:**
  - Each settlement keeps a shop open for about every 15 residents (a city, every 10), at least one of each kind
    and every inn.
  - The rest close (Upper Accord 25 of 55; Ser Ferro 4 of 33). Their buildings stand empty.
  - Their keepers and help, help beyond one for every two open shops (an inn keeps its own), and, where sites still
    needed hands, up to half a city's porters, messengers and washers went out to the sites.
  - 129 wolves moved: 60 in Upper Accord, 36 in Ser Ferro, 22 in Ridgemere. Two over 64 retired.
  - Names, homes, families and looks stay; descriptions and greetings say the new work ("I work the fields at High
    Terrace these days.").
- **Checked:**
  - Atlas validation;
  - `world_check` on build 23: all 896 places load. A morning from 5 to 11 with 1,344 residents ended with 1,237 at
    their place, 102 on their way and none without a route. Money conserved, mean tick 3.2 ms;
  - `test_worldgen`.
- **Supply on build 23** (a day, the year's average): every land good beats its demand except milk (814 against 693,
  just under the 1.2 margin) and honey (bakers make honey cakes only when they have it).
- **Not yet:**
  - **Iron has almost no buyers.** Mines bring in 600 ore a day against 2 wanted; tools, horseshoes, wheels and wear
    are Phase 5's.
  - The towns' new sites have only one or two hands.
  - Nobody lives at a site yet (farmhouses and bunkhouses stand empty): everyone walks out from town.
  - The closed shops aren't yet to let.

The proposal, as planned:

The user's brief: the rebalanced townsfolk (open question 7) need real places to work outside the walls (quarries,
farms, mines and the like) so they don't wander the wilderness by the hundred. For example, Upper Accord should have a
quarry and Ser Ferro farms.

**The idea:** most out-of-town work is at **sites**. A site is a place built by worldgen just outside the gates, with
its buildings, its work spots, its posts and, at the larger ones, cottages or a bunkhouse where some of its workers
live. Its workers are producers (crafts.json `producers`): they walk a known road to a known place, or live there.
Free gathering and hunting in the wild (Phase 3b) stays for a few, about eight a town at most.

**The kinds of site:**

| Site | Buildings | Brings in | Feeds |
|---|---|---|---|
| Farmstead and fields | Farmhouse, barn, fields | Wheat, oats, vegetables, flax, hemp, hay, straw | Mills, stalls, weavers, chandlers; hay and oats for horses |
| Orchard or vineyard | Press house, rows of trees or vines | Apples, grapes | Brewers (cider), vintners (wine for the church) |
| Pasture and dairy | Fold, dairy, byre | Wool, milk, cheese, hides, livestock | Weavers, inns, tanners, butchers |
| Apiary | Hives, a shed | Honey, beeswax | Beeswax candles for the church, mead |
| Quarry and lime kiln | Pit, cart ramp, kiln, office | Stone, limestone, lime | The Town Works, masons |
| Clay pit and brickworks | Pit, drying sheds, kiln | Clay, bricks, roof tiles | Potters, the Town Works |
| Mine and smelter | Adit, spoil heap, smelter | Iron ore, copper and tin ore, then iron and bronze bars | Smiths, finally: swords and nails have a source |
| Logging camp and sawmill | Camp, log yard, saw pit or mill | Timber, planks, bark, firewood | Carpenters, coopers, tanners, the Town Works |
| Charcoal kilns | Kilns, burners' huts | Charcoal | Smiths, smelters, glassworks |
| Fishery and salt pans | Boats, drying racks, pans | Fish, shellfish, salt | Fishmongers, smokehouses, cooks |
| Peat cutting, reed beds | Stacks, sheds | Peat, reeds | Fuel, thatch, chandlers (rushlights) |

**Where:**

| Place | Its country | Sites (new unless marked) |
|---|---|---|
| **Upper Accord** (capital, a crater between three peaks) | Rocky peaks, saddles, the western approach's lowlands and forested slopes | A **quarry and lime kiln** on the southern peak's flank; **highland pasture** (sheep and goats) on the saddles; **terraced farms** and an **orchard** on the western approach; a **logging camp** on the forested slopes; an **apiary** |
| **Ser Ferro** (grain country on the great river) | Golden fields, pasture, orchard hills, the river | Three or four **farmsteads** with grain, flax and hay; **vineyards and orchards** on the orchard hills (the Cathedral's wine); a **dairy** and pasture; a **water mill** on the river; **clay pits and a brickworks** by the river (its red roofs); **river fishers**; an **apiary** (the Cathedral's beeswax) |
| **Ridgemere** (rain coast, cedar forest, harbour) | Cedar rainforest, highland, wet meadows, the sea | Its five estates already are its industry: Grayrock's quarry and ironworks, Fell's timber, Ashcombe's charcoal and glass, Brinewater's ropewalk and yard, Vesk's fish and salt. **Add:** an **iron mine** in the highland for Grayrock's ironworks, which has no ore; **logging camps** deep in the cedar for Fell; **fishing boats and oyster beds** for Vesk; **sheep on the wet meadows** and **hill farms** (oats, potatoes). Its freed townsfolk become these estates' workers, lodged in the estates' cottages and bunkhouses |
| Cinderbrook | Hills and woods | Its miners and charcoal burners, *plus a smelter* (bars from its ore) |
| Accord Crossing | Crossroads, meadows | Its fields, *plus hay meadows* for the carters' horses |
| Amberford | Steppe grain | Its farms and mill (as now) |
| Westmarch | Chalk downs | Its farms, sheep and drovers (as now), *plus a dairy* |
| Saltreach | Grey coast | Its fishers and salt pans (as now) |
| Lakeside | The Mirrormere | Its fishers, reed cutters and fields (as now) |
| Fenhollow | Fens | Its peat, eels and herbs (as now) |
| Hollowmere Village | Valley | Its farms and woodcutter (as now) |

**Workers:** roughly the wolves freed by open question 7, about 60 to 70 from each city:

- **Upper Accord:** about 15 at the quarry and kiln, 25 at the farms and orchard, 10 at pasture, 10 logging.
- **Ser Ferro:** about 35 on the farmsteads and vineyards, 10 at the dairy, 8 at the brickworks, 6 at the mill and on
  the river, a few beekeepers.
- **Ridgemere:** about 15 to the mine, 15 logging, 10 fishing, 10 at the hill farms and sheep, the rest to the estates.

A third or so of each site's workers live at it (cottages, bunkhouses) and the rest walk out from town. *All
placeholders.*

**What the game needs for it:**

- new producers in crafts.json (quarry, lime kiln, mine, smelter, sawmill, brickworks, dairy, vineyard, apiary, hay),
  and the goods that are missing from the catalog;
- the sites placed by worldgen into DEV's cells around each settlement, additively (no existing building or resident
  moved), with Atlas able to adjust them afterwards;
- the residents' posts changed to the sites' posts (open question 7's rebalance).

## Phase 4: NPCs take contracts

**Built 2026-10-04, first part: contracts for goods** (`World::residentsFillContracts`, `tendContractCarriers` in
`Core/RatwProcure.cpp`; the contract's `source` and `carried`, saved with the roads):

- **Who takes it.** A contract for goods open a day with no player taker goes to a wolf of 16 to 64, with no post or
  apprenticeship, from the town that wants the goods, by lot.
- **Where the goods come from.** A shop with the goods to spare (more than the few it keeps), its own town's first,
  then whichever has the most. Only if the reward covers the goods at their price.
- **The errand**, in person: to the shop, where the goods are handed over on the contract's account; then to the market
  of the town that wants them. On arrival the goods go to the buyer. The shop is paid its price from the reward
  (`sold on a contract`), and the carrier gets the rest (`carrier's pay`). If only part could be fetched, the rest of
  the contract opens again.
- **Not yet:** supply runs, work contracts (projects, repairs) from towns and churches, and NPCs taking bounties and
  escorts beyond what doc 26 already has.
- Tests: `roads_tests` `residentsFillContractsForGoods` (stone carried from a west shop to east's Town Works).

- **Contracts for goods (procure).** A maker or supplier with the goods, in the town or the next, takes a contract
  whose reward beats what it would get selling them, carries them over, and hands them in. A wolf out of work may
  take one, buy the goods where they are sold, and deliver them, as a player would.
- **Supply runs:** the same, for a town store short of food.
- **Work contracts**, new: the Town Works and the church post paid jobs for labour ("mend the chapel roof: 4 days'
  work, 30p"). Workers out of work take them first, then players.
- **Players and NPCs compete fairly.** One taker per contract, as now. A contract stays open to players for its first
  day before NPCs may take it.

## Phase 5: towns and churches spending what they take in

**Built 2026-10-04** (with the rule against hoarding above; `Society::tradeUpkeep` and `townBuyers` in
`Core/RatwDemand.cpp`):

- **Tools a trade wears out** (crafts.json `tools`): every so often a worker buys its tool from a shop of its town,
  out of its own purse, and wears it out at work:
  - a miner a mining pick every 15 days, a quarryman every 20;
  - a farmer a sickle every 20 days; an orchard or vineyard hand every 40;
  - a logger a saw every 20 days, a woodcutter every 25;
  - a fisher a net every 30 days;
  - a dairy herd a barrel every 20 days; salt pans and charcoal burners a sack every 10.

  This is iron's steady sink.
- **Businesses' upkeep** (crafts.json `upkeep`), bought like materials and paid from the till:
  - a stables' horses eat 3 hay and an oats a day;
  - a cartwright's yard uses wheels and axles.
- **New buyers** (`institutions`), which can now be per worker of a trade (`"per": "producer:mine"`) or only in big
  communities (`minResidents`):
  - **the docks**, per fisher: rope, pitch, canvas, barrels, planks, nails, salt;
  - **the mines**, per miner: rope, lamp oil, timber props, bread;
  - **the quarries**, per quarryman: rope, timber, bread;
  - **the hall of records**, in communities of 150 or more: paper, ink, sealing wax, ledgers, candles.
- **The crafts for them:**
  - smiths: mining picks, sickles, saws, iron hoops;
  - coopers: barrels;
  - ropewalks and chandlers: rope, nets, pitch, lamp oil, sealing wax;
  - weavers: canvas and sacks;
  - cartwrights: wheels and axles;
  - papermills and scriptoria: paper, ink and ledgers.
- **The church's bread is alms.** What its basket uses up of anything to eat goes first to the town's hungry poor
  (under 12p, nothing to eat), one each, and the rest is eaten at its table.
- **Beggars** are given a penny each by up to three of their town's better-off (over 100p) each day.
- Tests: `roads_tests` `tradesWearAndCharity` (a miner's pick from the smith, horses fed from the farmer's hay, a
  beggar's pennies, the church's alms).
- **Not yet:** buildings decaying (the Town Works' basket stands in for repairs), care for the sick poor, and
  caravans' buyers (Phase 7).

**More buyers** (doc 35 Part 7's table), each funded by whoever runs it:

| Buyer | Where | Uses up | Paid from |
|---|---|---|---|
| Docks and shipyards | Harbour towns | Timber, pitch, rope, canvas, nails, barrels | Harbour dues (the treasury) |
| Mines and quarries | Where there is a mine or quarry | Rope, props, lamp oil, picks, food | Their owners |
| Administration | Cities and capitals | Paper, ink, sealing wax, ledgers | The treasury |
| Stables | Towns with stables | Hay and oats (a horse eats a bale or two measures a day) | The stables' keeper |
| The church, more fully | Every town | Beeswax candles, incense, wine, bread for alms | Tithes |

- The goods these buyers want must be made by someone, so each comes with its craft or producer: hay from the farms,
  rope at the ropewalk, barrels at the cooper, pitch from resin and charcoal, canvas from hemp, paper and ink at the
  scribe, lamp oil from tallow and fish.
- **Buildings decay.** Each building has a condition that weather wears down. The Town Works buys stone, timber and
  lime, and hires labour (Phase 3), to mend it. A town that can't pay shows it: cracked plaster, missing tiles.

**Charity** (the church)

- **Alms.** A wolf with less than a day's food money (6p) and nothing in its larder may go to the church for bread,
  free, once a day. The bread is the church's own, bought with tithes. The town's poorest eat without spending, and save.
- **Care.** A sick or injured wolf who can't pay a healer may be tended at the church, with its bandages.
- **The poor box.** What the church has above two months' basket goes back out as alms and work contracts, never
  hoarded.

## Phase 5b: great houses own businesses (added 2026-10-04)

**Decided (2026-10-04):**

- A house-owned business has a till of its own.
- A house props up a till that is struggling, from its own money. If it keeps struggling, the house sells the business
  to another house.
- The house takes all the profit. The manager's wage will later follow how well the shop does.

**Built 2026-10-04** (`Core/RatwHouses.cpp`; `HouseState` saved with the society as `houses`):

- **The houses** come from their heads' posts: a civilian "ruling House Fell", "keeping Vesk Manor", "holding court"
  (as `Society::houseHead`). Each has an account `house:<name>`, opened when the houses are founded, even before it
  owns anything.
- **Founding**, once, on the first day:
  - in each town with a great house, every workshop and yard (businesses.json `kind` "works" or "yard") goes to one of
    its houses, and about half its shops, by lot;
  - each gets a till (`till:<position>`). The keeper's goods go into it (it keeps one thing to eat), and so does its
    money, but for a week of its own wage (`the shop's till`, kept out of its profit).
- **The till:** `Society::tillOf(keeper)` is the till for a house-owned business and the keeper itself for anyone else.
  Every shop path uses it: selling to players and residents, crafting, buying materials, restocking, the town's buyers,
  wolves selling what they brought in, and repairs. The trade window shows the till's stock. A manager, its shop's goods
  no longer its own, buys its food like anyone else.
- **Each midnight** (`tendHouses`):
  - The manager's wage (8p) is paid from the till.
  - Everything above the float goes to the house (`house takings`). The float is a week of the manager's wage and its
    help's (6p a day for each post working there), plus 60p for materials.
  - Below half the float, the house props the till up to the float (`propped up by the house`) if it can, and the day
    counts as a struggling one.
  - After 10 struggling days in 28, the business is sold. The buyer is the house that can afford two floats, one of the
    same town first, then the richest, and it pays the old owner (`sale of a business`).
- **Houses pay the town tax and tithe** on their profit at the reckoning, like residents.
- **Not yet:**
  - the house spending its surplus (the rule against hoarding);
  - the house paying its staff from its own account (they are paid as Phase 2 has it);
  - the Dungeon Master transferring a business (`Society::sellBusiness` is there for it);
  - the manager's wage following the shop's takings.
- Tests: `roads_tests` `greatHouses`.

The user's brief: houses own industries and profit from the shops they own. A house owns several businesses. All
their profit goes to it, and each shop's manager is paid a wage.

- **The houses** are the great houses already in the world: House Fell, House Brinewater, Vesk Manor, Ashcombe House,
  Grayrock Hall, Ser Ferro's court houses and the rest. Each has its own account (`house:<id>`), and its head is a
  resident ("ruling House Fell").
- **What a house owns:**
  - first, the industry in its lands: ironworks, logging, saltworks, shipyards, mines;
  - then some of the shops in its town or city, a share of them by lot;
  - nothing in a town with no house: its shops stay their keepers' own.

  Ownership is data (`Data/Items/houses.json`, written once from DEV's buildings), and a Dungeon Master can transfer a
  business.
- **The manager.** The keeper of a house-owned business becomes its manager. It is paid a manager's wage (8p a day,
  *placeholder*) from the till before anything else.
- **The takings.** Each midnight, a house-owned business sends its house everything in the till above its **float**
  (`house takings`). The float is what the business needs for a week: materials for its batches, its help's wages and
  the manager's.
- **The till.** Today a shopkeeper's purse is its shop's till. For a house-owned business, the manager's own money
  (its wage and what it buys with it) is kept apart from the till (open question 5).
- **The house** pays its staff (Phase 2), keeps its reserve, and spends its surplus by the rule against hoarding. It
  pays the town tax and the tithe on its profit like anyone else.

## Phase 6: Restday at church

**Built 2026-10-04:**

- **The church** (`World::chapel`): each community's is where its clergy work, the first such post indoors (Society::
  clergy: a chapel keeper, priest, acolyte...). Its pulpit is that post's work spot. Its seats are up to sixty open
  floor tiles a few strides from it. On DEV every town has one: its chapel, Ser Ferro's cathedral, Upper Accord's
  Chapel of the First Oath.
- **The service:** on Restday, from 9 to 11 (`ServiceStart`, `ServiceEnd`), the clergy go to the pulpit
  ("preaching") and a third of the town sits in the church ("at church"). The third is a steady per-wolf choice, a
  different third each week (`goesToChurch`). The watch keeps its hours.
- **Shops open after the service**, 11 to 3, where they used to open 8 to 12.
- **The plate:** in the service's last half hour, each churchgoer puts 2p on it with over 60p, 1p with over 20p
  (`the collection`, to the town's church).
- **Churchgoers grow familiar** with one another, as at the market and at festivals.
- **The sermon** (`Game::sermons`): a preacher at the pulpit speaks this week's sermon, a line a minute, while a player
  is in the church. It is said aloud as anyone speaks, heard to the back of the church, and logged as a `sermon`
  event.
- **Sermons** (`Data/Voice/sermons.json`, `Core/RatwSermons.cpp`): eleven, of eight or nine lines each, with `{town}`
  for the town's name:
  - for any church: *The Poor Box* (charity), *Honest Weights* (trade), *The Shared Hunt* (the pack), *For Those Who
    Have Gone* (the dead), *The Road and Its Dangers*, *The Turning Year*, *The Tenth* (the tithe), *On Forgiving*;
  - each faith its own: *The Saint at the Forge* (the Iron Saint, Ser Ferro), *What the Tide Takes* (Ridgemere), *The
    First Oath* (Upper Accord).

  Each church takes them in turn from its own place in the list, so none repeats within four weeks.
- Tests: `schedules_tests` `restdayService` (Greyfen with a chapel: the preacher, some of the town and not all, the
  plate, over by noon); `game_tests` `aSermonOnRestday` (a player in the chapel hears the sermon begin).
- **Not yet:** the devout going more often than others, and sermons that answer the town's news (a death, a raid).

- **The service.** On Restday morning (9 to 11), a share of each town (a third, varying by wolf and week; the devout
  more often) goes to its church or chapel and fills the pews. Shops open after the service rather than at 8.
- **The preacher** is the church's holder of a priest's or chapel keeper's position. It stands at the front and preaches
  a sermon: lines spoken a minute apart, heard by everyone in the chapel, players too.
- **Sermons** are scripts in `Data/Voice/sermons/`, like the scene files: a title, a theme, and 8 to 12 lines. The first
  set has at least eight sermons, from the faith of the Iron Saint and the Accord's own: charity and the poor box,
  honest weights at market, the hunt shared, the dead remembered, the road and its dangers, the season's turn, the
  tithe, and forgiveness. A preacher doesn't repeat a sermon within a month.
- **After the service**, the plate goes round: a voluntary offering (a penny or two from those who can spare it) on
  top of the tithe. Churchgoers grow familiar with one another, as at the market.

## Phase 7: caravans and regional goods

**Built 2026-10-04** (`Core/RatwTrade.cpp`):

- **A town's market** (`World::marketOf`): what its producers hold and its shops have beyond what they keep (spare,
  and who holds it), and what its makers lack for eight batches and its suppliers for half a store (wanted, and by
  whom).
- **Trade caravans**, each day after the capital's own (`tradeCaravans`):
  - For each good a town wants more of than it has itself, the town with the most to spare (after its own want) is
    found. Up to 30 of a good and 80 a load go on one wagon for each pair of towns, at most 12 wagons a day.
  - **The trader** is the richest great house of the sending town, or else its treasury. It pays for the load at the
    catalog price; the money goes with the wagon, which buys from the holders at home (`bought for the road`), and
    the change rides along.
  - The trader also buys the carters' bread and the horses' hay, eaten on the road.
  - **On arrival**, the goods are sold to the shops that want them at 1.4 times the price, as far as each can pay
    (`carted in by caravan`).
  - **Home again**, the takings go to the trader (`trade takings`). Anything unsold goes back to the home town's
    stores, which pay what they can.
  - **Bandits** who rob a caravan now take every good it carries and its money, into their camp's hoard (`robbed by
    bandits`).
- **No more "carted in at 1.5×."** In a world of towns, a maker short of something nobody in its own town has waits
  for a caravan (`Society::setTradeByCaravan`). A world of one town, or a society on its own, still carts it in.
- **Regional goods** (`Data/Items/regions.json`, written by `tools/supply_balance.py EXPORT --regions`): each place's
  specialties (goods it brings in a quarter or more of the world's supply of), what it brings in, and what it lacks.
  On DEV build 23:
  - Cinderbrook: iron and copper ore, and charcoal;
  - Fenhollow: peat, herbs and wormwood;
  - Lakeside: fish and reeds;
  - Ridgemere: timber, stone, lime, charcoal and salt;
  - Saltreach: salt;
  - Ser Ferro: grapes, honey, beeswax, milk and pigs;
  - Upper Accord: stone, lime, wool and apples;
  - Westmarch: wool.

  Every place lacks something only caravans bring.
- Tests: `roads_tests` `tradeCaravansCarryGoodsAndMoney` (east's spare flour bought, carried and sold to west's bakery,
  the takings home).
- **Not yet:** prices following scarcity for every good (only meals and herbs do), carters hired from those out of work,
  wheels and axles worn out by the wagons, and caravans by water.

- **What each region has** (`Data/Items/regions.json`): the goods it makes or brings in, and the goods it lacks. For
  example:
  - Saltreach: salt and smoked fish;
  - Cinderbrook: iron, charcoal and glass;
  - Westmarch: wool and cloth;
  - Lakeside: fish and pottery;
  - Amberford: flour, beer and barrels;
  - Ridgemere: iron goods and timber.

  This comes mostly from the producers and workshops already placed; the file makes it explicit and adds the few that
  are only found in one place (ore, salt, clay).
- **Merchant caravans.** A trading house (a merchant in a market town, or a carter's yard) sends a wagon to another
  town:
  - It buys what is cheap and plentiful at home, with its own money.
  - It carries the goods and sells them where they are scarce.
  - It brings home the coins it was paid, and the goods it bought there.

  The coins travel in the caravan's own account, so a robbed caravan loses money as well as goods, and bandits'
  takings are real.
- **The end of "carted in at 1.5×".** A maker that can't get something in town orders it from a trading house, and it
  arrives with the next caravan. The price difference pays the trader and the carters, not a seller who never parted
  with the goods.
- **Prices follow scarcity** for every good, not only meals and herbs (`tendPrices`), so a trader can see where a good
  is worth carrying.
- **Caravan buyers** (doc 35 Part 7): wheels, axles, harness, sacks and rations, and hay and oats for the horses, paid
  by the trading house.

## Phase 8: watching the money

**Built 2026-10-04:**

- **The Money tab** in the Dungeon Master (`Editor/src/dm/MoneyTab.tsx`; `GET /api/money`, `DungeonMaster.money`), from
  the last save, refreshed every minute:
  - each town's residents, treasury, church and its buyers' funds, and the capital's treasury;
  - residents' purses: how many and their total, the middle purse, the poorest and richest tenth's average, how many
    are short of a day's food money (under 6p), and the same by kind of work;
  - the great houses and their businesses' tills;
  - money on the road (caravans), held for contracts, in bandits' hoards, and the players';
  - the month, and the latest reckonings, surplus spending, trade caravans and sermons.
- **`world_check`** prints where the money is at the start and end of a run, how many residents are short of a day's
  food money, and, with `--reckon`, the month's reckoning and where the money went after it.
- Tests: `test_dungeon_master` `MoneyTests`.
- **Not yet:** wages by payer and alms given as totals, and a month-long run (`world_check` takes about 15 minutes a
  game day; `/reckon` stands in for the month's end).

- `world_check` already prints where the money is (above). It will add the reckoning's totals, wages by payer, alms
  given, and how many wolves are short of a day's food money.
- A **Money** view in NPC Management: each town's treasury and church, tax and tithes collected, wages paid, the
  poorest and richest tenth, and money on the road.
- **The test:** after four weeks, no town's treasury runs dry, fewer than one wolf in twenty is short of a day's food
  money, and money stays conserved. It can't be run a month at a time (`world_check` takes about 15 minutes a game
  day), so a short probe runs a reckoning straight away (`/reckon` in the Dev Console).

## Still open, and what was done (2026-10-05)

The user reassessed the list on 2026-10-04 and asked for every item to be solved overnight, with these decisions:

- players pay no tax;
- smiths, armourers and the towns' repairs use iron;
- great houses start with large purses;
- houses don't collect rent from themselves, but may from other houses.

1. **Too much iron ore.** *Done:*
   - Mines yield one ore a spell. Copper and tin come only from the Bell Pit.
   - Iron now has uses:
     - smiths make steel, fittings, chain, rivets, buckles, cookpots, locks and iron swords;
     - armourers make kettle helms, gorgets and greaves;
     - households buy nails every 20 days and a cookpot every 60;
     - the Town Works uses iron, fittings, chain and locks;
     - the watch wears out rivets, swords and helms.
   - The foundry casts bronze handbells for the church.
   - On build 24: 165 ore a day brought in against 68 wanted.
2. **The towns' sites had a hand or two.** *Done:* 11 newcomers came to work them (`worldgen.industry --settle`,
   DEV revision 15), and they live at the sites.
3. **Nobody lived at a site.** *Done:* 17 bunkhouses were built beside the quarries, mines, logging camps, fisheries,
   dairies, folds, piggeries, clay pits and smelter. 37 site workers with no family at home moved into them or the
   farmhouses. Four found no room nearby (Larkrise and Peakside in Upper Accord, Fornace Bassa in Ser Ferro, Rainwash
   in Ridgemere).
4. **The closed shops stood empty.** *Done:* 10 are now to let (doc 32's lettings, as a hall for a Chapter, 15p a week)
   from the town's great house's head, or else the treasury. Found on the way: `world_store` never saved a letting
   (its JSON column wasn't wrapped as JSON), now fixed.
5. **Milk and honey were tight.** *Done:* a dairy herd yields 5 milk a spell, an apiary 3 honey (build 24: milk 968
   against 693 a day).
6. **Players pay no tax.** *Decided:* players pay neither tax nor tithe.
7. and 8. **Unpaid workers and the reserves were remembered in memory only.** *Done:* saved with the society
   (`EconomyMemory`: `unpaidSince`, `outgoing`, and the towns' repair `condition`).
9. **DEV's running server** has an older build until it is restarted (not done: nobody asked for a restart).
10. **Buildings didn't decay; the sick poor weren't cared for.** *Done:*
    - **Repair:** each town's buildings have a condition (0 to 100) that wears a point a day (two in winter). The Town
      Works mends it with what it actually uses of its basket, and a run-down town's basket grows to catch up. Falling
      under 50 is logged (`town in disrepair`), and so is being mended. It shows in the Money tab.
    - **Care:** a poor player (under 20p) with healing wounds may "ask for the church's care" of a priest or chapel
      keeper. The church spends one of its bandages and takes a quarter off each wound's rest, never all of it, once a
      day. The church's basket now buys bandages.
11. **Copper had no use.** *Done:* see 1. Copper is now scarce (only the Bell Pit) and goes into bronze for bells and
    swords.
12. **Prices didn't follow scarcity.** *Done:* every six game hours each town prices every good by its market: up to
    half again as dear where it is short, a fifth cheaper where plentiful. Trade caravans sell at the destination's
    factor on top of their markup.
13. **Makers waiting for caravans:** watched in the two-day run (below).
14. **Shops may not afford their help.** *Done:* a shop (or a house) pays its help 2p a spell from a till over 150p, 1p
    from a leaner one, so a quiet shop keeps its help and a busy one pays them well. See the two-day run.
15. **Bandits raided often.** *Done:* a camp wins less easily (odds `bold / (bold + 8 × guards + 20)`). A trader's
    caravan hires two more guards from its town (6p). A camp's hoard is no longer lost: the town recovers it when its
    watch clears the camp, and the treasury when the camp starves out.
16. **Great houses started poor.** *Done:* each is founded with a fortune of 3,000p, made once at the founding (counted
    as money made, like the world's first treasury). **Ground rent:** each month, every business in a town with great
    houses pays 20p to one of the houses that doesn't own it, never to its own.
17. **Contracts for goods weren't taken.** *Done:* carriers can now be those out of work, day labourers, out-of-town
    workers, and the town's porters, messengers and haulers. Producers holding the goods (a quarry's stone, a farm's
    grain) count as sources as well as shops.
18. **Found in the two-day run (2026-10-05): the towns paid people they shouldn't.** Of the towns' wages, the largest
    single share went to homemakers ("keeps the house"), then household servants, industry hands whose employer
    wasn't found, and fortress garrisons from their fortress's tiny purse. With tax coming in at a seventh of what the
    towns paid out, the treasuries would have been empty within days. *Done (`payerOf`):*
    - keeping one's own house, an apprenticeship, hawking, scavenging and sitting by the well earn no wage (the
      household keeps them, or they live by what they sell);
    - a household's servant is paid by the richest of the household whose home it works in;
    - a hand at a trade's works (the forge, the saws, smoking fish, the ropewalk, the glassworks...) is paid by a keeper
      of that trade in its town, or else by the town's richest great house;
    - nursing the sick is the church's;
    - a fortress's garrison is paid by the capital's treasury.
    - palazzo staff are paid by the household whose house they work in (its rooms on any floor); Mass, confession, the
      crypt and healing are the church's; counting a catch, a foreman's and an overseer's work is the trade's or the
      town's great house's.
19. **The towns' budget** (three days on build 24): wages of about 1,100p a day (watch 440, civic posts 680) against
    about 800p of tax and trade takings. *Done:*
    - A treasury keeps 14 days of its payroll before it funds its buyers (`PayrollDays`). Its buyers then get a
      month's share a day of what is above that.
    - A treasury with under 20p a head of its town (`LeanTreasury`) pays its posts 1p a spell instead of 2, as a lean
      shop does.

    So a poor town shows it (its buildings wear, its watch eats plainly, its posts are poorly paid) rather than going
    broke.

## Open questions

1. **Players:** do they pay the tax and tithe on what they earn in town (contracts, sales)? *Recommendation:* yes, the
   tax; the tithe only for a player who joins a faith.
2. **Wages:** should they rise with skill and with the employer's takings? *Recommendation:* later, in the balance pass.
3. **Lords and great houses:** rents from the houses and shops on their land? *Recommendation:* a rent from each
   business in a great house's quarter, paid monthly like the tax.
4. **Banks and loans:** doc 15 left them out. *Recommendation:* still out.
5. ~~A house-owned shop's till~~ **Decided:** a till of its own (Phase 5b).
6. ~~How much of a shop's profit a house takes~~ **Decided:** all of it; the manager's wage will follow the shop's
   takings later.
7. **More of the towns' people working outside.** The user chose the recommendation (2026-10-04), then asked whether
   the towns simply have too many jobs. They do. DEV build 22 has one shop for every 5 or 6 residents in every town;
   the three cities have no land workers at all:

   | Town | Residents | Shops | Shop help | Town labour | Land work | Children | Elders |
   |---|---|---|---|---|---|---|---|
   | Ridgemere | 296 | 46 | 64 | 55 | 18 | 35 | 22 |
   | Upper Accord | 295 | 65 | 60 | 39 | 0 | 51 | 9 |
   | Ser Ferro | 289 | 50 | 64 | 58 | 0 | 23 | 19 |
   | Lakeside | 60 | 9 | 8 | 0 | 10 | 14 | 8 |
   | Westmarch | 58 | 10 | 7 | 0 | 9 | 14 | 9 |
   | All 14 | 1,339 | 240 | 239 | 133 | 68 | 212 | 91 |

   Shopkeepers and their help are 46% of all working wolves, and only 7% work the land. A shop with five or six
   customers can't pay its help (Phase 2), and the makers are short of materials because so few bring any in.
   *Revised recommendation:* rebalance rather than add. Keep every building, but keep a shop open for about every 15
   residents in a town and every 10 in a city. The rest stand empty, to let (doc 32's leases) for players or a newcomer
   later. Shop help goes to workshops, inns and the busiest shops, about one for every two shops. The wolves this
   frees become farm hands, herders, woodcutters, quarrymen, fishers, hunters and gatherers, by the country around each
   town. The cities get fields and woods worked around them. Done additively: everyone keeps their name, home and
   family; only their work changes.
