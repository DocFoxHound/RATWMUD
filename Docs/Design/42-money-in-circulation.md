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

    *Measured* over three days on build 24:
    - Wages out, about 520p a day: most towns pay half while their treasuries are lean.
    - Buyers about 150p a day.
    - Trade takings in, about 450p a day, and the month's tax about 340p a day.
    - Treasuries 18,400p after the purse division, 16,300p after three days (the churches' foundations were most of
      it). They are roughly steady.
20. **The great houses' fortunes drained into their shops' tills** (13,000p in three days, propping tills up to a
    week's float). *Done:* a till's float is three days of wages, not seven. Over two days on build 24 the houses held
    9,477p (against 4,779p before). They propped up 10,314p, mostly the first day's filling of their new tills, and took
    in 2,609p. Money stayed conserved, with a mean tick of 2.4 ms.

## Starting money (the user, 2026-10-05)

"Everyone needs to have starting money." Every resident and every body that keeps a purse starts with a reasonable one
(`Core/RatwFounding.cpp`, `Society::foundPurses`). It is made once, when the world is founded, or the first time an older
save runs with this (`EconomyMemory::purses`). Like the world's first treasury and the houses' fortunes, it counts as
money made, and it is kept out of the month's profit (`starting money`, unearned), so nobody is taxed on it. The amounts
are placeholders (`RatwSociety.h`):

- **Residents**, topped up to:
  - a child: 5p, and half a penny a year of its age;
  - a beggar or rag-picker: 10p;
  - a grown wolf: 30p (some five days' food);
  - one retired: 60p;
  - a shopkeeper whose shop is its own: 80p, or its shop's float if more.
- **A town's treasury** (the capital's too): 40p a resident who pays it. That is over `LeanTreasury`, so no town starts
  lean.
- **A church**: four weeks of its basket, or 10p a resident if more. Made at its founding, no longer taken from the
  treasury.
- **A town's buyers** (works, watch, docks, mines, quarries, hall): their working funds, at their founding.
- **A great house's businesses**: each till starts with its float, so the house's 3,000p fortune isn't drained filling
  them.
- **Newcomers** arrive with 30p. **A child born** gets 5p from its better-off parent (given, never made).

On DEV build 24 (two days, `econ_watch`):

- Residents short of a day's food money: 82 → 6 to 10.
- The poorest tenth's average: 5p → 13p.
- Churches: 180p to 2,770p, against 15p to 250p before.
- Houses kept 2,900p to 4,500p, against House Brinewater's 32p on the second day before.
- About 98,000p was made in all, and money stayed conserved.
- **Still open:** the treasuries' daily deficit (wages against takings), which starting money only delays.

## The month's test, and what it changed (2026-10-05)

The user asked for a month left to the residents alone, watched for stuck materials, wages, anything going broke and
anything hoarding. `Tests/econ_watch.cpp` runs it (see doc 31, "Fast-forward").

**A false start.** The first run looked like a collapse: by day 13, 524 residents were starving, porters were stranded
in the wilds and 198 caravans were stuck on the road. Most of that was the tool's own fault: it threw away the cell
files after loading. Nothing could load a cell nobody had loaded yet, so anyone entering one stopped there for good.
With that fixed, the same week showed what was real:

- After the starting larders ran out (day 5), residents spent far more than they earned. Wages were about 2,900p a day,
  at most 6p a worker: three paid spells of a nine-hour day. Food was about 5,000 to 10,000p a day, two meals a wolf at
  about 4p.
- The difference piled up with independent shopkeepers, whom no rule sent back out. Residents short of a day's food
  money went from 15 to 93 between days 5 and 7, and the Gini from 0.50 to 0.60.
- A keeper's purse was its shop's till, so buying materials could spend its last penny. The Cinderbrook smelter's
  keeper went broke and hungry, and iron bars stopped.
- Goods piled up with producers while makers in other towns went short. Trade was one-off and daily, and stopped
  whenever a wagon was on the road.

What was built (the user's decisions are marked *user*):

- **Roads** (*user*: travel between towns keeps to the roads unless they are blocked, where bandits wait):
  - `World::indexRoads` finds, from every cell's tiles at load, the seams that cross on a road (dirt road, street,
    flagstones) on both sides.
  - Routes (`firstSteps`) count a step along a road as 1 and one overland as 3.
  - Travellers onstage and off cross into the next cell on the road.
  - Bandit camps follow, since they are placed along the routes between towns.
  - From Ser Ferro to the Upper Accord quarry: 20 cells, all on roads, against 17 overland through the marshes.
- **Porters' provisions** (*user*):
  - A carrier sent to another town for a contract's goods gets food for the way: a meal for every ten places there and
    back, bought at its town's shops by whoever posted the contract, or out of the reward if the poster has too little.
  - What can't be bought, it gets the money for.
  - A hungry traveller eats (or buys or fetches food) before going on with its errand.
- **Standing orders** (*user*; `StandingOrder`, saved with the roads):
  - A shop short of a good that its town can't spare signs a standing order with the town that has the most of it:
    about what it is short, a week, at the catalog price times the road's markup times the seller's scarcity.
  - Each road with orders sends its trader's caravan once a week (and at once when an order is new). On arrival it sells
    each order's share at the agreed price, as far as the buyer can pay; the rest goes to whoever wants it.
  - Every four weeks, or sooner if deliveries fall well short, a porter of the buyer's town walks (with provisions) to the
    selling town's market and renegotiates. A quarter less if the shop holds more than two weeks' worth, a quarter more if
    it went short or ran low, at the price there now. An order down to under 2 a week ends.
- **A keeper's food money:** a keeper whose shop is its own never spends its last `KeeperReserve` (20p) on materials,
  restocking or wages.
- **Shopkeepers don't hoard:** above its food money and two floats, a keeper spends a tenth a day. Four tenths go to its
  help as a share of the takings; a third of the rest buys food for its larder; the rest buys goods for its home.
- **Market dues** (weekly, where money gathers): every Restday each shop's till pays its town a twentieth of what it holds
  above its float.
- **Seasonal goods:** makers lay in six times their usual store of a good that comes only in some seasons (grapes,
  apples). Grapes were 0 in the test because it began in spring: the vineyards (Ser Ferro's Poggio Chiaro) yield in
  summer and autumn.
- **Rations:** the watch's bread, fish and cheese, and the mines' and quarries' bread, are no longer used up as a basket.
  They are rations that a hungry guard, miner or quarryman takes before spending its own pennies.
- **Paid for the hours worked:** wages are paid for up to `PaidSpells` (8) spells a day instead of 3, at 2p a spell (1p
  from a lean shop or treasury). A town's treasury keeps a week's payroll (`PayrollDays` 7, at the new rate) before
  funding its buyers.
- **Weekly reckoning** (*user*, in place of the month's): every `ReckonDays` (7) each resident pays a tenth of the
  week's profit in tax and a tenth in tithe, and towns send the capital a tenth of their tax. Ground rent to the great
  houses stays monthly (the reckoning that opens a month). Treasuries no longer wait four weeks for their income.
- **Food for the household** (*user*): a grown wolf stocking its larder buys food for everyone at home, so the children
  eat from the larder, not their own pennies. How many days it buys is the household's own steady habit: 5 to 8
  (`stockingDays`), as far as the purse goes. A new larder starts with 2 to 5 days' (`startingLarderDays`), so the
  world's households don't all shop on the same day. A keeper whose shelves hold nothing to eat buys a meal
  elsewhere, as anyone does.
- **Children's stipends** (*user*, `Society::childrenAndStipends`, daily):
  - Each child living with grown family is given a little from the household's purse (its grown members' together),
    paid by the richest of them.
  - Never a given (*user*): only what the purse can safely spare after two weeks' food for everyone at home (`FoodADay`
    5p each), the tax and tithe owed on the week's profit so far, and its rent (households pay none yet); and none at all
    while the household has lately been poor.
  - The more to spare, the more: a penny a day for every two weeks' worth, up to `MostStipend` (3p) and a penny more for
    every hundred to spare for each child (*user*: a rich family's children have plenty to spend frivolously).
  - Booked as unearned, so it neither lowers the giver's tax nor raises the child's.
  - Children spend freely: up to half of what they have each day, at their town's shops, on something cheap (3p or
    less). A treat is eaten when hungry; a trinket is kept.
- **Wants** (*user*, `Society::wants`, daily): a grown wolf with money to spare after a week's food and the tax and
  tithe it owes spends a tenth of what is above that on something it simply wants, by its own taste and the day's
  fancy: a treat or dish it fancies, a drink, jewellery, finery (a hat, a scarf, a shawl, paw wraps), soap and scent, a
  pastime (a pipe, dice, a game board, a broadsheet, a book), or something for the home. Weapons and armour aren't
  wants. Food is kept to eat, drink is drunk, a few pieces of finery are kept, and the rest is used. A keeper whose shop
  is its own spends as a keeper instead (above).
- **Odd jobs** (*user*, `Core/RatwOddJobs.cpp`):
  - A town treasury or church with money to spare posts a day's menial work from its surplus spending, paid only when
    the work is done. Each kind does something real:
    - **Deliveries:** food bought at a shop for the watch's mess or the church's table, and carried there.
    - **Repairs:** the Town Works' materials carried out and used about the town (its repair rises 1.5 a job).
    - **Gathering and hunting trips:** a spell at the ground about the town. What it gives is sold cheaply to the town's
      makers (a shop's buying price), and the proceeds go to the poster.
    - **Scouting:** out to the wild ground and back.
    - **A hand at a producer:** a farm, quarry or dairy with little of its yield in store gets a spell's yield worked for
      it.
  - **Who takes them:** wolves without other work (labourers, the out-of-work, those working out of town, idle posts),
    the poor without a paid post, and children of 8 to 15, in the day. The poor and hungry may start at 8, the others at
    10. Children don't hunt or work as hands.
  - **Hands and pay:** a job takes 1 to 5 wolves (*user*). Trips take 2 to 5, repairs and scouting 1 to 3, deliveries and
    hands 1 to 2. Each does its share and is paid its share of `OddJobPay` (4p) a hand. Jobs last the day and aren't
    saved.
  - **Funding moves with need** (*user*): a town puts half its surplus budget into jobs, or three quarters when more
    than a tenth of its people are poor (under 12p). A church puts half of what it spends beyond alms into them. What
    isn't posted goes on materials as before.
- **Relief:** a town with many poor (over a tenth under 12p) that isn't lean funds odd jobs from up to a fiftieth of its
  treasury a day, whether or not it is above its reserve.
- **Alms at the shop:** a hungry wolf with no food, not the price of a meal (4p) and an empty larder asks alms at the
  nearest open food shop, and its town's church pays the shop for a meal on the spot. Midnight alms found the small towns'
  shops sold out; a family of salt rakers in Saltreach starved with a church in town. A wolf with a penny or two no
  longer walks to the shop to buy what it can't afford.
- **A keeper's food money, everywhere:** a business's hires, its upkeep, the children's stipends and the household's
  larder are paid from what a keeper may spend (`spendable`), never its last `KeeperReserve`.
- **The church feeds the hungry, every day:** surplus or not, each church buys a day's food for each wolf living in its
  town (by home) with no food, no pennies and an empty larder, as alms, from the shops' last meals too if need be.
- **A fair share at the shop:** a household laying in its store takes at most half of what the shop has left beyond
  today's meal, so the next customer finds some too. (In the week's test, everyone's first big trips fell on days 4 to 6
  and emptied Ridgemere's food shops.)
- **Shared fields:** a producer's place (a field, a shore) is no one's alone. Two Lakeside fishers had starved waiting
  for the one spot.
- **Subsidies** (*user*: towns and churches keep industry turning): a business that can't pay a worker's wage has it paid
  by its town's treasury, if that holds over twice the lean line a head, or else its church, if it holds over twice its
  floor (`subsidiser`). Farms short of hands get hands posted as odd jobs (above).
- **Children's friend groups** (*user*): each town's children (4 to 15), by age, in fours, rebuilt each day. At play and
  in the evenings a group meets at one place on the square. On odd jobs a child joins a friend's job first, then one
  with room for a party.
- **Wages follow takings** (*user*): each day a shop shares `TakingsShare` (3) tenths of what it took in among its help,
  on top of their wages, as far as its till can (keeping half its float; a keeper's own purse, its food money).
  - Each help's share is at most `TakingsCap` (8p) a day. Past that, a busy shop takes on more hands, a week's hire from
    those without work, rather than paying its few help ever more. The month's test had shares concentrating money in
    the busiest shops' help while those without work ran dry.
- **Churches stand by each other:** a church run dry (under 2p a head) is given what it lacks of 5p a head by the
  richest church holding more than twice its floor ("from the mother church"). Saltreach's church spent itself out on
  alms in the month's test.
- **The larder errand:** each day a household's best-off grown member restocks the larder if it holds under two days'
  food for everyone at home (one, where someone keeps the house and goes to the shop for it). So the children, the
  apprentices and the others who earn nothing eat even when the earners eat elsewhere. In the month's first test,
  starving children and apprentices with nothing in the larder were most of the starving.
- **The household purse and keeping the house** (*user*, `Core/RatwHouseholds.cpp`, daily):
  - A home's grown members share one purse: what they hold is evened out among them each day, booked as
    unearned. A keeper's own purse, which is its shop's till, is kept apart.
  - A household comfortable for a week (`ComfortDays`: four weeks' food for everyone in the purse) keeps one of two or
    more working members at home ("keeping the house"). It shops for the household (walking to the shop with the
    purse), minds the children, or goes about the town.
  - It goes back to work when the purse holds under `KeeperDays` (10) days' food. A household poor for a week
    (`PoorDays`: under a week's food) sends everyone who can to labour, homemakers too, until it isn't poor.
  - The world records spouses only when they marry in play, so the purse is shared among all of a home's grown
    members.
- **Prices follow supply and demand** (*user*, `Society::shopPrice`): what the townsfolk pay is the catalog's price, times
  the town's scarcity, times the shop's own: up to a fifth more when the shop is running short of the good (selling
  faster than it is made), a fifth less with a glut. A shop flush with money (over four floats and 200p) sells cheaper,
  a tenth off for each time over, down to three tenths: its surplus goes back to its customers. This covers food,
  household goods, wants and the collectors' buying.
- **Businesses put spare money to work** (*user*: something to sink it into that isn't charity, `businessSpends`). A
  keeper with a surplus puts half its daily spending into it; a great house three tenths, into its least improved
  business; a producer over 300p a tenth of the rest:
  - **Hires:** a week's hire (`HireDays`) of 1 to 3 hands at its premises, a day's work (`HireSpells`: 4) paid by the
    day. A workshop's next batch is begun at once; a field's yield is worked. Pay starts at `HirePay` (12p) a hand. Each
    day a place goes unfilled it rises 2p (to 24p); filled, it eases (to 8p). A wolf whose own post pays less (16p a
    day from a comfortable employer, 8p from a lean one) takes it, leaving its post unworked.
  - **Improvements:** building materials bought from the town's makers and used, then two builders hired; 60p a level
    (times the level) for up to `MostImprovement` (3). Each level makes its batches or yields 15% quicker.
  - **Upkeep** (*user*): each Restday an improved business buys `UpkeepALevel` (5p) a level of materials to keep it up,
    from its till or its house. Two weeks in a row without, and it loses a level. A business that does well and then
    falls on hard times doesn't stay efficient for ever.
- **Children would rather not work** (*user*): they look for odd jobs only from noon, and on one day in three unless
  poor or hungry. Work not meant for them (a hand at a farm, a hunt) is theirs only when no grown wolf has taken it by
  mid-afternoon.
- **Public jobs pay by need** (*user*): a town's or church's odd job pays `OddJobPay` (4p) a hand, and up to three
  times that from a poster with full coffers where many are poor (by the share of its townsfolk under 12p and how far
  its purse stands above its floor).
- **Shopkeepers' families:** checked on build 24. Of 223 keepers, 3 live alone and 86 share their home with children;
  the other 134 share it with others but have no children.

## Where money pools, and getting it back out (2026-10-06)

**The user's aims:**
- Supply and demand drive money within and between the towns.
- Every church shares one purse, so churches can put in much more if it comes to that.
- An equilibrium where hunger is only seasonal, or comes when work changes, and never lasts.
- Find where money pools, and send it back out at the lowest level.

**The month, as it stood** (`econ_watch`, DEV build 24, 28 days from 06:00, eight threads; 212,489p in all):
- The median purse fell from 50p to 19p and the Gini rose from 0.52 to 0.75. By day 28, 136 were broke and 247
  starving.
- **Where it pooled** (day 3 to 28):

  | Holder | Change |
  | --- | --- |
  | Great houses | +17,200p |
  | Shopkeepers' own purses | +18,000p |
  | Guards' savings | +5,400p |
  | Town treasuries | −18,600p |
  | Churches | −7,600p |
  | Workers without wages | −5,300p |

- **Why the houses gained.** Their shops' takings (45,500p) and caravans' trade takings (7,400p) came to them. They kept
  a reserve of four weeks of their spending, and propping up their shops counted as spending.
- **Why the towns lost.** Their wages (about 34,000p) were far above their taxes and dues (about 14,000p).
- **Why the churches lost.** Their alms grew with the poor.
- **Why the reserves win.** The reserves that holders keep add up to more than most of the money there is: keepers'
  two floats and 70p, the houses' two floats a business, the towns' and the church's. The residents had what was left.
- **The capital's residents** ended with 50,700p, as much as the next two cities together.

**What was built (the user chose all of it):**

- **One church** (`Society::SharedChurch`, `town:all:church`).
  - Every town's church draws on one purse: tithes, the collection, clergy's wages, alms, odd jobs, subsidies.
  - Each town's church keeps its own goods: candles, bread for alms and bandages are in its storehouse
    (`Society::churchStore`, `town:<town>:church`). It buys them at its own town's shops and gives and uses them there.
  - The purse's floor is ChurchHead for everyone in the land. Its surplus is spent in every town, each its share by
    need (its poor, and a tenth by its size).
  - When the purse opens, every town founds it.
  - An older save's churches' money joins it the next day (`joinChurches`).
  - The "mother church" is gone: there is only one.
- **The tax is progressive:** a tenth of the week's profit, and a fifth of the part above `TaxBand` (70p).
- **A wealth tithe** goes to the church each week: a twentieth (`WealthTitheShare`) of what anyone holds above its
  comfortable line (`Society::wealthLine`):
  - a resident's line is four weeks' food;
  - a keeper's (its purse its shop's till), that, its food money and a float;
  - a great house's, its floor.
- **The capital shares by need.** Each week it sends the towns half of what it holds above four weeks' spending, by how
  many of their folk are short of a week's food ("from the capital, for the poor").
- **The dole** ("the church's dole", unearned):
  - Each day every household short of a week's food is given `DoleADay` (2p) a member in coins, to its eldest, the
    poorest first.
  - It is never more than a thirtieth of the church's purse a day.
  - The coins are spent at the household's own town's shops: money in at the bottom.
- **Great houses give back.**
  - A house keeps only its floor: 100p and a float for each of its businesses (`houseFloor`). What it props its shops with
    and sends on the road came back to it, and is no reason to keep more.
  - It spends a fifth (`HouseSurplusShare`) of the rest a day: two tenths in its businesses, then four tenths as
    **patronage**.
  - Half of the patronage is odd jobs; half is food for its town's poorest (those with nothing to eat and under 12p:
    "the house's charity").
- **Shopkeepers keep less:** their food money and one float, and a fifth a day spent above that (`KeeperSurplusShare`).
- **Food between towns.** A town whose shops hold under two days' food for its people (`FoodDaysKept`) wants, for each
  of its food shops, 12 (`FoodKept`) of each of the three foods the other towns have most to spare. Standing orders
  bring them, as a maker's materials come.
- **Food shops sell any wholesome food** they have in, besides their own wares (`Society::foodShop`, `sellsFood`).
- **Producers sell food to their town's food shops** (`producersSell`, daily):
  - the food they bring in beyond 10 (`ProduceKept`);
  - a shop takes up to 30 of a food (`FoodShelf`), at a shop's buying price, as far as it can pay;
  - each day from a different shop of the town.
- **`econ_watch` records each town each day:** its residents by home, what they hold, the median, its food shops and
  their food, and how many are short, broke, hungry and starving.

**The month, now:**

| Day 28 | Before | Now |
| --- | --- | --- |
| Median purse | 19p | 34p |
| Gini | 0.75 | 0.68 |
| Broke | 136 | 47 |
| Starving | 247 | 163 |
| Great houses | 37,400p | 21,300p |
| Shopkeepers | 51,700p | 37,700p |

- Nobody starves until day 21. Ser Ferro and Westmarch have none starving at the end, Ridgemere 11.
- Tests: `roads_tests` `oneChurch` (an older save's churches join the one purse) and `theDole`. The reckoning test
  counts the progressive tax, the wealth tithe and the capital's share.

**Still open** (2026-10-06):
- **The last week still falls.** Starving goes from 0 to 163 between days 21 and 28, mostly after the Restdays. By then
  the shops' food is gone: 5,300 at the start, 1,100 at the end.
- **The small towns run out first.** Fenhollow, Saltreach, Cinderbrook, Lakeside and Amberford have 5 to 15 meals' worth
  in their shops by day 28.
- **Food piles up in larders, not in the shops.** About 1,900 fresh fish sit in households' larders after a week.
  - A household takes its best food from the larder first, so the fish (15) is eaten last and never runs out.
  - Nothing spoils: `items.json` gives each food how long it `keeps` (fresh fish: a day), but nothing reads it.
- **Too few work the land** (open question 7): the shops and their help are almost half the working wolves.

## Spoilage, buying, prices, wages, and more on the land (2026-10-06)

The user, after the first pass:
- Food should spoil, fresh fish keeping long enough for the road (three days).
- Rebalance the jobs, with sheep farms (a pasture and a barn: mutton, a dear meat) and rabbit farms (a large farm of
  hutches: rabbit, the cheapest meat), placed in Atlas and saved.
- Move food-making materials between towns (flour and the like), not only food.
- The more an employer holds, the higher the wages it pays, rather than giving money away.
- Prices dynamic: cheaper food in poor places; shops that can't sell mark down (never at a loss), and that trickles back
  to the makers and the land.
- Wolves buy by their purse and their next pay, are choosy about price when poor, and don't keep a week's stock because
  they are told to.
- Everything in the design that keeps the simulation fast (doc 31).

**Spoilage** (`Society::spoil`, daily; `Item::keeps` from `items.json`):
- Each account's food is kept in batches by the day it came in. New stock is a batch of today; what has gone went from
  the oldest. A batch spoils when it is older than its food keeps: its day in, then `keeps` more days.
- Goods bought are fresh to their buyer; on the road, in a caravan, they age.
- Fresh fish keeps three days. Raw meat and rabbit keep three, shellfish and offal two, bread four, cheese forty, salt
  fish 120.
- A town's stores (its granary) don't spoil, and nor do players' packs (not yet). The batches aren't saved: a loaded
  world's food is all fresh.
- **Eaten first:** a household eats what spoils soonest (`Society::eatFirst`), the most nourishing of those.
- **Producers keep what they bring in to sell.** A rabbit farmer's rabbits, a shepherd's mutton and a fisher's catch go
  to the shops, not into the family larder. Before this, 8,600 rabbits rotted in larders in two weeks.

**Buying** (`Society::buyFood`):
- **How long a grown wolf lays in for:** as many days as a third of its purse buys, at most its household's habit (5 to
  8 days). One paid each day lays in at most three; a child at most two, for itself.
- **Less what is at home:** the household's larder is counted, so two members don't both stock it.
- **Never more of a food than the household eats before it spoils.**
- **Short of a week's food money,** it buys what feeds most for the money; with more, by its taste too.
- **A shop's meals:** about two days of what it sells (`mealsSold_`), not a fixed dozen.
- The household's size comes from the day's household index, not a walk over everyone.

**Prices** (`Society::tendPrices`, daily; read when selling):
- **Markdowns:** a shop whose day's takings fall under half a day's running (a float over `FloatDays`) marks its goods
  down a twentieth a day, to `Markdown` (0.6). As it sells again it marks them back up.
- **Poorer towns:** food is cheaper by the town's median purse against the land's (0.8 to 1.15).
- **Nothing sells under `CostFloor`** (0.6) of its price.
- **Materials** come cheaper from a seller with plenty and dearer from a scarce one (`supplyFactor`). A producer's price
  only falls, with a glut; holding little is how it sells.
- **What a marked-down shop pays** its suppliers and producers is lower by its markdown: down the line to the land.
- **Food shops sell any wholesome food** they get in (`sellsFood`). Producers sell their food beyond 10 to their town's
  food shops (`producersSell`).

**Wages by what the employer holds** (`Society::wageFor`):
- A spell pays 1p from a lean employer. Otherwise it pays 2p, and a penny more for each time over its floor the employer
  holds, to `MostWage` (6p).
- The floors: a shop's float, a great house's floor, a town's `TreasuryHead` a resident, the church's `ChurchHead` a
  resident of the land.
- **The great houses' patronage is gone**, at the user's word: their wealth reaches their people as wages. Their surplus
  share is back to a tenth.

**Materials between towns:**
- Standing orders already carried oats, milk, apples, vegetables, timber and the like. There was no flour: a mill made
  only half a store, and other towns' orders take what is over half.
- A maker of what other trades work with that keeps (a mill's flour, not a baker's bread) now makes a full store
  (`SuppliesKept`), so there is flour to spare and order.

**Jobs and farms** (`tools/worldgen/farms.py`; DEV revision 16, build 25):
- **New building kinds** (`buildings.py`):
  - `barn`: lambing pens of straw behind hurdles, hay by the door.
  - `rabbitry`: three rows of hutches down a long shed, a skinning bench by the door.
- **New yards:** a sheep farm's is a fenced pasture. A rabbit farm's (`hutches` in `industry.paint_yard`) is rows of
  hutches in a fence, with gaps to walk between.
- **New producers** (`crafts.json`):
  - `sheep_farm` ("raises sheep at …"): mutton and wool.
  - `rabbit_farm` ("keeps rabbits at …"): three rabbits and a small pelt.
- **New goods:** mutton (6p, 35 nourishment, keeps three days), sold and bought by butchers. Rabbit is now 1p.
- **Built:** 14 rabbit farms and 7 sheep farms, in the country around the three cities and eight towns; every door and
  work spot is walkable from the road.
- **The shops:** each settlement keeps a shop for about every 15 people (a city 10). It always keeps the first, nearest
  the square, of each kind it needs (`ESSENTIAL`: a bakery, a general store, a provisioner, a fishmonger, a butcher, an
  inn, a smithy). The rest close from the edge of town inwards: 33 closed.
- **A closed shop becomes its family's house** ("The Dustcote House"): its counters are tables now.
- **The people:** keepers and their help went out to the farms, then about half the towns' labourers where hands were
  still short. 86 moved.
  - Everyone kept their name, home, family and looks. Over 64, they retired.
  - Some small towns' farms are a hand or two short.
- Revision 15 is backed up under `artifacts/backups`.

**Measured** (`econ_watch`, build 25, 28 days, eight threads; tick 0.055 ms):
- Starving: 0 on day 7, 14 on day 14, 9 on day 21, then 145 by day 28 (179 before producers' prices only fell with a
  glut; 246 before the producers kept their catch; 396 on build 24 with spoilage). The median purse ends at 38p.
- Amberford, Westmarch and Lakeside end with one to three starving. Cinderbrook (31) and Ridgemere (21) are the worst;
  shops' food falls from 4,500 to 900 over the month.
- **Still open:** residents spend about twice on food (about 10,000p a day) what wages bring in (about 5,000p). Paid
  workers average about 5p a day against 16p if they worked every paid spell. The money gathers with the producers who
  sell to the makers (the richest tenth hold 48%), while the treasuries and the church drain.
- Tests: `roads_tests` `spoilage`; the crafting tests count carted-in prices by the seller's supply, and winter's wheat as
  brought in (the mill buys it as it comes).

## Pressure: keeping money moving (2026-10-06)

The user: money pools somewhere in every test (this time with the farms), not always in the same place. Build a
pressure system: always a sink pulling money elsewhere. Farms should have something cities need, and cities something
other cities need. First:
- hides and wool from sheep farms, poorer hides from rabbit farms, bought and ordered by tanneries;
- salting and smoking meat for keeping, with some kept fresh for hot meals;
- 500,000p in the world, spread evenly.

Then run tests of where money goes, with temporary goods, work and businesses, and 2 to 4 week runs, to learn what
healthy circulation looks like before building anything permanent.

**Built (permanent):**
- **Hides:**
  - Sheep farms bring in a sheepskin with their mutton and wool. Rabbit farms bring in two small pelts with their three
    rabbits. Small pelts are now 1p.
  - Tanneries buy both (`leather_sheepskin`: 2 sheepskins and bark make 2 leather; `leather_pelts`: 3 pelts and bark
    make 1). They also sell sheepskin.
  - Standing orders carry hides between towns like any other material, since a tannery wants what its crafts work with.
- **Preserving:**
  - Butchers and smokehouses smoke mutton (`smoked_mutton`, 8p, keeps 30 days) and rabbit (`smoked_rabbit`, 2p, keeps
    20).
  - Butchers and provisioners salt pork from raw meat and salt (`salt_pork`, keeps 90).
  - Smokehouses and provisioners make jerky; fishmongers and provisioners salt fish.
  - **Fresh kept back:** a shop that sells a fresh food it also cures keeps `GoodsKept` of it fresh on the counter and
    cures only what is over (`Society::craft`, `fresh`).
- **The world's money** (`Society::worldMoney`, `WorldMoney` = 500,000):
  - Once, at the end of the first day's pass after the purses are founded, so the town buyers and the church have their
    own first. Founding is now in two steps (`PursesFounded` = 2).
  - What's missing is made up: 85% shared alike among the grown residents (about 290p each), the rest to the towns'
    treasuries by their residents.
  - Only a land of 500 residents or more (`WorldMoneyResidents`): test villages keep their own starting money.
  - The DEV world gets it the first time it runs with this.
- **Buying food, fixed for a rich land:**
  - A food fills the household's stock only as far as it keeps, counting everything bought and at home: three days of
    fresh fish, not three days each of fish, rabbit and mutton.
  - The household's stock counts what its members carry home as well as the larder.
  - The meal bought now is what the wolf will eat by its hunger: a wolf eats one bowl at a time.
- **Watching** (`econ_watch`):
  - `trade.csv` books coins that change towns. A resident counts in its home town and a shop where its keeper works;
    the church, the capital, the houses and the road are places of their own.
  - `food.csv` gives each food's daily brought in, crafted, used, eaten, spoiled and bought.
  - `roster.csv` lists who works where.
  - Each town's day line gives what its producers, shops and treasury hold, and producers are a holder kind of their own.

**Trials (temporary):**
- Switched on by the `RATW_TRIAL` environment variable (comma-separated names, `Society::trial`) and off in play. Each is
  marked `TRIAL` in the code.
- Trial goods come from a copied data directory (`RATW_DATA_DIR`), so the real data and world are untouched.

| Trial | What it does |
|---|---|
| `church_valve` | The church keeps only its floor (`ChurchHead` a resident), not four weeks of its spending, which grows with what it's given. |
| `church_share` | A tenth a day of what the church holds over its floor goes to households under the land's median purse a head, by how far under. |
| `rates` | The tithe and each town's tax scale with what the collector holds against its need: double when empty, normal at its need, nothing from twice it. The church's need is its floor; a town's is its floor or four weeks of spending. |
| `tithe_relative` | The wealth tithe's line is at least twice the land's median purse. |
| `house_need` | A great house holding twice its floor leaves its shops three floats instead of one. |
| `wages_up` | A house's shop pays by the house's wealth when that's more than its till's, with the cap raised to 12p. |
| `markup` | A shop selling twice a day's running raises prices a twentieth a day, up to 1.25×. Its suppliers aren't paid more. |
| goods `gear` | Farm gear (5p), made by the cities' tinkers from timber. Every producer wears one out every few days and buys it at the town's general store, which orders it from the cities. |
| goods `wares` | Each town's general store becomes a trial store making its own town's ware from firewood. Every household wants each other town's ware every 11 days, so every store orders the others' wares. |

**What the runs showed** (DEV build 25, 500,000p, 14 to 28 days):
- **The money pools where an account's outflow is set by need but its inflow by activity.**
  - The church takes a tenth of every weekly gain (about 14,500p a week) but could only spend on the hungry and the
    poor, about 1,000p a day. With everyone well off, it grew 340% in four weeks.
  - The great houses take every shop's takings above the float and spend a tenth of the excess, mostly back into their
    own shops, which returns it. They grew 114%, nearly all of it in one house: the Court of Ser Ferro, while Ser Ferro
    itself lost a fifth.
  - Producers sell about 7,800p a day to the makers, but their costs are only their own living.
- **Spending that scales with holdings stops the pools.** Four weeks, all valves on, against the plain run:

| 28 days | Plain | Valves + gear + wares |
|---|---|---|
| Church | +338% (still +26% in week 4) | +86%, level in week 4 (−4%) |
| Town treasuries | −28% | −7% (−4% in week 4) |
| Great houses | +114% (+11% in week 4) | +91% (+14% in week 4) |
| Workers | −18% | −11% (−1% in week 4) |
| Producers | +16% | +14% (+4% a week) |
| Average town swing | 17% | 12% |

- **Taxes and tithes by need work.** The towns' budgets balance and the church levels off. The rest of the church's money
  goes to households below the median.
- **The great houses level off slowly** (week 4: +11 to +14%). Higher wages barely move them: their shops have few
  staff.
- **The farms still gain about 4% a week.** Farm gear moved 273p a day from producers into the shops and cities (base
  50p). There were too few tinkers (three, all in cities), so the stores were short most of the time. Made by every
  town's smithy too, gear gave the lowest inequality (Gini 0.42) and the narrowest town spread, but cut trade between
  towns.
- **Markups move money from farms to shops, but the workers and the house-owned shops pay.** Producers fell to −3%,
  workers to −17%, the houses gained most, and hunger rose. Not a good trade.
- **Town wares pulled about 1,000p a day between towns.** That's real but small against 500,000p. Three times as many
  bought did no better (the caravans' weekly loads limit it).
- **Food, not money, is now what goes short.** Even with full purses, 20 to 145 starve by days 14 to 28:
  - About two items spoil for every three eaten, nearly all in household larders.
  - Larders fill in waves (8,400 items put away on day 8) and rot by days 11 to 14.
  - The buying fixes above didn't change it. The next step is finding what synchronises the waves.
  - Farmers bring in nothing on Restday.
- **Speed:** a week in 1.5 minutes at eight threads (0.045 ms a tick), as before. The trials cost about 1%.

**What healthy circulation looks like, so far:**
1. Every pool has an outflow that grows with what it holds over its need: rates, shares, wages.
2. Every producer has costs that grow with its output, bought from the towns and cities: tools, feed, seed, hands.
3. Every town has something the others want, steadily.
4. Thresholds are relative (to the land's median purse, to an account's own need), not fixed pennies, so the system
   works at any amount of money.

## Steady taxes, smiths' gear, the household's food plan, and the ledger (2026-10-06)

The user's decisions on the pressure trials:
- Tax and tithe stay the same for everyone, except those below the poverty line.
- Farm gear is good; it comes from the blacksmiths.
- Feed would only be grain from farms, so it adds little.
- Food spoiling on the same day means everyone buys more than they need. Wolves should plan by the meals they eat and by
  how long food keeps:
  - 2 to 5 days in the larder, a different number for each household;
  - always enough to last over Restday and holidays.
- Then track money and food in detail, to see where to adjust supply and demand.

**Built:**
- **Tax and tithe:**
  - Steady for all. The `rates` trial and the `markup` trial are removed.
  - A resident holding less than `PovertyLine` (two weeks' food, 70p) pays neither that week.
- **Farm gear** (`items.json`, `crafts.json`):
  - Made by smithies from 1 iron bar and 2 timber, six at a time, at 5p each.
  - Every producer wears one out every 4 days, bought at its town's shops.
  - Every town has a smithy; the iron comes from the foundries and the mines.
- **The household's food plan** (`Society::buyFood`):
  - A wolf eats one thing at a sitting, from 60 hunger to below it; a day's need is about 50 nourishment, a meal.
  - A grown wolf lays in for its household only when what the household holds (larder plus members' pockets) won't last
    until the shops are properly open again (`shutAhead`: Restday and festival days).
  - It then buys up to the plan: `stockingDays`, 2 to 5 for each household, fewer if a third of the purse won't stretch,
    and always enough to cover the shut days.
  - Each food only up to the days it keeps, counting what's held already.
  - The plan follows what the household actually eats from home (`larderUse_`, a running average of what its larder
    gives), not a meal a member, since many eat out. A larder that ends the day bare moves its plan back up toward a
    meal each.
- **Eating and spending:**
  - A hungry wolf buys one thing as its meal and eats it at the counter.
  - Households eat whatever has the fewest days left, by each batch's age (`eatFirst(id, …)`).
  - A treat is one thing, eaten at once.
  - Children buy food only when hungry with nothing on them (one thing); otherwise trinkets.
- **A loop closed** (`Society::craft`): a shop that both sells a good and works with it (a fishmonger's fish) restocks
  it as a supplier, never from the town's other suppliers. They were selling the same fish back and forth, about
  29,000 a week at 17,000p a day, and each sale made the fish fresh again.
- **The ledger** (`econ_watch`):
  - `food_towns.csv`, `prices.csv`, `larders.csv` (each town's households by days of food at home, and the food on its
    shelves and with its producers), `town_flows.csv`, `food_moves.csv` (every movement of food, by why and between
    whom).
  - Scratchpad scripts `detail.py` and `build_report.py` read them. The report page is the artifact "Upper Accord
    Ledger".

**Measured** (28 days, DEV build 25, 500,000p; day 7 to 28):

| | Before this round | Plain | With valve trials |
|---|---|---|---|
| Food spoiled, of what is eaten | 52% | 23% | 24% |
| Starving on day 28 | 36–51 | 17 | 12 |
| Gini | 0.47 | 0.44 | 0.44 |
| Producers | +20% | +27% | +34% |
| Town treasuries | −29% | −43% | −44% |
| Church | +100% (valves) | +279% | +42% (−14% in week 4) |
| Upper Accord | +15% | +23% | +34% |

**What the ledger shows:**
1. **The farms are the pool.**
   - Producers take in about 6,800p a day from the shops and caravans and keep about 1,300p of it.
   - They bought only 114p a day of gear: the smiths can't get iron.
   - About 38 iron bars are made a day against some 5,500 maker-hours a day of smiths waiting for them, and charcoal is
     short too.
   - The farm → smithy → foundry → mine chain that would carry farm money to the mining towns is cut at the mine and
     the kiln.
2. **Upper Accord gathers it.** Its producers hold 42,000p, and the capital's wages are paid there.
3. **Steady taxes leave town treasuries short:** about 1,100p a day more goes out in town wages and buyers than tax
   brings in.
4. **The land makes about what it eats.**
   - About 62,500 nourishment a day is made and 62,700 eaten.
   - With a quarter of it spoiling, larders run down: about 300 of 450 homes hold under a day by day 28.
   - Fresh meat spoils most (rabbit 32%, mutton and fish 27%); cured foods lose nothing.
5. **Scarcity shows in prices but nothing answers it.** Raw meat sells to makers at 5.8p (catalog 3p), a meal at 8.2p
   (6p), smoked fish at 9.2p (5p).

**Where to push next:**
1. More iron and charcoal, so the gear flows.
2. A quarter more food, or more of it cured.
3. Town budgets set by their takings.
4. A pull toward the small towns.

Speed: unchanged, a week in 1.5 minutes at eight threads.

## Iterating on the pressure (2026-10-06, after d3a0e43)

The suggestions, tried in 28-day runs:
1. More iron and charcoal.
2. More food, or more of it cured.
3. Town budgets from takings.
4. A pull toward the small towns.

**Fixes (permanent):**
- **A producer's goods for sale stay out of the larder** (`Society::forSale`). The resident's own put-away already
  skipped them, but a producer doing the household's daily shopping put everything edible it carried into the
  larder: about 8,500 rabbits in three weeks, which rotted there.
- Goods for sale don't count as the household's food when it plans its shopping.
- **A missed sale counts as demand.** A producer who finds no gear in town adds to the shops that sell it, as a sale
  missed (`soldToday_`, `sellRate_`).

**Trials added (RATW_TRIAL):**

| Trial | What it does |
|---|---|
| `produce_more` | A producer works on until it holds 40 of a thing (not 20) and keeps 5 (not 10) when it sells to the town's food shops. |
| `make_what_sells` | A maker makes first what sells fastest at its shop, counting sales missed for having none. |
| `cure_more` | A shop curing food (an output that keeps longer than its input) keeps half a store of it, not four. |
| `town_budget` | A treasury's extras (its buyers' funds, relief odd jobs, wages it covers for others) come only from its takings above its wages, by running averages. Its savings above its reserve still go out a tenth a day. |
| data: iron | Mines bring in 2 (or 3) ore and charcoal burners 2 (or 3) charcoal a spell (trial data directory). |
| data: gear | Gear worn out every 2 days. |

**Measured** (day 28; days 7 to 28):

| | Plain (d3a0e43) | Produce more | Every trial, gear every 2 days |
|---|---|---|---|
| Starving / hungry | 17 / 220 | 1 / 98 | 0 / 64 |
| Food made a day (nourishment) | 62,200 | 68,700 | 64,600 |
| Spoiled, of eaten | 23% | 34% (before the larder fix) | 25% |
| Average town swing | 17% | 14% | 8% |
| Workers | −7% | −5% | +2% |
| Producers | +27% | +25% | +31% (+6% a week) |
| Great houses | +59% | +39% | +25% |
| Church | +279% | +279% | +40% (−12% in week 4) |
| Town treasuries | −43% | −45% | −36% |

**What was learned:**
- **The land was idle.** Producers stopped at 20 and kept 10: about 30% of what the rabbit farms could bring in.
  Letting them work on ends starvation. Spoilage rises with the extra food, then falls back to a quarter once the
  larder leak is fixed and shops cure more.
- **Iron doubles or triples with the mines and kilns, but gear stays small** (100 to 146p a day).
  - The smiths' iron goes where it sells fastest: nails, fittings and locks for the Town Works and for shops and houses
    improving their premises.
  - That flow is healthy (the rich spend surplus on city goods), but gear is too small a want to carry the farms'
    money.
  - Producers keep about 1,100 to 1,300p a day. To spend it in the towns they need costs of about 3 to 4p a day each,
    three times what gear can be.
- **The town budget only slows the treasuries' drain** (−36% against −45%). Their wages alone (about 1,450p a day)
  nearly match the tax (about 1,600p). With the tax steady, the wage bill decides it.
- **The valves keep the church level and the towns closest.** Saltreach loses most in every run (17 to 29%).
  Cinderbrook gains with more iron (+7% at triple).

**Next:**
1. Larger farm running costs: hands at harvest, seed bought back from the mills, tools worn by output.
2. Town wage bills by takings.
3. Saltreach's salt into more cured food.

## Later (noted 2026-10-05)

- **Skills from odd jobs and hires** (*user*): working a trade's odd jobs and hires should teach its skill, leading
  toward apprenticeships and, in time, taking over a business. Not built: the jobs teach nothing yet.

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
