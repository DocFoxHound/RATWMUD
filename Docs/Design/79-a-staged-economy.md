# 79. A staged economy: an RPG economy that looks live (2026-11-08)

Drafted 2026-11-08 and revised the same day after four reviews (the Review section at the end lists what changed).
**Agreed by the user on 2026-11-08** ("Go with the recommendations": all 30 decisions in section 22 take their
recommendation; doc 79 is built before doc 78). **Plan only so far.** No code was changed, nothing was built or run, no server was started and no database was touched. The
code is `ridgemere2` at c8ac05c. Line numbers come from seven read-only code maps and from the reviews' checks the same
day. Other sessions are working in `.claude/worktrees/`, so line numbers will drift.

How this draft was made: seven code maps (the money core, production and trade, NPC behaviour, prices and players,
steering and the DM, sims and tests, the dependents of conservation), a pass over how other games fake an economy,
three competing designs and three judges. The judges picked the "believability first" design as the spine and grafted
on the rules that stop exploits, the rules that keep a host's edits from breaking anything, and the cheapest
mechanics. Four reviews then checked it: one against the code, one for exploits and saves, one for believability (a
week as a player in Ridgemere and Coldwater), and one for scope and hours.

Words used the same way throughout:
- **staged:** worked out to look right, not kept to add up. NPC purses, shop tills, the treasuries, shop shelves,
  caravans, ships and farm output are all staged.
- **real:** player money, and the escrow that holds player money. It is the only money that has to add up.
- **the slider:** the DM's one economy control, renamed **prosperity**: −100 (hard times) to +100 (boom), 0 for
  ordinary times.
- **a band:** an NPC household's economic status: destitute, poor, comfortable or rich (section 6).
- **class:** upper, middle or lower, from the household's posts (`classOf`, `Core/RatwCareers.cpp:460-517`). It
  already exists. It stays as a second axis beside the band.
- **a market:** a trade point with merchants: the city, and each community with a merchant post (section 5.1). It is
  not a town: law, the Watch and the town treasuries stay as they are. In code it is `TradePoint`, because `Markets`
  (`RatwSociety.h:106`) and `items::cityLoop().markets` already exist.
- **the Board:** one small saved table per world: for each market and good, its pressure; for each market, its
  Times; for each farm area, its harvest; and the few commitments players can see (section 5.2).
- **pressure (P):** a number per market and good that player trade and visible events push up or down, and that
  decays back to 0. Prices, shelves, talk and caravans read it.
- **Times:** a market's mood: hard, lean, ordinary, good or boom.
- **an account holder:** the central account behind a character (from the ticket, doc 77), or the login in password
  mode, or the character when neither exists. Every per-player cap counts per account holder.
- **a faucet:** any move of coins or goods from the staged side to a player. **A sink:** any move from a player to
  the staged side.
- **shown price:** the price the client lists. For a good under 3p it is the price of a dozen (section 7.1).

Standing rules, kept:
- Births match deaths, one for one. NPCs are never generated. Empty posts are filled from the city, then from the
  unplaced NPCs of the Country (memory `births-match-deaths`).
- Every job keeps its apprenticeships (memory `living-npc-roadmap`).
- Characters are male or female only.
- Never the word for the domestic animal in anything players read (wolves; pups for young).
- NPCs talk through Tracery dialects and salience (doc 62) and through the NPC Mind. The Mind bills the real model;
  its tests stay offline.
- DM-tweakable rules, robust to a host's edits in the DM tool and Atlas (doc 78).
- The perf gate is `world_check --players 20`.
- Ask before simulations and paid model runs. Nothing writes PROD.
- No data migrations: migrations change schema only.

## Headline

- **The conserving economy goes.** The city loop, the steering and controller, farm money, the Wharf's money valve,
  the store room, minting, `minted`/`sunk` and the conservation gates at restore all come out: about 7,800 lines of
  Core, about 8,200 lines of economy tests, about 1,400 lines of `world_check`, about 10,600 lines of tools and about
  2,300 lines of DM and Editor screens. Farm staffing, the farm list and the fast-sim flags stay.
- **Players keep a real economy.** They buy, sell, earn, rent, pay fees and rob each other with real pennies. Player
  purses and the escrow that holds player money are the only money that has to add up. The 260 `conserved()` asserts
  become `realBalanced()` asserts, so tests still catch a penny made from nothing. The DM gets a daily report of
  player money made against money sunk.
- **The world's side is staged and elastic.** NPC purses, tills and treasuries stay as accounts, so the existing
  `shift(` call sites (about 165 in 31 Core files) keep working. Each night they move toward a target set by the
  slider and the NPC's band, and they never need to add up. A theft, a gift or a festival prize still shows for a few
  days. A staged payer is topped up when it has to pay a player, so rewards and sales always pay, within caps.
- **Every NPC household gets a band:** destitute, poor, comfortable or rich, the four `wealth` words the talk library
  already uses (about 480 lines test `wealth` and keep working). Bands are given **by rank**: households are scored
  (class, home, earners, a bounded family fortune, district, recent events, the market's Times) and cut into bands
  by the slider's shares. Some households are always richer and some always poorer, at every slider position and on
  a world of any size. Destitute needs a reason (no earner, a lost post, a death, no home).
- **The band drives behaviour:** the food a household gets each day and how often it goes short (hunger stays real,
  and it follows the bakery's shelf); its purse; what it buys; how many turn to theft; who begs; how they talk; and
  the NPC Mind's one-line "Means" note.
- **Prices follow players.** A Market Board keeps a pressure for each market and good. Players buying bread push its
  price up, and the dozen price on the list shows it. A player dumping iron ore pushes ore down at once, and the next
  morning the smiths' bars and nails a little ("working the new ore"). The next wagon out carries some of the glut to
  the next market, and that market hears of it. Every piece is priced at the pressure after the pieces before it, so
  a bulk buy or a dump pays its own price impact.
- **Trade looks live.** Wagons driven by real carters run on committed, posted schedules between real markets; farm
  carts come in from staffed farms every morning; ships call at the Wharf where there is one. Imported goods refill
  only when a wagon or ship brings them, so a late wagon shows as thin shelves. Staged events (a House wedding, a fire
  at a works, blight) give a quiet world something to talk about. Bandits, bounties, escorts and courier carts come
  back to life in Ridgemere 2.0, where the road layer has been off since doc 66.
- **Every change has a cause a player can find.** A slider move comes with a reason in the world (a war levy, a
  silver strike). A DM override posts a notice. Talk names a price move only when the shown price really moved.
- **No setting can open a money loop.** Rules inside `quote` hold whatever the DM sets: a bought lot of at least 1p;
  sold lots rounded down; no profit from buying and selling back in one shop; one bound on how far the whole price
  factor can differ between places and seasons; qualities moving together; and a backstop: a sold lot never pays
  more than 0.95 × the cheapest lot of it anywhere. Every faucet has a cap per account holder. A rule can make prices
  silly; it can never make them a pump.
- **Lenient on read, strict on write.** Restore, authoring and the world package never refuse a world over economy
  data. Unknown ids are dropped with a warning, unknown items in players' hands go to a quarantine list, missing
  accounts are made at their target, out-of-range values are clamped. The world package's coins-add-up check goes.
- **The DM's levers shrink to a handful:** the prosperity slider (with its cause), Never Hungry and Never Broke as
  plain clamps, the seasons, harvest overrides, a Times nudge per market, a band override per household, the
  pressure settings, the faucet caps, ships a week, caravans and staged events, and "reset prices". Everything else
  is cut (section 12).
- **It should be cheaper.** The hourly household shopping (4.2% of instructions in doc 69, before farms and the
  Wharf were added), the store-room restocking and the 18:00 money spike go. What comes in is a few hundred numbers a
  day. The perf gate gains two windows, 17-19 and 23-25, beside 6-8.
- **The client changes a little:** each ware gains a list count, a smallest sell lot and a trend marker, and the
  keeper's purse line becomes "BUYING TODAY".
- **Size:** about 158-221 plan hours, about 73-101 expected, in three slices, with two reviews at face value. Slice
  1 (the loop off first, then the Board, bands and the cut) is about 66-90 plan, 33-45 expected. The first check-in
  with the user is Phase 0's list of what breaks when the loop is switched off.

## 1. The user's direction (verbatim)

The user, 2026-11-08 (verbatim excerpts):

> overhaul the entire economy again because I do NOT think it's going to work very well for players that launch and
> make their own servers/worlds. Our economy is finely tuned to work in a nearly lifelike way, and I think we just
> need to SIMULATE an economy working...

> pretty much undo all of the real economy stuff and just make a roleplay-game-like 'economy' that LOOKS like it's
> working live... imports/exports through the port (if there is a port) and trades between towns and outlying farms
> and areas is all simulated to look like its happening, complete with caravans and trade between towns and whatnot.

> The Economy slider will influence NPC simulated wealth, to include how many turn to theft, how many are going
> hungry, and how they talk about their economic statuses. Everything about their hunger and buying habits should be
> gauged by the slider. That said, portions of the population should always be richer or poorer than the rest. I
> think what we need to introduce is an economic status categorization for the NPC's so that their behavior can be
> determined better...

> throw away all of our delicate economy work in favor of an RPG-style economy that LOOKS and ACTS real but is very
> much fake and automatically tuned by the slider.

The user's answers the same day:
- Players still buy, sell, craft and earn. What is simulated is the players' effect on the economy, verbatim: "if
  there's a lot of players in an area buying bread then bread should increase in price. If another player dumps a
  lot of iron ore into the economy there should be a ripple effect of how that changes things. But it should all be
  simulated."
- NPC prices still react (to that).
- The DM's economy levers, verbatim: "I don't know if they'll even be needed anymore". This doc proposes keep,
  change or cut for each (section 12).
- The world package's coins-add-up check goes away.

What that means here:
- The aim is **believability**, not balance. Nothing is tuned by long runs. Behaviour is pinned by small seeded tests
  that run in seconds (section 16), and the look is judged by the user on a scratch server in a boxed last phase.
- Players' money stays real, because players can feel a fake purse. Everything players only see or hear is staged.
- A host's world must work after any edit: a deleted Wharf, a town without merchants, a family moved, a rule set to
  nonsense. There is no invariant left for an edit to break, except that player money adds up.
- A small hosted world, with one to five players, must still look alive. Player trade alone won't move a city, so
  wagons, harvests, ships and staged events carry the motion.

## 2. What the plan rests on (today's code)

Prices and trade:
- `Society::quote` (`Core/RatwSociety.cpp:425-475`) makes every price a player sees: the shop list
  (`Core/RatwGame.cpp:4100-4140`), the keeper's trade answer and `Society::trade` (:476-492). Its formula is
  catalogue × `shopFactor` × share, with share 1 to buy, 0.9 at a market stall (the lot rounded down) and
  `SellBackShare` 0.55 to sell (the lot rounded to the nearest penny). **Both lots are at least 1p a piece**
  (:451-456).
- The shop list shows `lot(1)`, the price of one piece (`RatwGame.cpp:4130-4133`). A 1p piece at any factor up to
  1.28 shows 1p, so a 1p good's listed price never moves today.
- A shop's buying is capped by what it holds (40 of a supplied or bought good, 4 of a wearable or sword, 24 meals, 20
  herbs) and by its till's cash ("The trader cannot afford that purchase", :466-468).
- `Society::transfer` refuses any price under 1p a piece (`price < 1`, :387). `Society::trade` splits a lot into
  `each = total / quantity` plus the odd pennies (:483-485). So a sold lot worth less than 1p a piece can't execute
  today.
- The client turns off Buy or Sell when the unit price is 0 and compares the unit price with cash: `canTradeItem`
  (`Client/src/game/state.ts:2456-2460`).
- **58 catalogue goods cost 1p**, among them bread, wheat and iron ore. Salt, herbs, flour, fish and timber cost 2p,
  nails 3p, hides 5p, iron bars 9p (`Data/Items/items.json`).
- **No shop buys iron ore from players today.** `crafts.json` `buys` (:239-253) lists 12 kinds of shop that buy
  gathered and hunted goods, and ore is in none of them. `general` buys stone, clay and sand, not timber (:243). The
  ironworks and foundry already buy `iron_bar`, because a maker deals in what it makes.
- `Game::afterFactionTrade` (`Core/RatwGameFactions.cpp:467-492`) pays a trusted Chapter 15% back, and a sworn one
  25%, from the merchant's purse after the sale. Today the merchant's real cash limits it.
- Season price ranges in `city_loop.json`: fruit runs 80-140 (×1.75), grain 85-115 (×1.35).

Money and saves:
- `Society::restore` (`RatwSociety.cpp:973-1121`) refuses a save when cash doesn't equal `minted − sunk` (:1037);
  when an account names an unknown resident (:1030); when a resident's role changed (:1039-1048); when the resident
  count differs from the authored roster (:1070-1074); and when a ledger entry is dated after `budgetDay` (:1064). An
  authored-roster restore already drops departed residents' accounts into the treasury and mints newcomers' purses
  (:986-1016).
- `budgetDay` is the society's day clock, not economy state. `decide` rolls the day with it: herb patch regrowth,
  `wagesToday` and `needsBodies` (:560-577). Ledger entries carry it (:344), and `World` refuses a save whose
  society day is in the future (`RatwWorld.cpp:6388`).
- `readSociety` (`RatwWire.cpp:1294-1372`) requires exactly 12 top-level fields: `enabled`, `minted`, `sunk`,
  `nextEntry`, `budgetDay`, `exportsRemaining`, `importsRemaining`, `herbPatch`, `decisionRemainder`, `accounts`,
  `residents` and `ledger` (:1304-1331). `careers` and `loop` are optional; five old fields are read past.
- Town treasuries are `stores:<community>` accounts: `Society::treasuryOf` returns one when it exists and falls back
  to `treasury` (:1200-1206). Festivals (purse and feast meal, `RatwFestivals.cpp:218, 607`), projects, letters,
  boards, camps and stalls use them (14 files). The DM's money view reads them (`tools/dungeon_master.py:1011`), and
  `recoverHoard` pays into `town->store` (`RatwRoads.cpp:1093`). A second `treasuryOf` in
  `RatwGameEstates.cpp:18` returns `chapter:<id>`: a Chapter's treasury, real player money.
- `facilityAccount` (`RatwCareers.cpp:153`) knows 22 prefixes: `stores:`, `caravan:`, `bandits:`, `contract:`,
  `ground:`, `chapter:`, `home:`, `town:`, `house:`, `till:`, `fund:`, `bank:`, `letter:`, `let:`, `stall:`,
  `table:`, `fest:`, `project:`, `outside:`, `farm:`, `crew:`, `estate:`.
- `bandits:<camp>` holds real player coin, taken in `beaten` and `payBandits` (`RatwRoads.cpp:1442-1446,
  1550-1552`), mixed with caravan coin (:711-713). `clearCamp` pays it all to the player who breaks the camp
  (:1474-1480).
- The welcome is taken from the treasury's cash (`RatwSociety.cpp:368-382`). Festival purses clamp to the treasury's
  cash (`RatwFestivals.cpp:216`).
- `Society::create` writes a ledger entry and a journal entry (`RatwSociety.cpp:531, 341-349`). Each journal entry
  becomes a WorldEvent "economy" (`RatwWorld.cpp:3466-3479`), which `bondsFromEvent` reads (:2991-3032) and which
  marks open promises between the two as kept (:3010-3018). The Mind's `describe_event` turns a 0-coin "economy"
  event into "X gave Y N item" (`tools/npc_mind.py:609-613`).
- 260 `conserved()` calls in 37 files (a grep over `Tests`, `Core` and `tools`).

Careers and farms:
- `postClass` (`RatwCareers.cpp:420-458`) starts from `payOf` (`RatwCityLoop.cpp:115-169`) and reads the keeper
  words from `city_loop.json` `wages` (:446). Delete the city loop naively and class goes, and succession by class
  stops filling posts empty for 8 days.
- `staffFarms` (`Core/RatwFarmPeople.cpp:125`) loops over `farms_` and skips any farm whose `farm:` till or `crew:`
  account is missing or whose area is off (:323-330). `farms_`, `farmWorkplace_` and the area switch come from
  `buildFarms` and `refreshAreas` (`Core/RatwFarms.cpp:26-128`). `farmOfPost` (`RatwFarmPeople.cpp:58`) reads
  `farms_`, and both `payOf` (`RatwCityLoop.cpp:124`) and `postClass` (`RatwCareers.cpp:428-432`) call it.
- `items::producerFor` sets a post's skill level (`RatwCareers.cpp:542`), the head's learning rate
  (`RatwResidents.cpp:764`), work-place reservations (:791) and players' farm activities (`RatwTogether.cpp:493, 522,
  574`). Only the store room's call goes.
- Arrivals' purses come from `WharfFund` in `RatwCareers.cpp:387-396, 414-416` and `RatwFarmPeople.cpp:527`.

The roads:
- `sendCaravan` (`RatwRoads.cpp:826`) has no callers. All 7 communities in `Data/Economy/communities.json` are
  `"town": false`, so `setupTowns` (:112-258) finds one settlement and `tendRoads` returns at once (:453), before
  `absorbJournal` and `roadsDaily`. So caravans, bandit camps, bounties, escorts, NPC contract-taking
  (`residentsTakeWork`) and NPC courier letters (:1101-1128) are all off in Ridgemere 2.0.
- `towns_` is also law (`lawTown`, `RatwCrime.cpp:80-89`) and is read in 9 other files.
- Community sizes (homes plus flats): Coldwater 14, Larkfield 11, Frosthollow 9, Southmeadow, Fenshore and Oakhaven
  6, Saltwell 4. Ridgemere has about 600 households.

NPC life and talk:
- The talk facts (`Data/Talk/facts.json`): `wealth` is destitute, poor, comfortable or rich (:12); `business` is
  thriving, steady, failing or none (:16); `on_dole` yes or no (:14); `topic.item` only herbs or meal (:97).
  `RatwGameTalk.cpp:115-162` works `wealth` out from days of food in the household, scanning every resident for each
  speaker. `on_dole` is hard-coded "no" and `business` "none" (:139).
- `robbed` already exists (theft in the last 3 days, `RatwGameTalk.cpp:175-181`), and so does `wealth_days` (:129),
  which goes with `foodDays`. `errand=goods` comes from the tasks "fetching goods" and "delivering goods" (:211-212);
  "shopping for the household" maps to `working=yes` (:184-186).
- `households()` (`RatwCityLoop.cpp:890-921`) leaves out residents with an empty `homeCell`, so the homeless are in
  no household today. Unplaced NPCs are not residents.
- A resident needs `NourishADay` (`RatwSociety.h:828`) a day. Hunger grows .0035 a game second
  (`RatwResidents.cpp:421`); an NPC eats from its wallet at hunger 60 (:510-516, :804-818). Today the only food in a
  wallet is what the city loop's household shopping buys.
- `steal()` (`RatwCrime.cpp:286-357`) takes from the victim's own purse: a maker-marked prize 1 time in 4, then coins
  (1-5p), then a meal, then herbs (:310-338). `chooseMarks` (:743-776) picks a merchant whose till holds meals, but
  the theft still lands on the keeper's purse. Its 40% gate (`roll(id,7)%10>=4`, :753) is fixed per NPC, not daily.
- Residents join staked tables when their purse is at least 30p and at least 20× the stake
  (`RatwGameTables.cpp:342-357`).
- Talk counts, by a plain grep of `Data/Talk`: about 480 lines test `wealth`, about 108 test `business`, 16 mention
  `price_cause`, about 18 `on_dole`, about 190 `prices`, and 11 caravan rules have nothing to feed them.

Sims, hosts and data:
- `--fast` is `--unseen-step 30 --straight --coarse-ticks` (`Tests/world_check.cpp:41, 1802`). `unseenStep_` is the
  game's own offstage step (`RatwSociety.h:674-678, 1479`), and `sim_fast_tests.cpp:261-265` calls
  `Society::setUnseenStep`. These stay.
- `city_loop.json` holds `seasons`, `services`, `wharf`, `shelves` (`startingStock`, `RatwSociety.cpp:1684-1691`,
  which `shelfOf` calls), `wages`, `luxuries`, `food`, `houseBusinesses` (`RatwItems.cpp:802`), `patrons`, `estates`
  and `markets`.
- `houses.json` and `specialties.json` no longer exist; b431ab1 deleted them.
- Doc 78 makes `Data/` read-only in a hosted world's image (doc 78 :1463). Host edits live in the database.
- The perf gate's window (`--simulate 6 8`) misses shop hours, the 18:00 pay step and the day's turn.

## 3. The believability contract

The player never sees a ledger. They see what a shop has and charges; who is in the street and what they are doing;
what comes and goes on the roads and at the pier; what NPCs say; what the notice boards say; and whether all of that
agrees. Each row is something a player can see or hear, what makes it, and what it must agree with. **These rows are
the acceptance list for every phase.**

| # | The player sees or hears | Made by | Must agree with |
|---|---|---|---|
| T1 | Bread costs more in Ridgemere after a crowd of players bought it: "12 for 13p" becomes "12 for 15p", with ▲ | Board `P[ridgemere][bread]` → `quote`, shown per dozen | the keeper's "bread's dear"; `world.bread_price=up` only once the shown price moved |
| T2 | A shop sold out of bread by noon, or a thin shelf | the shelf target, refilled at opening | the client's stock count; the poor it serves go short that day |
| T3 | Selling 30 iron ore at a smithy pays less per piece, then levels; "no room for more today" past the allowance | the per-piece ladder; the per-holder cap; the take allowance | next morning bars and nails a little cheaper, the smith "working the new ore"; the next wagon out carries ore; the next market hears of it |
| T4 | A wagon driven by a named carter leaves the Ridgemere gate with a guard and arrives at a community's market | a committed, posted departure (the `road:<id>` wagon), its driver a real carter away from his post | the notice board listed it; the destination's shelves of its cargo refill on arrival; "Tam's on the Coldwater run" |
| T5 | A caravan was raided: a bounty notice, bandit talk, the carter back robbed or hurt, salt dear where it was going | the existing raid path, plus a Board nudge at the raid | the price rises at the raid, never before; the salt is at the camp for whoever clears it |
| T6 | Ship day at Grayrock: the Wharf's posts busy "unloading", the timetable on the board, imports cheaper after | a committed ship timetable | `world.ships`; the imports' P falls when the ship docks |
| T7 | Hungry beggars by the chapel steps in hard times, almost none in a boom | the band mix × slider; the begging task | `wealth=destitute`, `hungry=yes`, the Mind's "Means" |
| T8 | Petty theft at a food stall, more in hard times, never a crime wave | an expected theft count per market, capped | `world.crime`; the Watch's pursuit, unchanged |
| T9 | A merchant says business is thriving or failing | the `business` fact from sales against the shelf, P on their wares and Times | their prices and stock; never "can't complain" in the same breath |
| T10 | The rich buy luxuries and the poor complain of prices, at any slider position | band floors | the existing `wealth` lines; village rich boast of land and mills, not Houses |
| T11 | A good or bad harvest in an area: farm carts heavy or light, grain cheap or dear | the area's harvest each season | `world.harvest`, cart loads, the staples' P |
| T12 | Farm, works and Wharf people at their posts, apprentices learning | Careers, unchanged | succession; births one for one with deaths |
| T13 | A staged event with its notice: a House wedding buying up wine, a fire at a works | the staged events layer | the notice or crier; the price move; talk |
| T14 | An imported good goes thin between wagons, then refills when one comes in | regional restock | the timetable; "the wagon's due"; the price |
| T15 | A beggar a player helped doesn't beg the next day, and thanks them later | the lifted state | `pair.owed` lines |
| T16 | A slider move has a cause people talk about: a war levy, a silver strike | the slider's cause | `world.times_cause`, a crier, the Mind's Means |
| T17 | The price an NPC names is the price on the list | talk reads the shown price | the client's list |

**The timing rule:** every Board change caused by a visible event lands at the moment a player could see it (the
wagon loads or arrives, the ship docks, the raid happens, the smith works the ore), never on a hidden pass. **Every
ripple between markets has a visible cause:** there is no silent leak along the roads.

**The standing-still rule:** in a cell a player occupies, or the next one, nothing visible changes without a visible
actor. Stock doesn't tick down in an empty shop; a staged sale needs a buyer in the room.

## 4. The shape

```
   the slider + its cause (smoothed)           player trade (buy / sell at a till)
             │                                            │
    ┌────────┼──────────────────┐                         ▼
    ▼        ▼                  ▼                ┌──────────────────┐
 band     Times per market   harvest per area ──▶│  The Market Board │◀── caravans, carts, ships,
 shares      │  (no player      │                │  P[market][good] │    staged events (all visible)
    │        │   pressure)      │                └────────┬─────────┘
    ▼        │                  │                         │
 households ranked ◀            │       ┌─────────────────┼─────────────────────┐
 and cut into bands             │       ▼                 ▼                     ▼
    │                           │   quote (prices,    shelf targets,       talk facts, notices,
    ▼                           │   ladder, caps)     regional restock     the Prosperity tab
 food (from the shelf), purse,  └──────────────────────────────────────────▶ wagon and cart loads
 theft, begging, talk, Means
```

Four small staged systems, all reading the slider:
1. **Bands** for households (section 6).
2. **The Board** of pressures, Times and harvests (sections 5 and 7).
3. **Staged trade:** committed schedules of visible caravans, carts and ships (section 8).
4. **Staged events and causes** (sections 8.4 and 8.5).

Under them, the account layer stays: player purses and escrow are real; NPC accounts are elastic (section 9).

## 5. Markets and the Board

### 5.1 Markets, split from towns

Today the road layer needs two or more towns, and Ridgemere 2.0 has one. Instead:
- **A market is a trade point,** separate from a town (law, the Watch). A `TradePoint` list sits next to `towns_`.
  It is found at every load from merchants' work cells, by the "thickest merchants" rule at `RatwRoads.cpp:57-103`,
  with a smaller minimum.
- **A market's id is always its community id** (`townIdOf`, `RatwAmbient.cpp:89-95`); Ridgemere's is `ridgemere`.
  Every community with a merchant post is a market, while it stays `plain` for law. Ridgemere is one market (D6).
- **A merchant outside every community** belongs to its nearest market by path, or, with no path, to one "outside"
  market with the depth floor.
- **Endpoints that are not markets:**
  - each staffed farm or works place (the 31 farm places and 11 works in `communities.json`, matched to
    `farm_people.json` posts);
  - the port: any place whose posts carry the Wharf job words (today Grayrock's 58 posts). *(As built, Phase 5 and
    its repair: a port is its market, not an endpoint with a route, since nothing travels to it: D8. It is a market
    with a post of the Wharf's own words, `wharf.posts` in `market.json`; the sea trade's other posts there are the
    port's too. A post is of the market its work room is in, else the nearest by path.)*
- **The road layer moves to markets; law doesn't.** `Contract`, `Caravan` and `BanditCamp` keep their `town`
  fields, now holding market ids. The road-layer uses of `towns_` and `townOfCell_` move to the market list: camps'
  wild cells, `roadRoutes_`, bounty patrols' guards, `believe` and the courier letters' two ends. `lawTown` and the
  other law uses stay on `towns_`, so communities never become law towns. Phase 4 lists each use by grep first.
- **Routes:** for each market, the 3 nearest other markets by path on roads and farm tracks, plus each farm, works
  and port endpoint to its nearest market. Pairs with no path are left out quietly. Routes are found at load and not
  saved.

**Host edits.** Markets, endpoints and routes are found again at each load. Delete the Wharf and there are no ships.
Delete every farm and there are no carts. One market means no wagons between markets, but farm carts still come. A
market with no route at all refills its imports each morning (section 7.7). Nothing refuses to load. This answers
doc 73's open question 5 (wagons on the road).

### 5.2 The Board (saved; small and sparse)

```
struct Board {
  int slider;                                    // the smoothed prosperity, -100..100
  Cause sliderCause;                             // its words, from the DM or a seeded list (8.5)
  map<market, Times> times;                      // hard / lean / ordinary / good / boom, plus a DM nudge and expiry
  map<market, map<baseGood, Cell>> cell;         // only non-zero entries
  map<area, Harvest> harvest;                    // this season: poor / fair / good, plus a DM override and expiry
  map<till, int> budgetLeft;                     // the shop's buying budget left today
  map<till, map<baseGood, int>> takenToday;      // the take allowance, reset at dawn
  map<market, map<holder, map<baseGood, Push>>> pushed;  // per account holder, decaying like P; never packaged
  map<market, map<baseGood, int>> netToday;      // the day's net player position, for the morning's ripples
  vector<Departure> committed;                   // wagons posted on boards, with their causes
  vector<Ship> ships;                            // the ports' timetables (Phase 5): each port's ships a week ahead
  vector<StagedEvent> events;                    // live staged events (8.4)
  deque<Ripple> recent;                          // the last ~50 notable moves, for price_cause and the DM's log
};
struct Cell { float P; double touched; std::string cause; };  // cause: players, dump, caravan, raid, ship, harvest, event
```

- On load, unknown market or good ids are dropped and values are clamped.
- P is derived state. "Reset prices" eases every P to 0 over a game day, logged as "fresh stock everywhere", which is
  safe after any edit.
- `recent` holds cause words, never player names, so the world package can ship it (section 13).
- `committed` holds only what a notice has promised: the next two days of wagons. The next week of ships is beside
  it in `ships` (as built, Phase 5): a ship has no route, wagon or carter to commit, and a day, an hour and a cargo
  that a departure has not, so the two are saved, journalled and restored together and kept as two lists.

### 5.3 Times (per market)

`timesScore(m) = slider + harvest of m's areas + raids and late wagons in 7 days + staged events + DM nudge`

- **No player-caused pressure goes in.** Times sets band shares and shop budgets, so players must not be able to push
  a market toward boom by dumping staples, or toward hard times by buying them.
- The score is cut into five words: hard, lean, ordinary, good, boom.
- Times feeds the band mix at that market (section 6.3), the staples' price factor (section 7.1, if D2 says so),
  wagon frequency, the talk facts `world.times` and `world.times_cause`, and the DM's Prosperity tab.
- Each market keeps its own: the realm can be in ordinary times while one community is lean because its salt wagon
  was robbed twice.
- *(As built, Phase 7: `Society::timesScore` is the slider, the roads' and harvests' shift of the morning, the market's
  staged events on (their `times` points, at most ±20 together) and the DM's nudge while it holds (±100, with its words
  and expiry: `World::nudgeTimes`, Phase 9's lever). `Society::timesCause` is the largest part on the score's side in
  words: the slider's cause, "a good harvest" or "a poor harvest" (the Board's new `harvestShift`), "the wagons robbed on
  the road", an event's words, the nudge's; "" when the score is 0. Because the bands read the Times at their weekly cut,
  an event's few points can tip a market's band mix in the week it is on: 5.3's formula was kept over 8.4's "never touch
  bands", for the user to overrule.)* *(Open, the user's decision (the Phase 7 check, its item 4): through the score an
  event's Times points move everything the market's Times band moves, not only the bands' weekly cut: the staples' price
  factor (`marketBase`), the day's expected thefts and the shops' buying budgets. Left as built by the repair; the choice
  is between keeping it (events are part of the Times, as 5.3 has it) and counting events only in the talk and the cause,
  never in the score.)*

## 6. NPC economic status (bands)

### 6.1 Four bands, with class as the second axis

| Band | `wealth` fact | Who, typically |
|---|---|---|
| destitute | destitute | no earner in the home, a lost post or a death in the family; beggars; the homeless |
| poor | poor | labourers and hands in lean homes; the Sump; households of apprentices |
| comfortable | comfortable | most posted households: hands, carters, Wharf men, keepers, masters, officials |
| rich | rich | Houses' heads and kin; owners of large businesses; estates; the Heights' well-off |

- These are exactly the `wealth` words (`facts.json:12`), so the existing `wealth` lines, `pair.richer`,
  `tools/voice_tables.py` and the Mind keep working with no content change.
- **Class stays as a second axis.** Class is station, from the post; the band is fortune, from rank and the slider. A
  poor upper-class widow and a rich lower-class carter can both happen, and both are good talk. `class` becomes a
  talk fact (doc 62 lists it; `facts.json` lacks it).
- **Option (D1):** a fifth status, "working", between poor and comfortable, which reads as "comfortable" to the
  existing lines. Its shares are in section 6.3.

### 6.2 Households

- Households come from `households()` (`RatwCityLoop.cpp:890-921`), which moves into Society. A resident with an
  empty `homeCell` is now a household of one, so the homeless are counted and fed. Farm crews are residents in homes
  and are households like any other. Unplaced NPCs are not residents and have no band.
- `Society::statusOf(id)` returns `{band, class, score, override, liftedUntil}`. It is worked out once a day,
  single-threaded, at the day's turn, and cached in `ResidentRecord` the way `homeClass_` is
  (`RatwSociety.h:1626-1628`). The threaded half of deciding (`RatwResidents.cpp:414-636`) only reads it.
- **Saved:** each household's `{band, since, liftedUntil}`. Bands are worked out at restore from the saved state, not
  at the next day's turn, so a restart never makes bands jump.

### 6.3 Score, rank and the cut

**Score, per household:**
- **Class base:** upper 2, middle 1, lower 0.
- **Home:** a house or grounds +0.5, a cottage 0, a flat −0.2, a room −0.5.
- **Earners:** no earner in the home −1.5; one earner for four or more mouths −0.5.
- **Fortune:** a stable lot for the home (`lotOf`/`mix`, `RatwCareers.cpp:38-56`) in [−0.5, 0.5], plus bounded noise of
  ±0.3 worked out from the home and the week number and eased between weeks. Families rise and fall over seasons,
  nothing is saved, and the noise can never pile up.
- **District,** by label when the label exists: the Heights +0.5, the Sump −0.5. A label a host removed adds 0.
- **Events in the last 28 days,** read from the WorldEvent memory that exists: bereaved −0.5, an earner lost a post
  −1, robbed −0.2, a business failing three days running −0.3, helped by a player +0.4 to +1 (section 6.6), earning
  from players +0.3 to +1 (companion and Hold wages, lodging rent, repairs paid to the household; section 6.6).
- **The market's Times:** hard −0.6, lean −0.3, good +0.3, boom +0.6.

**The pool.** Households with a guarantee, a DM override or a lifted state sit outside the ranked pool. The shares
apply to the rest, so lifting one household never pushes another down.

**The cut.** Households in the pool are sorted by score inside each market (Ridgemere is one market, so the Sump and
the Heights sort against each other) and cut by the slider's shares (section 6.4), with largest-remainder rounding.
- **Destitute needs a reason:** no earner in the home, an earner's lost post or a death in the family in the last 28
  days, or no home. A household with no reason never takes a destitute place; that place goes to poor.
- **The minimum is realm-wide:** at least one destitute and one rich household for every 10 in the realm (unless
  Never Broke clears destitute), placed where the scores are lowest and highest. A 14-household village is not forced
  to keep a beggar at +100.
- **A band stays near its home.** A household's band is at most one away from what its class and home imply (a rich
  family in a room, a destitute one in a manor, never), except after a recent event or a DM override.

**Guarantees, in order of precedence** (the first that applies wins; each applies only if its subject exists;
today's overrides at `RatwGameTalk.cpp:147-162`, made the rule):
1. a DM override, until its expiry;
2. the homeless (an empty `homeCell`, or the beggar job) are destitute;
3. a House's head and kin are rich;
4. the Sump is at most poor;
5. the Heights and estate residents (not servants) are at least comfortable.

*(Repaired, doc 79 Phase 6, **for the user's nod**: one of the Watch, a guard by its post or by what it was written as
(DEV's patrol guards placed with no work spot load with no post; `ResidentSpec::watch`), is an earner, paid by its town,
and a household with one of the Watch in it is at least comfortable, under any guarantee but the DM's override. Before,
DEV build 70's 16 postless guards, each a household of one at the Watch House, scored as households without an earner
(slider 0: 5 destitute, 8 poor; −100: 9 destitute, 4 poor), and at −100 four posted ones were poor too; now all 30 are
comfortable at −100 and 0 (two rich at +100).)*

**Hysteresis.** A household changes at most one band a week, and only when its rank is more than 2 percentiles past
a band's edge. The slider the bands read is smoothed: it moves toward the DM's setting by at most 10 points a game
day, so a full swing takes about 20 days (D17). The DM's "now" skips both the smoothing and the hysteresis.

**Class without the city loop.** `payOf`'s kind detection (`RatwCityLoop.cpp:115-169`: merchant → keeper; the single
shop in a work room → hand; guard → watch; then the public, estate and farm words) moves into `RatwCareers.cpp` as
`postKind(pos)`. `farmOfPost` keeps reading the kept farm list (section 8.2). The keeper words move from
`city_loop.json` `wages` into the class word lists (`RatwCareers.cpp:67-91`). `postClass` reads `postKind`. This
lands in Phase 0, while the city loop is still there, with a test that succession still fills a post emptied for 8
days.

### 6.4 Slider to shares

Percent of households in the pool, interpolated in straight lines between the rows. Defaults live in
`Data/Economy/status.json`; a host's changes live in `live.economy.rules` (section 12).

| Prosperity | Destitute | Poor | Comfortable | Rich |
|---|---|---|---|---|
| −100 hard times | 18 | 45 | 32 | 5 |
| 0 ordinary | 5 | 25 | 60 | 10 |
| +100 boom | 2 | 12 | 70 | 16 |

- A market's Times shifts its own column by up to ±25 points of slider, so a lean community shows more poverty than
  the city at the same slider.
- **Never Broke:** the destitute share is moved into poor.
- **The loader checks the table.** Each row must sum to 100 (otherwise it is normalised). Each share must be at least
  `minShare`, 2%, a constant. Destitute must not rise and rich must not fall as the slider rises. A table that fails
  falls back to the default with a warning; it is never applied half-valid.
- Because the cut is by rank, the shares hold on any world size. A host's moved homes or deleted families simply
  re-rank the next day.

If D1 takes five statuses:

| Prosperity | Destitute | Poor | Working | Comfortable | Rich |
|---|---|---|---|---|---|
| −100 | 14 | 30 | 36 | 15 | 5 |
| 0 | 5 | 18 | 44 | 26 | 7 |
| +100 | 2 | 8 | 42 | 37 | 11 |

### 6.5 What each band does

One row of `status.json` each. Every field has a clamp in the loader, and missing fields take these defaults. The
numbers are first guesses; tuning waits for Phase 10 (memory `implement-before-balancing`).

| | Destitute | Poor | Comfortable | Rich |
|---|---|---|---|---|
| Food a day, a head | 1 meal | 2 meals | 3 meals | 3, fine quality |
| Chance of a short day at 0 (× 1.5 at −100, × 0.5 at +100) | 45% | 12% | 1% | 0 |
| Purse target (clamped to at most 500p) | 0-3p | 3-12p | 12-60p | 60-300p |
| A keeper's buying budget × | 0.4 | 0.7 | 1.0 | 1.5 |
| Services and luxuries | none | a barber rarely | services by `every`; luxuries 10% a week | luxuries 40% a week; the tailor, the jeweller |
| Theft weight | 1.0 | 0.35 | 0.05 | 0 (a sly one 0.02) |
| What a pickpocket finds | 0-1p | 1-3p | 2-8p | 5-25p |
| Begs | yes, if no post | no (asks kin) | no; gives alms on some Restdays | no; gives alms |
| Talk tags | hunger, cold, the dole | prices, thin wages, debts | the work, the town, "can't complain" | by class: Houses, politics, boasts (upper); land, mills, the harvest (middle and lower) |

How each row is applied:

- **Food comes from the shelf.** At the household's market hour, the household's food is a staged sale from its
  nearest food keeper: the till's stock of the staple drops by the provision, and the same count of meals (fish at
  the Wharf, bread in the city, cheese for the rich) is added to each member's wallet, **up to** the band's two-day
  target, never beyond. It is a raw stock change: no ledger entry, no journal record, no WorldEvent.
  - **A short day** gives half a day's food: half the band's meals, and never more than half of what a wolf eats in a
    day (Review A: the poor's two meals are more than a day's need, so half of them was still a full day). The roll is `hash(household, day) < chance`, so a replay gives the same days on one
    thread or four. The chance for poor and destitute households rises with the staple's P at their market (up to
    ×2 at +0.5), and with a raid or a poor harvest. A bad week shows as hungry poor people, not a starving city.
  - **An empty shelf:** if the keeper's staple is out at the household's hour (players bought it out), poor and
    destitute households served by that keeper have a short day. Comfortable and rich ones are fed from elsewhere.
  - **Buying ahead:** the day before a Restday or a festival, staged sales and errand-runners at food shops run
    `buyAheadDays` heavier (the visible queue before a feast).
  - Hunger growth and eating stay as they are. NPCs still visibly stop to eat, `hungry` is real hunger, and the theft
    gate's hunger test still means something.
  - **Never Hungry:** every short-day chance is 0 and an empty shelf costs nobody a meal.
  - This replaces `householdsShop` (`RatwCityLoop.cpp:923-1449`).
- **Purse.** Each night every resident purse moves halfway toward a target in its band's range (a stable point from
  the home's lot, plus a little daily jitter). It moves down as well as up, so coins players paid in drift away (a
  sink) and a purse never grows into something worth farming. The move is a raw set: **no ledger entry, no journal
  record, no mint.**
  - **Holds:** a theft victim, a festival winner, a gift's recipient or a household earning from players keeps the
    change until its `holdUntil` (3 days for a one-off, 14 days after the last steady player payment), then drifts
    back. `holdUntil` is saved per account.
  - **Lean days:** a resident robbed in the last 3 days isn't refilled, and a second theft from them pays nothing. The
    existing `robbed` fact already says so.
- **Goods in staged wallets are trimmed nightly:** food above two days' worth goes, and other goods above what the
  band would hold. Dole bread, alms, restitution and inheritance can't pile up into a store for thieves.
- **Luxuries and services.** Staged sales from the till: luxury goods leave the jeweller's shelf as the rich "buy",
  so the jeweller's stock moves and `business` can read it. Only where a luxury shop exists; in a community, the rich
  household's wants become a wagon's manifest line instead (section 8.1). Services keep the 22-entry `services`
  list, at its frequency × band.
  *(As built, Phase 6 and its repair: besides its food a household buys its band's `shops` pieces a day, a mean
  (destitute 0, poor 0.3, comfortable 1, rich 2), of its market's everyday wares: the foods besides the bands' staples
  (meat, greens, fruit, preserves) and the goods that aren't food, those its band can afford (a piece at most the top of
  its purse: the poor's 12p, never a steel sword), each picked by its shelf's target, so a shop's custom goes with what it
  sets out. The luxuries (`market.json`'s, cheese) are luxuries first, though the rich also eat cheese: the comfortable
  10% a week, the rich 40%. The poor's services are only those `market.json` marks `poor` (the barber's).)*
- **Jobs.** The band never changes posts. Posts are authored, NPCs are never made, births stay one for one with
  deaths. A born child takes the household's band at once. An arrival placed in a post joins that household. An
  apprentice is scored with their household. An adult who loses a post takes the −1 event and may slip a band in a
  week. A child's 5p from the parent and an arrival's purse become plain amounts from the band's range: no
  `WharfFund`, no minted float.
- **Inheritance.** Between NPCs, `Society::bequeath` (`RatwCareers.cpp:159`) moves goods only (trimmed that night);
  the coins are staged and drop. The heir takes the bereaved −0.5; the inheritance itself is ignored by the score.
- **Homes.** No evictions. Homelessness stays authored. Doc 59 (lamps and homes) is rewritten later as cosmetic only
  (D13).

### 6.6 Bonds without NPC shopping

Household shopping fed keeper-customer familiarity through the ledger. Instead:
- **Offstage:** once a day, each household's food provision applies `bondsFromEvent`'s familiarity step directly
  between one member and the keeper. There is no WorldEvent, so the event store doesn't fill, no promise is marked
  kept, and the Mind never remembers a provision as a gift.
- **Near players (a rule, on by default, D21):** the household's errand-runner walks to the nearest open shop that
  sells the good, stands at the counter for the 8 seconds the bites already use, and the till's stock drops by the
  real items. The task is "fetching goods", so the existing `errand=goods` fact follows. Only where a player is in
  the cell or the next one; at most 2 errand-runners in a player-occupied cell and 12 in a market.
- An errand-runner who finds the shelf empty goes home without, and that household has a short day if it is poor or
  destitute.
- *(As built, Phase 6: only to an open shop, its keeper keeping it; a member free to go, one without a post first; one
  that hasn't reached the counter in a game hour gives up. With no runner to send (the caps, nobody free, the shop shut,
  the rule off), the household's food waits for the next hour; still watched at 17:00, it is fed from elsewhere and the
  shelf is left as it stands, a bare one still a short day for the poor. Buys besides food at a watched shop wait the
  same way and go undone at the shutting: no rich buyer or servant is shown yet.)*
  *(Repaired: with its own food shop shut, the runner goes to another open one of its market with the food on its
  shelf, the same cell's first; one that gives up past the shops' last hour leaves its household fed from elsewhere at
  once (the comfortable always fed). A buy besides food at a watched shop sends one of the household's own to the
  counter (7.7's "rich buyer, servant": a rich household's member), under the same caps; with nobody free it waits, and
  goes undone at the shutting.)*

### 6.7 Players and bands

- A player's gift (`RatwGameGive.cpp:90`) or "help for a household" (`RatwGameTroubles.cpp:312`) to a poor or
  destitute household gives it a **lifted state** at once: no begging, no short days and no hunger tags for a week's
  help (a week's food or more) or a month's. The household leaves the ranked pool while lifted. Its score also gains
  +0.4 or +1, fading over 28 days, so it may move up a band for real.
- At most half a market's destitute share can be lifted at once, so a rich player can't empty a city of beggars
  overnight. `fed_household` fame counts at most once per account holder per household per week.
- The "short" trouble (`RatwTroubles.cpp:170-200`) fires from the band (a poor or destitute head of household)
  instead of `foodDays`. Its coins are a fortnight of the band's food at the market's quoted price, at least 10p
  (`RatwTroubles.cpp:190-196`). The help and refill check in `RatwGameTroubles.cpp:463` reads the band too. Solving
  it lifts the household for its length.
  (Review A brought the band's gate forward: since slice 1 only a poor or destitute head is "short"; the days, the
  coins and the help check are still `foodDays`' until Phase 6 does the rest.)
  *(As built, Phase 6: the coins are `refillDays` (a fortnight) of the band's food a head at the market's quoted price,
  each head's day never more than a day's need (the poor's two meals are more, Review A), at least 10p; a gift solves it
  when worth those coins; a lifted household isn't short. A week's food is `bandFoodCost(7)`. The one lifted owes the
  helper the help's worth both ways round in their bonds, so the `pair.owed` lines can fire.)*
- Steady pay from players (companion wages, `RatwGameCompanions.cpp:135, 271`; Hold wages; lodging rent; repairs)
  holds the purse (section 6.5) and adds the "earning from players" score event. A guard a Chapter pays 10p a day
  doesn't drift back to poor and complain of thin wages.
- So the beggar a player helped stops begging the next day, and thanks them later (an existing `pair.owed` line).

## 7. Prices: what players pay and get

### 7.1 The price and the shown price

For good g (base good b = `items::baseOf(g)`) at a shop in market m:

```
factor(m, b) = clamp( season(b, m, day) × region(m, b) × district(shop, b) × times(m, b) × exp(P[m][b]),  0.80, 1.28 )
buy lot      = round( Σ over the q pieces of  catalog(g) × factor(P stepped up a piece at a time) ), at least 1p
               (at a stall: × 0.9 and rounded down, at least 1p)
               never under ceil( catalog(g) × 0.80 × 0.9 × q ), the clamp floor's stall price (Phase 1, the repair)
sell lot     = floor( Σ over the q pieces of  catalog(g) × factor(P stepped down a piece at a time) × 0.55 )
               then capped by the backstop (7.6); a lot worth 0p is refused
```

- `catalogPrice` and the quality multipliers (`RatwItems.cpp:955-975`) don't change.
- `season` is the seven season groups (grain, fruit, fish, meat, dairy, honey, preserved), moved from `city_loop.json`
  to `Data/Economy/market.json`. **Fruit's 80-140 (×1.75) is wider than the clamp (×1.60) and is rescaled in Phase 1;**
  grain's 85-115 fits. If seasons become per climate, the market's climate picks the row. (The user, 2026-11-08: each
  group's swing is rescaled to at most `seasonSpanMost` ×1.06, so that at a season's ends, with each region factor,
  players' pressure keeps at least ×1.10 of room up and ×0.90 down inside the clamp; see Progress.)
- `region` is cheaper where a good is made (×0.85 to 1) and dearer far from it (1 to ×1.15). It comes from which
  endpoints make the good: `workshops.json`, the communities' farm and works areas in `communities.json`, and
  `crafts.json` `supplies`, with a harvest bias.
- `district` is a static factor inside Ridgemere: Heights luxuries ×1.05-1.1, Sump goods ×0.95. It sits inside the
  clamp and inside the backstop, so it opens nothing.
- `times` touches only staples and luxuries: hard staples ×1.08, boom luxuries ×1.08, and so on. D2 decides whether
  the slider moves player prices at all.
- **This replaces `Society::shopFactor`** (`RatwCityLoop.cpp:2876`), the one call `quote` makes (:445), outright:
  Phase 0 has already switched the loop off, so there is one price path. `followStock`, `loop.prices`/`loop.sales`
  and the `soldToday_`/`soldOut_` counting in `transfer` (:404-409) go.

**The shown price.** For a good whose catalogue price is under 3p, the list shows the buy lot of a dozen ("12 for
13p"), worked out by the same ladder and the viewer's own cap, so the shown price is what that player would pay
for that lot now. For every other good it shows one piece. The client gets each ware's
`listCount` (1 or 12) and `sellLot`, the smallest quantity whose sell lot is worth at least 1p ("I buy ore by the
half-dozen"). The client's `canTradeItem` (`state.ts:2456-2460`) tests the lot's price, not the unit price, and
allows a 0 unit price. A single 1p loaf still costs 1p; a dozen is where the move shows. Buying single loaves can save
a player a penny or two on a dozen; it is a quirk, not a pump, because it only shrinks a sink. (Without the floor
above it was a pump at a stall: rounded down, a 2p good bought singly cost 1p a piece, under the clamp, and sold on as
a lot for more. With the floor, q pieces bought in any lots cost at least 0.72 × catalogue × q, and a lot of q sells
for at most floor(0.704 × catalogue × q): a penny lost at least, whatever the moment or the place. I3's backstop is
the cheapest of every split of q into lots.)

**The trend marker.** Each row gets ▲ or ▼ when its shown price differs by at least 1p from the price at P = 0, with
the cause word on hover ("travellers buying", "the wagon's late"). The footer reads "Prices follow what's bought and
sold here, and what the roads bring."

### 7.2 Why the clamp is 0.80-1.28 (the arbitrage arithmetic)

A player can buy at a stall at 0.9 × the factor and sell to a shop at 0.55 × the factor. The round trip pays only if

`0.55 × factor(dear) > 0.9 × factor(cheap)`, that is if `factor(dear) / factor(cheap) > 0.9 / 0.55 ≈ 1.64`.

- Without the stall the bound is 1 / 0.55 ≈ 1.82. The stall's discount makes it 1.64.
- **The clamp covers time as well as place.** Buying grain in the cheap season and selling in the dear one is the same
  loop, so the season sits inside the clamp, not outside it.
- 1.28 / 0.80 = 1.60, under 1.64. On factors alone that leaves a margin of about 2%.
- **Rounding and faction discounts can still break it,** so the clamp is not trusted alone:
  - Rounding to the nearest penny made small lots profitable (a 4p good bought at a stall for 2p and sold for 3p).
    Sold lots now round down, and the backstop (I3) is applied to the rounded, floored lots. Every round trip loses
    at least a penny.
  - A sworn Chapter's 25% back makes the lowest buy share 0.9 × 0.75 = 0.675, and the bound about 1.23: well inside
    the clamp. The discount is therefore capped in pennies per Chapter per week (section 7.8). A loop through it can
    win at most the cap, which is the discount itself.

So **hauling goods bought from one shop to sell to another never pays.** What stays fun about trade: selling your own
gathered, hunted or (later) crafted goods where they fetch the most (a real difference of up to 1.6×), selling to
other players at stalls, escorting wagons, recovering raided cargo and robbing bandits. A bounded trader's margin is
D3.

### 7.3 Pressure

- **Range** −0.5 to +0.5 before the clamp.
- **Player trade moves it, at the till.** In `Society::trade`, after a success: buying q pieces adds q / depth,
  selling takes q / depth. Nothing reads what players *hold* (the Ultima Online lesson). Hunting and gathering add
  nothing; selling the result does. Player-to-player stall sales add nothing in v1.
- **A refused or 0p lot moves nothing.** Otherwise dumping worthless lots would be a free way to push prices.
- **Piece by piece.** Inside a lot, each piece is priced at P after the pieces before it. Buying 99 bread costs
  visibly more than 99 × the first loaf; dumping ore pays less for each piece. This replaces the one factor per lot
  at `RatwSociety.cpp:451-456`.
- **Depth.** `depth(m, b) = max(20, depthBase(group of b) × catchment(m)^0.6)`, where the catchment is the market's
  households, plus its farm and works crews, plus 20 for passing trade. First guesses:

  | Group | depthBase |
  |---|---|
  | staple food (bread, flour, grain, fish, cheese) | 2.0 |
  | other food and drink | 1.0 |
  | meat, game, hides and furs | 1.0 |
  | herbs and forage | 0.8 |
  | timber, stone, clay, sand | 1.0 |
  | charcoal, peat, firewood | 1.0 |
  | ores and bars | 1.0 |
  | cloth and wear | 0.5 |
  | tools and arms | 0.4 |
  | luxuries | 0.3 |

  Every catalogue good is mapped to a group in `market.json`, with "other food and drink" or "tools" as the default
  for anything unmapped; a test checks coverage.
- **Worked examples:**
  - **Ridgemere,** catchment about 620: 620^0.6 ≈ 47. Bread's depth is about 95. One player buying 10 loaves adds
    0.105, so the dozen goes from 13p to 14p, ▲ (a dozen's own ladder adds about 6%, so it is 13p at P = 0).
    Twenty players buying reach the clamp: about 15p a dozen.
  - **Coldwater,** 14 households, its crews and passing trade, catchment about 40: 40^0.6 ≈ 9, so every good sits on
    the floor of 20. A player selling hides (5p) to its tannery: the first piece at P = 0, then −0.05 a piece. After
    three hides P is −0.15 and the per-holder cap is reached; three hides pay floor(2.75 + 2.62 + 2.49) = 7p and
    each further hide about 2.37p. The next day P is about −0.14; the next wagon out carries half the glut off
    (section 8.1). The next visitor finds hides a little cheap and hears why.
- **Decay:** `P ← P × 0.5^(days / halfLife)`. Half-life: perishables 2 days, keeping food 5, metals, tools, cloth and
  hides 8. Applied lazily from `touched` when read, plus a nightly sweep that drops entries under 0.005. That keeps it
  exact at any `--speed`.
- **Cause:** each entry keeps the cause of its last big move (players, dump, caravan, raid, ship, harvest, event).
- **Per-holder cap.** One account holder's pushes on one market and good are summed in `pushed`, which decays exactly
  like P. While it is at 0.15 or more, further pieces price at the capped P and add no pressure. A lone player's day
  moves no good in any market past ±0.15, and there is no reset at dawn to game. A second wolf on the same account
  shares the cap.

### 7.4 Ripples

**Through goods, in the same market, the next morning.** A link table built at load from `crafts.json` (125 crafts)
and `recipes.json` (343 recipes): each good's inputs and outputs, two steps at most, with a visited set so cycles are
safe and unknown goods skipped. Uncapped, iron ore would reach 35 goods and charcoal 116, so **each step keeps at most
6 goods,** chosen by how many recipes use the link (ties by catalogue price).
- The day's **net** player position per market and good (`netToday`: pieces sold minus pieces bought, over all
  holders) is what ripples. Buying and selling the same good the same day nets to nothing, so churn moves nothing
  downstream.
- **A net glut ripples downstream:** ore −Δ, iron bars −Δ/2, nails and tools −Δ/4.
- **A net want ripples upstream, weighted by value:** bread +Δ, flour +Δ/2 × min(1, catalog(bread)/catalog(flour)),
  wheat +Δ/4 × the same weight. A want for a cheap good can't lift a dear input more than its worth.
- Ripples land at the next dawn, the same moment for every market (P is the market's, shared by its shops), and the
  smith or the baker is seen working the new stock at the morning restock ("Setting out the wares"), even where no
  shelf needed filling (Review A): `price_cause` "cheap iron since the ore glut". A ripple names the cause only where
  it leaves P on its own side; one that only eats into a move the other way leaves that move's cause. They count against each holder's cap in proportion to their share of the net, and
  ripple-caused |P| on any good is capped at 0.15.
- At most 13 updates a good, once a morning. A host's new recipe joins the table at the next load. It is never a
  production chain (the X4 lesson).

**Between markets, only through visible wagons and ships.**
- **At departure** (visible as "loading the wagon"): for each good on the manifest, the source's P moves halfway
  toward 0 if it is below 0 (the glut leaves on the wagon). Player-sold surplus is the first line of the next
  manifest out, so T3's ripple is a wagon a player can watch leave.
- **At arrival:** `ΔP_dest = −0.2 × max(0, P_dest − P_source at departure)` for each good on the manifest, never
  positive. A wagon that brings salt never makes salt dearer.
- **A raid** adds +0.15 for its manifest goods at the destination, at the moment of the raid.
- There is no daily leak along the roads. Wagons are scheduled toward the biggest gaps (section 8.1), so **the visible
  wagon is the correction.**

### 7.5 What a shop pays players

- **The 1p-a-piece floor goes for sales.** A sold lot is the ladder's sum × 0.55 rounded down once per lot. Selling 99
  ore at P = 0 would pay 54p, not 99p (in practice the per-holder cap flattens the ladder first). A lot worth 0p is
  refused in words: "Not worth the counting. I buy those by the half-dozen."
- **The sale moves goods and coins separately.** `trade` moves the goods with a goods-only `transfer` at price 0 and
  the coins with one `shift` of the lot's total. `transfer` keeps its `price < 1` refusal for every other caller.
- **The shop's buying budget.** `budget(shop) = 40p × keeper's band × slider (×0.8 to ×1.2)`. The keeper's band here
  is worked out without player pressure. It is set at dawn, drawn down only by what the shop buys from players, and
  saved as `budgetLeft`, so a restart doesn't refill it. **Player purchases never raise it,** so a second character
  can't pump a till's buying power. Past it: "I've spent all I'll spend today." A lot worth more than the whole day's
  budget: "That's more than I lay out in a day." (Review A: at dawn, "spent all" was untrue.) The client shows "BUYING TODAY · N p"
  (`Client/src/ui/hud/dialogs.ts:1320`; the heading at :1312 drops "REAL PURSES").
- **The till always has the cash.** When a player sells within the budget, the till's cash is topped up to the lot
  first (a raw set). "The trader cannot afford that" stops happening.
- **The take allowance,** per shop per day, counted against what it took today: gathered and hunted raw goods at
  least 30 pieces (or depth × 0.5 if more), made goods at least 6, a wearable or sword 2. Past it: "I've no room for
  more today." This replaces the 40 / 4 / 24 / 20 caps (:462-470).
- **A daily sell total per account holder** across the world: a rule, default 400p, clamped to 0-5,000, logged. Past
  it: "No one here is buying more from you today."
- **Goods move on.** A till's stock above its shelf target leaves on the next wagon or cart out of that market. With
  no departure, it keeps until one goes.

### 7.6 Rules no setting can break

Enforced inside `quote`, whatever the DM's rules say, and property-tested (section 16):
- **I1. Price.** A bought lot costs at least 1p; every factor is finite (NaN or infinity becomes 1).
- **I2. One shop.** A sold lot pays at most 0.6 × the bought lot for the same good, quantity, shop and moment, on the
  rounded and floored lots. `SellBackShare` 0.55 is a constant, never a rule; the ladder only widens the gap.
- **I3. Everywhere.** The whole factor, across every market and every day, stays inside 0.80-1.28. As a backstop
  beneath it: **a sold lot never pays more than floor(0.95 × the cheapest bought lot of that good and quantity
  anywhere in the world now),** stall-discounted. It is worked out live at quote time over every market (about 10-30),
  never from a daily snapshot. For a good no shop sells (iron ore today), the cheapest lot is
  `floor(catalog × 0.80 × 0.9 × q)`.
- **I4. Qualities.** Crude, fine and masterwork share their base good's P, so they can't be played against each
  other.
- **I5. Crafting, for whoever builds it** (doc 35's Craft panel, unbuilt): an output's sell price may be at most 1.6 ×
  its inputs' buy price (a labour premium). A recipe that breaks this sells at the cap.

The clamp's ends (0.80, 1.28), `SellBackShare`, `minShare` and I1-I5 are constants, not DM levers.

### 7.7 Shelves and restock

- **Target:** `shelf(g) × abundance(m, g) × (1 − 0.5 × max(P, 0))`. `shelf` comes from `shelfOf`
  (`RatwCityLoop.cpp:508-527`) and the `shelves` section of `city_loop.json` (`startingStock`,
  `RatwSociety.cpp:1684-1691`), both moved to `RatwMarket`. `abundance` comes from the season, the harvest and recent
  arrivals. Targets cover the day's staged food provision as well as players.
- **Regional restock:**
  - **Goods made in the market** (its workshops, its areas, the farms whose carts come to it) refill toward the target
    each morning, when the keeper opens (the existing trigger, `RatwResidents.cpp:769-771`). The keeper is "setting
    out the wares" for a few minutes, visibly. No inputs, no money, no store room.
  - **Goods the market doesn't make** refill only when a wagon, cart or ship brings them. Between arrivals they come
    from a small back-room buffer that runs down by a fifth a day. "The wagon's late", thin shelves and dearer prices
    then agree (T4, T5, T14). *(As built, Phase 5's repair: at a port, a ship brings `market.json`'s `wharf.imports`
    that its shops set out and its workshops don't make, and whatever else its shops set out that nothing in the realm
    makes and no wagon or cart brings; `wharf.never` goods never. What a Country farm makes stays the carts'.)*
  - A market with no route and no port refills everything each morning, so a host's lonely town never runs bare.
- **When a wagon with that good arrives:** the till jumps toward the target, and the porters and keeper are
  "unloading" (T4).
- **Staged sales:** the day's food provision (section 6.5), luxuries and services take stock down. They run only when
  no player is in the cell or the next one. Near players, each staged sale needs a visible buyer: an errand-runner, a
  rich buyer, a servant. A ware players bought out stays thin until morning or the next wagon (T2).

### 7.8 Other hooks

- `worth()` in crime (`RatwCrime.cpp:42-48`) uses the quoted price. Today only meal, herbs and sword are worth
  anything; every other stolen good counts 0 toward fines and restitution.
- **Faction terms** (`RatwGameFactions.cpp:467-492`): the surcharge is unchanged. The 15% or 25% back stays coins but
  is capped per Chapter per week (a rule, `factionDiscountMost`, default 30p) and logged as a faucet "faction
  discount".
- Repairs (`RatwDurability.cpp:159-196`) and the maker's scent don't change.
- The keeper's trade answer (`RatwGameVoice.cpp:138-156`) lists the shop's real wares at their shown prices, and the
  dearest or cheapest with its cause.

## 8. Staged trade: caravans, farm carts, ships, events

None of it conserves anything. Cargo is a list of goods with counts, for show. Arrivals are Board nudges and shelf
refills.

### 8.1 Caravans between markets

**Reused:** RatwRoads' `Caravan` (`RatwRoads.h:20-41`), `sendCaravan`, `tendCaravan`, `caravanEntered` and
`caravanArrived` (`RatwRoads.cpp:528-882`). The wagon walks where players are near and hops while unseen (a measured
cost). Escorts, letters by carter, raids, bandit camps (:1216-1595), bounties, rumours, NPC contract-taking, NPC
courier letters and the watchpost and waystation projects (`RatwGameProjects.cpp:703, 716`) all run again.

**Changed:**
- `Caravan.account` and the `caravan:<id>` load give way to `std::vector<std::pair<std::string, int>> manifest`. On
  load, an old `account` field is ignored and the wagon comes home empty.
- The loading and unloading between `stores:` accounts (:631-842) goes. `stores:<community>` stays, as the staged
  town treasury (D24); `recoverHoard` pays into `treasuryOf(market)`.
- **Bandits:** a raid on a staged caravan puts no coins in the camp. Its manifest becomes goods at the camp, made with
  `create` (a goods faucet, capped at about 40p of goods a camp). Coins robbed from players stay real, in
  `bandits:<camp>` (section 9.1).
- **A dropped caravan** (its ends no longer exist) is removed on load. Its letters (`c.letters`, courier contracts
  taken by `caravan:<id>`, `RatwRoads.cpp:458, 791, 871`) reopen at the origin, or go back to the writer if the
  origin is gone too. Contracts whose `town` no longer exists refund their poster.

**Schedule:**
- **Committed when posted.** Two days ahead, each route rolls `hash(route, d) < frequency` for day d. A departure
  that passes is posted on both ends' boards and saved in `committed`, with its hour, size and manifest. Frequency's
  inputs are read once, at posting. After that only an announced cause may change it: a raid, a storm, "Overdue".
- **Frequency** scales with the size of both ends: city to a community about every 2-4 days by the community's size,
  between communities every 6-10 days, mostly through the city. × 0.6 at −100 to × 1.4 at +100, × the season, × up
  to 1.5 when the biggest P gap on the route is over 0.2, × the DM's `caravanRate`.
- **Drivers are real carters.** Each departure takes a carter from the market's Carters' Yard or the community's
  carters, who is away from his post for the trip: his task is "on the Coldwater run", his schedule paused until he
  is home. Wagons a day are capped by carters available, and wagons in transit by a rule (default 16). A host who
  deletes the carters gets fewer wagons.
- **Late,** with a seeded cause people talk about: a broken axle, mud, the weather.
- **After a raid** the carter comes home robbed or hurt, never killed off-screen; deaths drive births, so off-screen
  deaths would distort the population.
- **Manifest:** up to 5 goods, each one some shop at the destination stocks or buys. First the source's player-sold
  surplus, then what the source makes (its workshops, its areas' goods), then the goods with the largest
  `P_destination − P_source`, then a rich household's want ("silk sent up from Ridgemere for the miller's wife").

**What players see:**
- The carter loading the wagon at the yard.
- A notice on both ends' boards (kind `notice`, poster "the carters", expired by `RatwBoards.cpp:356-376`), with
  the calendar's weekday names (Dawnday … Restday): "Wagon to Coldwater, Emberday: salt and nails for the general
  store, cloth. Escort wanted." An escort contract posts when a camp is active on the route.
- The caravan and bandit topics (`RatwAmbient.cpp:525-556`) gain `topic.what`, the manifest's first good, and the
  driver's name.
- An arriving wagon carries news: `world.news:<source>`, the source market's top cause ("ore's cheap in Ridgemere").

**Bandits** never scale with players' wealth (the RimWorld complaint). Camps' spawn and strength take a mild slider
and Times factor instead (D14). Camp strength and bounty value are capped by rules, since strength grows with each
raid (`RatwRoads.cpp:715`).

### 8.2 Farm carts

- **The farm list stays.** `FarmNow` (def, kind, area, keepers, hands, outdoors), `farmWorkplace_`, `countFarmPosts`
  and `farmOfPost` stay in `RatwFarms.cpp`, read from `country_farms.json`'s farm defs. The area switch is always on.
  `staffFarms` (`RatwFarmPeople.cpp:125`) loses its `farm:`/`crew:` account checks (:323-330). Farm posts keep
  filling after the cut, and a test says so.
- **Every morning** each staffed farm or works place sends a small cart to its nearest market, heavier on Marketday.
  The carter is the farm's keeper or a hand: a real NPC at a real post. It walks the farm track when seen.
- **Harvest:** one word per area per season (poor, fair, good) from slider × season × a seeded weather hash, or the
  DM's override with an expiry (which posts a notice). It sets the cart's load and the chance a cart comes: good
  100%, fair 70%, poor 40%.
- **Arrival:** the market's shelves of the area's goods refill. Their P is pulled toward the harvest's baseline (good
  −0.15, fair 0, poor +0.15): halfway, and only downward for the cart's goods. Between carts P decays toward 0, so a
  good harvest reads as cheap but steady, and a missed cart as a visible bump.
- **Marketday stalls:** NPC stalls are stocked from that morning's farm carts, a visible tie.
- **Talk:** `world.harvest:<area>`; the crew's own talk follows ("best barley in years").

### 8.3 Ships (only where there is a port)

- **A port** is a place whose posts carry the Wharf job words: Grayrock today.
- **Timetable:** 1-3 ships a week by slider and season (fewer in winter storms), seeded by week and port, or
  `shipsPerWeek` set by the DM (0 means none). Committed and saved a week ahead, when the timetable is pinned to the
  Wharf office's board. A name from a Tracery list (the *Gull*, the *Saltwife*). Imports drawn from the 12 `wharf`
  imports and exports from the 13 takes, kept as a short flavour list in `market.json`.
  *(As built, Phase 5 and its repair; each the user's to overrule. The rule is the middle of the week's count, not the
  count: one more or fewer by the seeded score, so 1-3 at the default 2 and 5-6 at 6, never more than a week's six
  working days, and none on Restday. The names are a list in `market.json` a host edits, checked by the loader for
  the forbidden word, not a Tracery grammar: a ship's name is saved with the ship, never made up by a speaker. The
  notice is on the port's town board, signed "the Wharf office": this game has one board a community.)*
- **Ship day:** the Wharf posts' task becomes "unloading the *Gull*" and the cell is a crowd (`here.crowd`). The
  imports' P is pulled toward a ship baseline (−0.1) in the city; the exports' P rises ("the *Gull* took all the
  hides"). The shops that sell the imports refill.
- **A storm-cancelled ship** raises the imports' P by 0.1, is posted ("No ship this week") and talked of.
- **A ship at the pier** is optional, about 4 hours of client work (D8).
- **No port:** no timetable and no ship talk. Imports come by road from a map-edge "outside" endpoint if the host
  marks one; otherwise the market refills them each morning.

### 8.4 Staged events

On a small world player trade rarely moves anything, so the world needs its own visible happenings.
- **A seeded scheduler** posts 1-3 events per market a week (× the DM's `eventRate`, 0-3), from a list in
  `market.json`: a House wedding buying up wine; a fire at a works; a good catch; blight in an orchard; a festival's
  buy-ahead; a lame ox delaying the Coldwater cart; a ship's rich cargo.
- **Each has a visible cause:** a notice, a crier, a crowd, a talk fact (`world.event`). Each pushes P toward a target
  for a few days, inside the clamp, with its own cause word.
- The DM sees the week's events on the Prosperity tab and can cancel or add one.
- Events never touch money, bands or posts.
- *(As built, Phase 7, `Core/RatwEvents.cpp`; each the user's to overrule. The list is `market.json`'s `events`: twelve
  kinds (a House wedding, a fire at the works, a good catch, blight in the orchards, the festival's buying ahead, a lame
  ox, a ship's rich cargo, a ship lost, the mill wheel stopped, the drovers in, the salt pans flooded, a brewing too
  many), each with its words, notice, crier's line and sign, its goods' targets (within ±0.3), its days (1-7), Times
  points, days posted ahead, hour, weight and needs: "city" (the realm's largest market), "port", "cart" (a staffed
  farm's cart, held home that day by the lame ox), "festival" (the day before one), a ship it comes in with or loses. A
  week has `perWeek` (2) one more or fewer by a seeded score, × the rule `eventRate` (percent, 0-300; new), so 1-3 at the
  default; a kind once a week at a market, one event a day, only goods its shops sell or buy, the days a festival or a
  ship needs kept clear. Rolled a week ahead and kept on the Board (`events`, `eventsThrough`, `nextEvent`), as the ships
  are, so saved, journalled whole and restored with it; its words filled as it is rolled. As it begins its goods are
  pressed halfway toward their targets (the Board's `setPressure`, cause "event"), again each morning while on, a good
  catch's set out where a shop sets them out; its notice is pinned ahead (`ahead` days) or as it begins, its crier's line
  cried at the square in the daytime (the festival criers' machinery; the market's official, innkeeper or priest), a
  crowd at its place where it says, and talk's `world.event`, `world.event_words` and `world.event_when` (now, soon,
  past). A ship lost is a timetable line of its own (`Ship::lost`: "is lost at sea", `world.ships` "lost", its cargo a
  storm's +0.1 dearer and the event's more); a rich cargo begins as its ship docks and is called off if a storm keeps it
  out. The DM's hooks for Phase 9: `World::addStagedEvent`, `cancelStagedEvent`, `stagedEvent`, the Board's `events`.)*
  *(Repaired: each kind's words are a noun phrase that reads after "since", after a colon and after "talk of" ("the
  drovers and their herds", "the brewers' glut", "the festival trade", "the lame ox that held up the Longmead Farm
  cart"); a kind and a cause may name the `seasons` they happen in (no good catch or orchard blight in winter, "the hard
  winter" only in autumn and winter, "the rich harvest" only in summer and autumn); a kind's needs and seasons hold for the
  DM's events too; no kind twice at a market within a week; a rich cargo is of its own goods first and its notice says
  "is in" once she is (`noticeOn`); a lost ship's name stays off every timetable a year (the Board's `lostShips`).)*

### 8.5 A cause for every slider move

- When the DM moves the slider, they may write its cause ("the Crown's war levy"). With none, one is picked from a
  seeded list for the direction: a war levy, rot in the granaries, a bad winter; a silver strike, peace, a rich
  harvest.
- The cause becomes `world.times_cause`, a crier's line and a board notice in every market, and part of the Mind's
  Means line.
- The realm-wide harvest roll leans the same way while the slider is moving, so the cause and its effects agree.
- Times nudges and harvest overrides post a visible notice too, by default (a DM rule can turn it off).
- *(As built, Phase 7: `Society::setProsperity(target, now, cause)` takes the DM's words (plain, 80 at most, never the
  forbidden word; else the seeded cause stands in, and it returns false); with none, a move a new way takes one of
  `market.json`'s `events.causes` for its direction (three each way), seeded by the day, and a move on the same way keeps
  the cause it has. The Board keeps it (`sliderCause`: words, notice, cry, way, day, number, told). The world tells every
  market once (a notice "the crier"'s on its board, the cry at its square); the DM's words take the list's templates
  ("Hard times ahead: the Crown's war levy."). The Means line ends "Times here: lean, since the Crown's war levy.". The
  harvest roll leans ±0.08 its way while the slider moves. The rule `overrideNotices` (1, new) posts the DM's nudges and
  harvest overrides (`World::overrideHarvest`, with its words); the levers themselves are Phase 9's.)* *(Repaired: the
  seeded cause is of those its way the season allows, picked by `seeded::lot`; `world.times_cause` is said only where the
  times are other than ordinary; a harvest override's notice reads "A poor season for wheat and oats." (what its farms
  grow, never the area's id) and comes down when it lapses.)*

### 8.6 The arrival table

| Event | Board | Shelves | Notice | Talk |
|---|---|---|---|---|
| A wagon is posted | — | — | posted at both ends two days before | `caravan due` |
| A wagon loads and leaves | the source's glut halved toward 0 for its goods | over-target stock leaves | — | `caravan left`, the driver |
| A wagon arrives | −0.2 of the gap on its goods, never up | its goods refill at the destination | taken down | `caravan arrived` and what; `world.news` |
| A wagon is late (a day past due) | +0.05 a day on its goods at the destination, at most +0.2 | the buffer runs down | "Overdue", with its cause | `late` and why |
| A wagon is raided | +0.15 on its goods at the destination | the buffer only | bounty and escort contracts (`RatwRoads.cpp:700-740`) | `raided`, `bandits`, the carter hurt |
| A farm cart comes in | the area's goods pulled toward the harvest's baseline | refill | — | harvest |
| A ship docks | imports toward −0.1, exports up | the importers refill | timetable updated | `ships` |
| A ship is lost or stormbound | imports +0.1 | — | "No ship this week" | `ships late` |
| A staged event | its goods toward the event's target | as the event says | notice or crier | `world.event` |

## 9. Accounts: what stays real, what is staged

### 9.1 Every account, sorted

`Society::classify(id)` sorts every account by prefix, never by a function name. A test checks that every
`facilityAccount` prefix is in this table.

| Prefix | Kind | Notes |
|---|---|---|
| a player id | **real** | purses, with their limits (64 kinds, 10,000 a stack, `MoneyLimit`) |
| `chapter:` | **real** | Chapter treasuries (the second `treasuryOf`, `RatwGameEstates.cpp:18`) |
| `contract:` | **escrow** | a player-posted contract holds real coins; an NPC-posted one holds staged-sourced coins |
| `letter:` | **escrow** | enclosures; a resident's letter holds staged-sourced coins |
| `let:` | **escrow** | lodging chests |
| `stall:` | **escrow** | a player's stall; an NPC's Marketday stall is staged |
| `table:` | **escrow** | stakes; NPC stakes are staged-sourced |
| `fest:` | **escrow** | festival pots; NPC entries are staged-sourced |
| `project:` | **escrow** | player money in town projects; town money in them is staged-sourced |
| `ground:` | **escrow** | player grounds |
| `bandits:` | **escrow** | coins robbed from players; staged raids add goods, never coins |
| `treasury` | staged | the realm's treasury |
| `stores:` | staged | each community's town treasury (D24) |
| `till:` | staged | shop tills |
| `town:` | staged | the Town Works, watch and church; `town:<t>:granary` reads the market's Times |
| `home:` | staged | residents' larders |
| NPC resident ids | staged | purses |
| `house:` | staged if a kept call site reads it, otherwise dropped | Phase 0's list says which |
| `caravan:`, `fund:`, `bank:`, `outside:`, `farm:`, `crew:`, `estate:` | dead | dropped at restore with their goods; `fund:relief` goes with them |

**Escrow carries its source.** Each escrow account records how many of its coins came from the staged side. When it
pays a player, the staged-sourced part counts as a faucet then, at payout. When it expires back to the staged side,
nothing is counted twice. A festival prize counts as a faucet for the pot's payout minus the winner's own entry.

### 9.2 Staged (elastic, set nightly)

- **Resident purses:** halfway to the band's target each night (section 6.5), except while held.
- **Goods in staged wallets:** trimmed to the band's targets (section 6.5).
- **Tills:** the buying budget at dawn (section 7.5); shelves to target; anything over target leaves on the next
  wagon.
- **The treasury and each `stores:` town treasury:** set each night to between a floor of 200p (`TreasuryFloat`,
  `RatwSociety.h:829`) and a ceiling of 2,000p. Fees, rent, postage, stall rent and fines vanish into it above the
  ceiling.
- **Staged payers are topped up on demand.** When a staged account must pay a player (a reward, a bounty, an escort, a
  festival purse, archive pay, a faction reward), it is topped up to the amount first, logged by the faucet door. The
  call sites that clamp to cash change: `RatwFestivals.cpp:216` and the rest found by grep in Phase 2.
- **The welcome:** 20p, 2 herbs and 3 meals made with `create` (a faucet, "welcome"), once per account holder per
  world per game week. A later wolf on the same account that week starts with 3 meals and no coins. It is no longer
  taken from the treasury or capped by its cash (`RatwSociety.cpp:368-382`).
- **At load,** only staged accounts that are missing or out of range are settled. Saved budgets, holds and lean days
  are kept, so a crash or a host's Restart changes nothing a player could see.

All of these are raw sets with no ledger or journal entries. The journal's account shadow-diff
(`RatwGameJournal.cpp:150-180`) narrows to real and escrow accounts: today it diffs about 16,000 NPC accounts on every
record.

**Why keep NPC accounts at all** (instead of deleting them and rewriting the call sites): the existing `shift(` call
sites expect a payee that exists. Thefts, gifts, festival entries, letter enclosures, lodgings and NPC eating all need
*a* purse. Elastic accounts keep them working with no change at the call site. The cleaner end state is D16.

### 9.3 The faucet door

`classify` runs inside `shift`, `transfer` and player `create`, and sorts every move:
- **staged → player:** a faucet, with a reason ("sale", "reward", "bounty", "theft", "refund", "welcome", ...);
  goods count at their quoted price;
- **player → staged:** a sink, with a reason ("purchase", "rent", "fee", "fine", ...);
- **staged → escrow:** recorded on the escrow as staged-sourced; counted as a faucet when it pays a player;
- **escrow → staged:** a sink for its real part (a project's leftovers to the town works,
  `RatwGameProjects.cpp:465, 474`);
- **player ↔ escrow, player ↔ player:** real, as today;
- **staged → staged:** allowed and uncounted; the nightly set erases it.

Every faucet and sink adds to a small daily tally per account holder and reason. The DM's Prosperity tab shows player
coin made against coin sunk, per day and per holder: the EVE report.

**In play, the door logs and alerts; it never refuses** (D16). An unregistered faucet path is logged with its call
site, so nothing that works today breaks. In tests it is enforced (section 9.4).

**Caps on faucets,** each per account holder, each a rule with a clamp:

| Faucet | Cap |
|---|---|
| Selling to shops | the shop's daily budget; the take allowance; the per-holder P cap; the daily sell total (400p) |
| Rewards (factions, archive, festival prizes, bounties, escorts, story prizes) | each system's fixed amounts, plus a daily cap per holder; bounty value capped by camp-strength rule |
| Faction discount | 30p per Chapter per week |
| NPC-posted courier contracts | 3-7p (`RatwRoads.cpp:1123`), at most 3 a day |
| Theft from staged accounts (`RatwCrime.cpp:329-337`) | the band's take (section 6.5); lean days; at most 20p of coins and goods a day ("nothing worth the risk" past it); warrants as today |
| Staked games against NPCs | net winnings from NPC stakes at most 30p a day; NPCs join only within that |
| Raided cargo at a camp | about 40p of goods a camp |
| Refunds (letters `RatwGameLetters.cpp:588`, lodgings `RatwLodgings.cpp:243`, Chapter rent `RatwGameEstates.cpp:194`, camp materials `RatwGameCamps.cpp:194`) | at most the payment on record |
| The welcome | once per holder per world per game week |

NPC alms and the Restday dole go only to NPC households, never to a player standing at the chapel steps. The bond-debt
fallback in lodgings (`RatwLodgings.cpp:244`) becomes dead code, kept harmless.

### 9.4 `realBalanced()`, the test gate

The 260 `conserved()` calls in 37 files are converted, not stripped:

`realBalanced()`: Σ player cash + Σ escrow's real coins == the opening real total + faucets − sinks, from the tally.

- It is a mechanical rename, done in Phase 3. It keeps the only coverage that catches a duplication bug, which
  players will hunt for, and it makes an unregistered faucet fail CI.
- Coins only in v1; goods are counted in the tally for the report but not in the balance.
- In play it is never checked at restore (lenient on read).

## 10. Street life

### 10.1 Theft: an expected count per market

`chooseMarks` (`RatwCrime.cpp:743-776`) today needs hunger ≥ 70, no meal, cash under 6p and a fixed per-NPC hash.
Instead, once a day per market:
- **Expected thefts:** `E = base × Σ households' theft weight × Times factor`. The day's count is `floor(E)` plus one
  more if `hash(market, day) < frac(E)`, capped at 4 a market. A 4-household hamlet sees a theft every few weeks; the
  city a few a day. `base` is in `status.json`; `theftCap` is a DM rule.
- **Candidates:** adults, never guards, never anyone in custody, not tried today. Weighted by band, by the `honesty`
  trait (frank 0.2, plain 1, guarded 1.5, sly 3; read at `RatwGameTalk.cpp:103-110`) and ×2 if `hungry`. The roll is
  a seeded daily hash.
- **Targets fit the motive:**
  - a hungry thief goes for a food till or stall and **takes food from the till's stock**: a new path in `steal()`,
    since today it always takes from the keeper's own purse;
  - a destitute thief who isn't hungry picks the pocket of a richer resident (`pair.richer`) at a crowded cell, and
    `steal()` takes coins first, as now;
  - NPCs picking players' pockets: D11.
- **Unchanged:** everything after the target is chosen: the approach within 1.6 tiles, the unseen-by-guards check
  (:720-741), witnesses, warrants, gaol and pursuit.
- **When a player picks a staged pocket,** the take follows the victim's band (rich 5-25p), within the per-holder
  daily cap.
- **Fines and restitution** use `worth()` from the quoted price. Restitution to an NPC is a plain purse change; a fine
  to the treasury is a sink.
- *(As built, Phase 6: `base` is `status.json`'s `street.theftBase`, 0.03, and the Times factor `street.theftTimes`,
  ×1.5 hard to ×0.7 boom; a thief's hour is seeded between 8:00 and 20:00, and one not on its mark by 21:00 lets the day
  go; a hungry thief (hunger 55 or more, nothing to eat) goes for a keeper of its market whose till holds food and takes
  one or two of its cheapest food; any other for someone of a higher band in its market, not of its household, where
  most people stand. A lifted household has no thief. Residents in no band, a world without
  the staged households, keep the old rule.)*
  *(Repaired: "hungry" is hunger 55 or more and less than a day's food in hand (a household's two days run down by a
  short day; a crust is no meal); a food till is a mark only while its keeper keeps the shop, between the shops' hours,
  and steal() takes from it only then. A household lifted after the day's draw steals no more that day. Never one of the
  Watch: a guard by its post or by what it was written as (DEV's guards placed with no work spot load as civilians with
  no post; `ResidentSpec::watch`). The day's plan waits for the society's staged households, so a fresh world's first day
  has its thieves. Restitution of a till's food still in the thief's paws goes back on the till's shelf (the incident
  keeps the till, `Incident::from`); its worth in coin, as before, to the keeper.)*

### 10.2 Begging, rough sleeping, the dole

- **Begging (a new task):** residents of a household in want of food, adults without a post or a master and not
  lifted, on Marketday and Restday by day, at the market or the chapel steps (a spot finder like `boardSpot`,
  `RatwBoards.cpp:19`), at most 2 at one place, each place 8 tiles or more from the next, and at most a number a cell
  that follows the conditions (below). A bark when a player is near. Players give through `RatwGameGive`. Comfortable
  and rich NPCs passing sometimes give 1p: a visible `shift`, and the beggar's purse moves.
- **The user's decision (2026-11-11), verbatim:** "I think up to 5 beggars per cell is good, but only in the worst
  conditions. Beggars should be in the NPC list, typically someone from the lower caste that doesn't have money for
  food. Old people living with family can be an option but not the only choice." As built:
  - **The most in a cell** (`status.json` `street.beggarsInACell`, straight lines between the points): **5 at −100,
    3.5 at −50, 2 at 0, 1 at +50, 0.5 at +100**, never rising as the slider rises; a market's Times moves its slider by
    `timesShift` (25) as it moves the bands' column (hard −1, lean −0.5, good +0.5, boom +1 of it), and a fraction is one
    more on that share of the cells, by a lot of the cell and the day. Since a market's Times follow the slider, an
    ordinary market gives **5, 3.9, 2, 0.9 and 0.5** at −100, −50, 0, +50, +100: 5 only at the worst (−100, or near it
    in a hard market), about 0-1 in a boom. The rule of 2 at one place, 8 tiles apart, stands under it
    (`beggarsACell`).
  - **Who begs:** a resident of the NPC list in a household in want of food (`Society::foodWant`): the destitute always;
    the poor when its purses and the food its members carry beyond today's would not buy a day's food a head
    (`Society::NourishADay`, in its own food) at its market's price, judged on the morning's purses and larders at the
    staged day's turn (today's food is in hand: the staged ration keeps every household near two days' of it, 6.5). An
    adult (16 or over) with no post and no master; never one of the Watch, never a lifted household, never one with the
    money for food.
  - **The draw:** each market's beggars are drawn each day by a seeded lot weighted by want (destitute 1 to 2, poor 0 to
    1: the share of a day's food it can't pay for) times its class's weight (`beggarsClass`: lower 1, middle 0.5, upper
    0.25: typically the lower class); each group (an elder past its retiring age or of working age, by home kind: house,
    cottage, flat, room, communal lodging or none) keeps `beggarsSpread` (0.5) of its weight for each one drawn from it
    already; and while anyone else in want waits the elders are at most `beggarsElders` (half) of a market's beggars, at
    least one: an elder living with family begs only when the household is in want, an option and never the only
    choice.
- **Rough sleeping:** `homeless=yes` stays authored. Destitute residents in communal lodging are "sleeping by the warm
  wall" in a sheltered cell some winter nights (D12).
- **The dole:** on Restday the chapel's priest, a real post, hands bread to destitute and poor households near the
  chapel: a raw stock add into their wallets, visible, with no church account, trimmed with the rest. It sets
  `on_dole` and wakes the `on_dole=yes` lines.
- *(As built, Phase 6: the chapel steps are where the chapel's first door to the open air comes out, the market's spot
  its notice board's; begging is shown as the existing `job=beggar` barks, a beggar near a player calling first every
  other look; a passer-by's alms are a penny by its band's `alms` chance, once a day a pair. Rough sleepers: a third of
  winter nights, by a seeded lot, the destitute with no home or a communal one, by a bakehouse's or a forge's wall in
  their market (indoors or by its fire), else its chapel's. The dole: the hour after the service, by the first clergy
  post held in a community, to the destitute and poor households of that community, `street.doleDays` (a day) of bread
  a head, once a Restday a household.)*
  *(Repaired: beggars also stand before the market's shops (a stall's side in the open, a shop's doorway on the street),
  at most `beggarsACell` at one place, each place `World::BeggarApart` (8) tiles or more from the next in its cell (a
  DEV city cell is a quarter of the city: two a whole cell held the city's beggars to four), in a seeded order of the day,
  never one of the Watch; so they go with the band mix (on DEV build 70, Marketday: 9 at −100, 6 at 0, 2 at +100). A
  rough sleeper lies in the shelter of the warm building's doorway on the street (by its fire where it works in the open),
  never inside, at most `beggarsACell` to a wall and side by side, the chapel's doorway when every wall is taken. The
  dole is seen: with a player near the chapel steps one of each household comes and waits in a queue (at most
  `World::DoleQueue`, 6, at once), handed its bread at the front (`World::DoleSeconds`, 4); out of sight it is given at
  once; whoever is left at the dole's end is given it then. A passer-by's penny is a staged move between two purses (no
  ledger, no journal).)*
  *(Repaired again: the beggars at a place, the sleepers by a wall and the dole's queue stand side by side, each on a
  tile of its own (`World::freeBeside`: ground someone could stand on, stepped to from the place's own, so DEV's raised
  streets at 1.5-2.5 work as level ones do), none on the priest's tile (the chapel door's mouth) or a beggar's; at most
  `beggarsACell` at one place (status.json says so now; the key keeps its name). Never one of the Watch, posted or not:
  its household is comfortable or better in the bands (6.3's repaired note), and it never begs, sleeps by a warm wall,
  takes or queues for the dole, even under the DM's override. After a restart past the dole's hours on its Restday (a
  crash before the next checkpoint took the bread given), the households unserved are given it then, once.)*

### 10.3 Business

`business` (hard-coded "none" at `RatwGameTalk.cpp:139`) is set for keepers from staged and player sales against the
shelf, with P and Times as tie-breaks:
- **thriving:** the shelf sells down faster than its target (a queue, an empty shelf by noon), or players bought more
  at their till in 3 days than the market's median, or Times is good or boom;
- **failing:** the shelf barely moves, or the mean P of their wares is under −0.15 (a glut), or Times is hard and their
  wares are luxuries;
- otherwise **steady**.
- Three days of failing add a −0.3 score event to the keeper's household.
- A salience rule: `business=failing` suppresses the comfortable band's "can't complain" tags.

It wakes about 108 lines. The keeper whose ore players dumped says business is slow, which agrees with T3.

*(As built, Phase 6: worked out at the day's turn from the day before. "Faster than its target" is a day's sales of
half its shelves' targets or more, or a ware sold out before noon; "barely moves" is two days' sales under a twentieth
of its targets. Its three days of players' buying are against the middle of its market's shops'. "none" until its
first day's end. Sales count the staged buyers' pieces (food, wares, luxuries), services done, and players' purchases.)*

*(Repaired: the yardstick is each shop's ordinary trade (`Society::ordinaryTrade`): the pieces a day its market's
households would buy from it at the slider's middle (`status.json`'s shares at 0) by the same rates and picks, services
included. Thriving: a ware sold out before noon, a day's sales of its whole shelves over its ordinary trade, a day past
chance above its ordinary one and half as much again (E + 1.645√E + 1 and 1.5E), or players' three days over its
market's middle; failing: a glut on its wares, or, once two whole days are known and where two ordinary days are 3
pieces or more, two days' sales past chance below two ordinary days and under half of them; or hard Times for a luxury
shop (luxuries half its shelves); Times good or boom makes a day above its ordinary one (E + √E) thriving; else steady.
A household's staple food from its food shop moves the shelf but isn't counted (it is what it eats, not trade), and a
service moves no shelf and never "sells out before noon". The failing event is added once for a run (and again each
`eventDays` it lasts), not each night. In tests of a market of eleven shops and 120 households: at 0, 2% failing, 4%
thriving; at −100, 15% failing; at +100, 42% thriving.)*

## 11. Talk and the Mind

The rule: **the price an NPC complains about is the price the client shows.** Talk calls the same function.

**Person facts:** `wealth` (from the band), `class`, `business`, `on_dole`, `hungry` (real), `homeless`, `robbed`
(exists), and `status` only if D1 takes five. `wealth_days` goes with `foodDays`.

**World facts:**
- `world.times` and `world.times_cause`;
- `world.<good>_price` (up, down, steady) for the market's three largest moves **in shown price**: a good counts as up
  or down only when its shown price differs from the price at P = 0 by 1p or more; otherwise it is steady.
  `world.meal_price` and `world.herbs_price` stay;
- `world.harvest:<area>`, `world.ships`, `world.ship_name`, `world.trade` (in, late, raided), `world.news:<source>`,
  `world.event`.

**Cause and direction** come from what actually moved the clamped factor (season, harvest, players, wagon, event).
When the clamp swallowed the players' push in a dear season, talk says "it's winter", not "travellers buying".

**Scene topics.** `prices` and `price_cause` come back in `sceneTopics` (`RatwAmbient.cpp:500-656`; they were deleted
in b431ab1), with `topic.item` the good, `topic.dir`, and `topic.price` the shown price ("twelve for fourteen pence").
Cause words: "travellers buying", "someone sold a heap", "the wagon's late", "the *Gull* came in", "a good harvest",
"the wedding up at the Heights". `topic.item` in `facts.json` widens from herbs and meal to text: the existing
`topic.item=herbs` lines keep matching, and new generic lines use `#topic.item#`.

**Class-gated tags:** rich upper-class households talk of Houses and politics; rich middle and lower ones of land,
mills and the harvest.

**The Mind** (`dialogueContext`, `RatwGame.cpp:4499-4505`): the raw purse and the old herbs-and-meals stock line go.
A `means` field, at most 300 characters, takes their place: "Means: poor; gets by week to week; pay about eight
pence a day. Times in Coldwater: lean, since the Crown's war levy. Salt dear, 4p, since the wagon was robbed." It
carries the shown price of the top-cause good and the post's pay word, so the Mind doesn't invent figures. It goes
into `OPTIONAL_LIMITS` in `tools/npc_mind.py` (:50-52), beside `trouble`, with one RULES sentence. Tests stay offline.

**Pay words** come from one table per post tier, × slider. Where a player really pays someone in that kind of post
(companion and Hold wages), the word matches that wage, so talk and the purse agree.

**New content,** about 450-650 Tracery and salience lines (doc 62): band × Times complaints and boasts, generic price
lines, caravans with their cargo and driver, ships, harvests, staged events, slider causes, beggars' barks and the
dole. Ship names and every new list pass the check for the forbidden word.

**Performance:** talk reads the cached band instead of scanning every resident for each speaker
(`RatwGameTalk.cpp:121-127`). `talkRichLine`, `richLine_`/`poorLine_` and `troubles::foodDays` as a measure of wealth
go.

*(As built, Phase 8; each the user's to overrule. **The shown price:** `Society::priceMove(till, item, holder)` reads the
same two ladders as the shop list's row (the lot of a dozen for a good under 3p, the holder's own push, a stall's 0.9), so
`topic.price` is the list's price, in words: "seven pence", "a penny", "fourteen pence the dozen" (said that way, not
"twelve for fourteen pence", because it fits every sentence the older lines put a price in). A move exists only where
the shown price differs from the price at rest by a penny; `Society::priceMoves(market, holder, 3, nearer)` gives a
market's three largest, each read off the shop the speaker would quote (its own, its cell's, else the first by id), for
the overhearing traveller's holder (`World::talkHolder_`, set while the ambient picks are made). `world.<good>_price`
(up, down, steady) is set for those three and for meals and herbs; `facts.json` lists 34 goods, and the Game drops a
price fact for any other. **Cause:** `topic.cause` (players, dump, ripple, caravan, late, raid, harvest, ship,
ship_takes, ship_late, ship_lost, event, still) and `topic.cause_words` ("travellers buying", "someone sold a heap",
"the wagon's late", "the Gull came in", an event's words); a Board cause that runs against the move is "still" ("though
the wagon came in"), so no line blames it. Season is never a cause: it isn't P. The older price lines that named a cause
are gated on it. **`topic.item`** is text: "herbs" and "meal" keep their old lines; every other good is its plain
words, plural where counted by the piece (`World::cargoWords`), with `topic.what` and `topic.is` ("hides are").
**`price_cause`** comes back for a traveller's own buying or selling (the holder with the largest push the move's way,
named as the teller knows them); its older scenes are the "down" ones. **New topics:** `talk.ships`, `talk.times` (only
in Times that aren't ordinary), `talk.event`, `talk.harvest`, and the caravan topic's `due` (posted or on the road here,
`topic.when`) and `news` kinds, with `topic.why` for a late wagon's mishap. **Salience:** `settings.json`'s `salience`
(engine: `Library::muted`): words a speaker never says while its facts hold; a failing shop never "can't complain" (nor
"couldn't be better", "run off my feet", "sold out"...), the destitute never boast of plenty. **The Means** ends with the
market's largest move: "Salt dear, fourteen pence the dozen: the wagon's late." **The keeper's answer** (7.8) adds the
ware whose shown price moved most with its cause ("Nails are dear just now: the wagon's late."), and is never replayed
"as I said" (the list may have moved). **Bands:** a resident in a band reads only its band; the food-days scan and
`wealth_days` remain for one in no band (a demo roster), and `pair.richer` within a band compares the households' scores.
The "short" trouble says itself by band (`troubles.json`'s `saidBy`). `market.json`'s `houseBusinesses` and `estates`
are not read: the `house` fact and the bands' guarantees cover a House's people, so Phase 9 may drop them.)*

*(Phase 8 repaired; each the user's to overrule. **The listener's own list:** each ambient pick is made for one player
(`AmbientPick::hearer`), and `World::talkFor` sets whose list talk's prices are read at (the trade menu's holder, the
Game's `accountOf`) while the director picks, while the Game writes the pick (`exchangeQuery`) and for a bark near a
player; `talkPrices` is worked out once a look for a shop and a cell. **The cause is what moved P:** a traveller's trade
takes the Board cell's cause only where P was at rest or crossed over, or the travellers' pushes together are half of P
or more on its side (one traveller's cap, 0.15, never takes over a late wagon's 0.25; two do); a ripple likewise only
where it is half of P. **The topic's good:** `topic.good` (its id), `topic.sort` (food, drink, goods: a line about
eating is gated on food), and each speaker's `deals` (their shop sells it: a line of "my shelf" is gated on it);
`topic.ship` names the ship a ship's cause or ship topic is of, and each ship at the port is its own topic (one lost is
talked of while another is in). `world.trade` is called `world.wagons` (in, late, raided, no): `trade` is a person's
fact. `lifted` (a speaker living on a player's help) gates the helped beggar's greeting. **Scores:** the market's news
(prices, events, ships, wagons, Times, harvest, the town's lives) is drawn once an hour for a place, not a pair, within
about 1.35-2.1, so whichever is live takes its turn (five days of hours with an event, hard Times, nails dear and a
wagon's news: prices about 30%, the event 24%, the Times 16%, the wagon 15%); the harvest is talked of in summer and
autumn, and a poor one into the winter. **Words:** talk says "bread", "fish", "porridge", "white bread", "meat",
"candles" for the catalogue's stiffer names; an event's lines say a good is dear or cheap only beside its
`world.<good>_price`, and every kind has lines for when it's over. The Means names the speaker's own shop's price at the
player's list and is cut at a sentence. **At the merge with Phase 7's repair:** an event's words are a noun phrase, so a
price that runs against its event is "cheap still, despite the brewers' glut" (the list, talk's `topic.cause_words`, the
Means), never "though"; `world.event_goods` is the event's goods as a line says them ("brandy, wine and chamomile"), so a
rich cargo is spoken of by what its market deals in (silk only beside `world.silk_price`), and a ship in with a rich
cargo is talked of with its first good.)*

## 12. DM levers: keep, change or cut

All of them stay per world in `live.economy` (the `rules` jsonb) and reach a running server through `economy.sync`
(`RatwGame.cpp:944-956`, kept). **Defaults live in `Data/`, which is read-only in a hosted world's image (doc 78).
A host's changes to `status.json` or `market.json` values live in `live.economy.rules`,** are checked by the same
loader and travel with the world package.

| Lever (today) | Verdict | Why |
|---|---|---|
| `steerLean` 0-200 (shared ↔ held by the rich) | **Change:** a new rule `prosperity`, −100 hard to +100 boom, steps of 5, with an optional cause text; same storage, audit `economy.steer` and route `/api/economy/steer` (`tools/studio.py:640-686`; the route went with the steering in Phase 3, since nothing read a slider then, and Phase 9 brings it back carrying `prosperity`) | The user's one control. The axis changes from distribution to prosperity. The old value is **not** mapped across: old worlds open at 0. Out-of-range or non-numbers are clamped, never refused |
| `steer` on/off | **Cut** | The slider is always in force; 0 is ordinary times |
| `neverHungry` | **Change:** a clamp, no band has short days | Gentle servers want it; no machinery behind it |
| `neverBroke` | **Change:** a clamp, the destitute band is empty | No begging or need-theft |
| The 11 steering levers, measures and targets, skim, tithe, dividend, charters, supervisor, weekly ledger, `steerMeanDays`, `skim*` | **Cut** | Nothing is conserved, so nothing is steered or audited |
| Edicts | **Change:** "word on the street", read-only: the Board's top causes, harvests, ships, events, Times and the slider's cause | The same data talk reads, so the panel and the talk agree |
| Year log | **Change:** slider moves with their causes, overrides and big ripples (a pressure move over 0.2) | Hosts can see why bread is dear |
| `needsPaused` | **Keep** | Maintenance and testing |
| Store room: `internalStore`, `storeMarkup`, `storeResale`, `resaleMarkup`, `storeRule`, per-business `nostore` (`live.business_settings`) | **Cut** | Restocking is free |
| `wharf`, `wharfReserve`, the 11 wharf rules, `wharf.credit` | **Cut;** add `shipsPerWeek` (0 means no ships even with a port) | Ships are a timetable |
| `wharfOff` goods | **Change:** `neverImported`, a goods list | Kept out of ships' manifests |
| `gateToll`, `carriage`, subsidies | **Cut** | NPC-only plumbing |
| `wageKeeper`, `wageHand`, `wagePublic`, `wageWharf` | **Change:** a pay word per post tier, × slider, for talk only ("eight pennies a day") | NPCs talk about pay; nothing pays them |
| `seasons` | **Keep,** moved to `market.json`: price and abundance by group, inside the clamp | The best part of looking real |
| `comfortsAbove`, `comfortsShare`; luxuries (7); alms and treasury (10); trades (12); land rent (7); `farmSurplus`; `shopRot`; wage drift (6); treasury drift (8) | **Cut** | The band sets buying; the treasuries are elastic. `buyAheadDays` stays as a constant (section 6.5) |
| `stockPrices`, `stockPriceBand`, `stockPriceStep`, `shelfFollow`, `shelfDays`, `shelfCover`, `shelfGrow` | **Change** into the Board's rules: `halfLife.<group>` (0.5-30 days), `ripple` (0-0.7), `depthScale` (0.1-10), `playerPushMost` (0-0.5), `playerSellMost` (0-5,000p), `takeAllowance` (0.05-1), `shelfScale` | Where "players buying bread raises its price" is tuned. The clamp's ends are constants |
| `services` and its 22-entry list | **Keep** (frequency by band); cut `servicesCityShare` | Visible purchases |
| Farms panel: schedule, wages, `crewScale`, `priceFactor` | **Cut** | Crews are real NPCs at posts |
| Farms panel: per-area `on`, `yieldScale` | **Change:** a harvest bias, and a per-area harvest override (poor, fair, good) with a season's expiry; areas are always on | A story lever: "blight at the orchards" |
| `economy_transfer` (the Storykeeper; `RatwDirector.cpp:222, 381-388`; `dm_service.py:575`) | **Change:** "grant or fine a player" (a faucet or sink with a reason and a cap), and on an NPC target a band override with an expiry | It must not need a funded source; a story can still make Bram destitute |
| `operatorTransfer` | **Keep** for real and escrow accounts | Stories |
| Money tab (`MoneyTab.tsx`; `dungeon_master.py:986-1049`) | **Change** into a **Prosperity** tab: bands by market, the hungry, would-be thieves and beggars today, Times and causes, the price board (the largest pressures, with causes), wagons, ships and staged events this week, player coin made against sunk | The one read model |
| Drift view (`studio.py:508-552`) | **Cut** | |
| NpcTab "Paid by the town treasury" (`NpcTab.tsx:392`) | **Keep,** relabelled "A paid post (for succession)" | `p.paid` feeds succession rank (`RatwCareers.cpp:49-52, 894`) |
| **New** | a Times nudge per market with an expiry ("boom at the port"); "reset prices" (eased over a day); `caravanRate` (0-3 ×); `eventRate` (0-3 ×); `theftCap`; the faucet caps (theft, stakes, faction discount, rewards, welcome); `wagonsMost`; a band override per household with an expiry; override notices on or off; `status.json`'s shares and behaviour table (edited in Atlas into `live.economy.rules`) | DM-tweakable, each with a clamp in `RULE_LIMITS` (`tools/map_editor.py`) |
| `SellBackShare`, the clamp's ends, `minShare`, I1-I5 | **Not levers** | Anything a lever touched could open a money loop |

## 13. The world package (doc 77)

In `tools/world_package.py`:
- **Remove:** the `minted − sunk` equality in `money()` (:718-741, called from `scan()` :790); "money conserved into
  the treasury" in `strip_society` (:537-582, docstrings :9-11 and :35-41); the cash-moving part of `pour` (:522-534).
- **Strip players by deletion.** Player accounts and player escrow (`chapter:`, `table:`, `stall:`, the `let:` of
  stripped lodgings, player-posted `contract:`, `letter:…@wolf-`, the player coins in `bandits:`) are deleted with
  their goods, not poured into the treasury. A resident's letter escrow still goes back to the writer.
- **Drop staged accounts.** They are derived; the importing server makes them at load. That also shrinks packages.
- **Reset pressure** in a shipped world (`board.cell` to {}, `pushed`, `netToday` and `budgetLeft` dropped), so a new
  world starts at normal prices. `recent` holds only cause words; the scan checks it holds no player id.
- **Scrub** the faucet tallies (they hold account ids) and every hold, lifted state and committed departure that
  names a player.
- **Keep:** a treasury present (a warning if missing, since restore makes it), no negative cash, the goods limits,
  every player-id prune and scrub, the maker-mark removal (:567-574), the whole-snapshot id scan, `SECTIONS` and
  `KNOWN_SAFE` (:589-634).
- **`live.economy`** shipping (:918, :1030, :1244) follows the slim table and carries the host's rule overrides.
- `tools/dm_service.py:293-303` drops `minted` and `sunk`.

## 14. Saves and existing worlds

**The checkpoint** (`readSociety`, `RatwWire.cpp:1294-1372`):
- The strict "exactly 12 fields" becomes "required: `enabled`, `accounts`, `residents`, `ledger`, `nextEntry`,
  `budgetDay`, `herbPatch`, `decisionRemainder`; `careers` optional; anything else read past". `minted`, `sunk`,
  `exportsRemaining`, `importsRemaining` and `loop` join `orchestrator` on the read-past list (:1303-1305).
  `budgetDay` stays: it is the society's day clock. `minted = -1` stops meaning "invalid".
- New optional fields, each lenient: `board`, `bands` (`{band, since, liftedUntil}` per household), `holds`
  (`holdUntil` per account), `overrides` (bands, harvests, Times nudges, each with an expiry), `lean` (lean days),
  `faucets` (the daily tallies), `escrowSource` and `quarantine`.
- The encoder loses the loop save (:970-1102) and `readLoop` (:1107-1258).

**`Society::restore` (`RatwSociety.cpp:973-1121`), lenient:**
- The sum gate (:1037), the `minted`/`sunk`/exports/imports range gates (:1017-1022) and the old-save fund moves
  (:1094-1116) go.
- **Every gate that refused over people becomes a reconcile:**
  - an account for an unknown resident (:1030): dropped if staged; kept if real or escrow;
  - a resident whose role changed (:1039-1048): takes the authored role;
  - a resident count that differs from the authored roster (:1070-1074): departed residents' staged accounts are
    dropped (not paid into the treasury), newcomers get purses at their band's target (not minted). This rewrites
    the authored-roster path at :986-1016.
- The ledger check `e.day > s.budgetDay` (:1064) stays, since `budgetDay` stays.
- Accounts under dead prefixes (section 9.1) are dropped with their goods.
- **Unknown items** (`itemValid`, :1032-1034): dropped from staged accounts. In real and escrow accounts they move to
  a `quarantine` list on the account, shown to the player as "goods the world no longer knows", and come back if the
  item returns. No faucet, nothing lost. An escrow emptied and closed hands its quarantine on with its goods
  (`Society::closeAccount`'s heir: a stall's to its keeper, a lodging chest's to its renter, a pot's to whoever takes
  its coins); with goods in quarantine and no heir it stays open, keeping them.
- Staged accounts that are missing or out of range are settled to target. Saved budgets, holds and lean days are
  kept. Real and escrow accounts are untouched.
- Refusal stays only for real corruption: a negative or overflowing real or escrow account, or broken goods limits.

**`live.npc_state`** (`npcStates`, `RatwCheckpoint.cpp:541-570`) stops writing residents' `cash` and `stock` and
writes `band`. `applyExternalNpcStates` (`RatwGame.cpp:1797-1850`) ignores cash and stock, drops its treasury pay-in
and pay-back (:1830-1846) and the `restore()` call that follows it (:1847), and applies a band only as an override.

**Authoring is lenient.** `RatwAuthoring.cpp:494-740` (about 240 lines, to about 90) parses one `economy` line (with
`treasury`, `store_herbs` and `store_meals`, :494-504), plus `rule`, `service` and `season` lines (:621-644).
**Unknown economy record kinds and ids are skipped with a warning, never `reject()`.** This differs from the rest of
the file's reject style (:842-979) on purpose: a host's half-finished edit must not stop the world loading.

**`EconomySpec`** (`RatwSociety.h:455-511`) keeps `needsPaused`, `rules`, `Service`/`services` and
`Season`/`seasons`, and the `treasury`, `storeHerbs` and `storeMeals` fields. Its `farms` and the rest go.

**Migration 0061** (schema only; the last today is 0060):
- `live.economy` keeps `world_id`, `rules` (jsonb: prosperity and its cause, the clamps' flags, the Board's rules,
  the faucet caps, Times nudges, harvest overrides, the host's `status.json` and `market.json` overrides),
  `seasons`, `services`, `needs_paused`, `treasury`, `store_herbs` and `store_meals`. The dead columns (the other
  `store_*`, `country*`, `gate_toll`, `carriage`, `wage_*`, `subsidies`, `comforts_*`, `farms`) stay in place and
  ignored (D20).
- `live.business_settings` is dropped (D10). It comes out of world_sync's synced tables (`tools/world_sync.py:431,
  444, 718`) and out of `tools/world_build.py:42-43`.
- `live.npcs` gains a nullable `band_override`. `purse`, `herbs` and `meals` stay, ignored.
- `live.people_manifest()` is redefined with one `economy` line plus `rule`, `service` and `season` lines.
- Matching changes: `tools/world_store.py`'s versioned tables (:118-176), `tools/live_edit.py`'s `economy` key
  (:172-358). `live.economy` stays never-synced (`world_sync.py:45, 63`).
- NpcTab's Purse, Meals and Herbs (`Editor/src/dm/NpcTab.tsx:391-394`, `Editor/src/components/Inspector.tsx:368-374`)
  become a Band picker, default "from rank". The Inspector's hint "Hungry residents buy meals at the nearest open
  shop" (:373) becomes "Residents are fed by their band; the poor go short when the nearest food shop is bare."

**Order.** DEV applies 0061 over 0060, after a backup (DEV is not precious). PROD is on the old map at 54; it applies
55-61 in order when the user pushes, and 0061 only drops and redefines, so it is safe on either path. When PROD gets
this is D19. On a world's first boot after the change, a one-line log notes "economy staged (doc 79)". Doc 78's
build order is D22.

## 15. Performance

**Today:**
- The society's hourly loop runs about 14 money steps at 18:00, single-threaded, inside `Society::decide`
  (`RatwSociety.cpp:583-598`). The old map measured a 90-200 ms spike.
- A year's run (`artifacts/economy/r2/steer-p5c-A9.out`) shows days getting 45% dearer over the year, unattributed.
  It probably lived in loop or steering state; not proven.
- The gate (`--simulate 6 8 --players 20`: mean 2.56 ms, p99 5.26 ms, worst 24.5 ms) never covers shop hours, 18:00
  or the day's turn. Phase 0 takes a baseline of the 17-19 and 23-25 windows on today's code, with the user's
  go-ahead.

**Removed:** the hourly `householdsShop` (4.2% of instructions in doc 69, before farms and the Wharf), the store
room's `restockFromStore` and its `producerFor` call (about 2.5%), the 18:00 money steps, most journal volume (no
NPC-to-NPC trade entries, no provision events; the shadow-diff narrowed).

**Added, per day:** households scored and sorted (about 800, under 1 ms); one provision per household at its hour
(raw stock moves); the nightly set and goods trim over about 1,000 accounts; the Board's sweep over 100-500 entries;
the morning ripples (at most 13 updates per good with a net position); posting checks for up to about 50 routes;
10-16 wagons that hop while unseen (an existing cost); a few staged events. **Per trade:** two ladders of at most 99
steps and I3's minimum over about 10-30 markets; no ripples at trade time. Errand-runners only near players, at most
2 a cell.

**The gate (Phase 11):** *(The baseline it is measured against is the merged Phases 7 and 8's table in Progress,
2026-11-11, "Phases 7 and 8 repaired and merged; the new baseline": 765 residents, two runs each, 6-8 mean 2.60 ms, p99
5.77, after the first minute 5.44 and worst 12.2; 17-19 2.14, 5.21, 4.99, 21.9; 23-25 1.62, 4.52, 4.04, 12.8; 6-8
`--ship-day` 2.65, 5.86, 5.49, 12.7. It replaces Phase 0's table from now on, for Phases 9-11: no window slower than it
beyond noise.)*
- `world_check --simulate 6 8`, `--simulate 17 19` (the old peak and shop hours) and `--simulate 23 25` (the nightly
  set and the day's turn), all at `--players 20`;
- p99 at most 5.3 ms and worst at most 25 ms in every window; 17-19 and 23-25 no slower than 6-8 beyond noise;
- a 30-day `--coarse-ticks` soak with a flat day's wall time and no state growth (the Board, committed departures,
  caravans, the journal, the tallies, holds and staged wallets' goods stay bounded). The soak is a simulation: ask
  the user first.

## 16. Tests and checks

All seeded and offline; each ctest takes seconds. No balance runs.

1. **`status_tests`**
   - At −100, −50, 0, 50 and 100, on worlds of 1, 4, 9, 14, 120 and 800 households: shares of the unguaranteed pool
     within one household of the table; the realm-wide minimum holds; no destitute household without a reason.
   - Shares, short-day chances and theft odds are monotone in the slider.
   - The guarantees hold in their order of precedence (a House head in the Sump is rich); with the Sump deleted, no
     error. A band never sits more than one from its home and class without an event.
   - Never Broke empties destitute; Never Hungry empties short days.
   - At most one band a week; "now" skips it; the slider smoothed to 10 a day; the same result on 1 and 4 threads;
     save and restore round-trip with no band jump; the fortune noise stays within ±0.3 over 500 weeks.
   - A `status.json` override that sums to 140, isn't monotone or has a share under 2% falls back to the default.
2. **`life_status_tests`**
   - A born child takes the household's band; an arrival placed in a post takes its household's; losing a post
     lowers the score.
   - Kept from `people_flow`: `noNpcIsMadeButByBirth` and `birthsFollowDeaths` (:194, :132).
   - `theClasses` (:115) re-based on `postKind`.
   - **With the city loop gone, succession still fills a post emptied for 8 days, and farm posts still fill.**
   - A seeded 3-day small world: a comfortable household never shows hungry; a destitute one does on its short days.
3. **`market_tests`**
   - Twenty players buying bread raise its **shown** dozen price inside the clamp, and it decays to half at the
     half-life (lazy and nightly decay agree). One player buying 10 loaves in Ridgemere moves the dozen by 1p.
   - A lone player's day in Coldwater moves no good past ±0.15.
   - A 30-ore dump pays less for each piece, then levels at the cap; the next morning bars at half and nails at a
     quarter, no third step; each step keeps at most 6 goods; a cyclic recipe graph terminates.
   - Buying and selling the same good the same day leaves no downstream pressure.
   - A wagon's departure halves the source's glut; its arrival never raises a cargo good's P; a raid adds 0.15.
   - Imported goods thin between wagons and refill on arrival; a market with no route refills each morning.
   - The take allowance, the budget and the per-holder caps refuse with their reasons; a restart keeps the budget.
   - Every catalogue good has a depth group.
4. **`exploit_tests`** (properties, over 10,000 random states of slider × season × climate × region × district × P ×
   quality × stall × faction standing × quantity, on **rounded, floored, discounted lots**):
   - I1: every bought lot at least 1p, everything finite;
   - I2: a round trip in one shop loses at least 40%;
   - I3: for every good and every pair of markets and days, buying (at a stall or a shop, with any faction discount
     or surcharge) and selling elsewhere or later never pays; the sweep covers lots of 1-5 pieces of every good from
     1p to 20p at both clamp ends, and q = 1 for every 1p and 2p good;
   - I4: crude, fine and masterwork share P;
   - a 0p lot is refused and moves no pressure;
   - a second character on the same account shares every cap (pressure, sell total, theft, stakes, welcome);
   - a second character's purchases never raise a shop's budget;
   - a 1p good's sold lot falls as P falls;
   - a second theft from a lean NPC pays nothing; the daily theft cap holds against thousands of victims;
   - the faction discount can't pay more than its weekly cap;
   - a refund never exceeds its payment;
   - 10^6 trades: no overflow, no NaN, no price at or under 0.
5. **`trade_show_tests`** (grown from `roads_tests`' `townsAndCaravans`, :205)
   - Departures are committed when posted, between real markets on real routes, carry no account, and keep their
     notice's promise through a DM change.
   - The driver is a real carter, away from his post for the trip, home robbed or hurt after a raid, never dead.
   - Manifests name only goods the destination's shops stock or buy; notices use the calendar's weekday names.
   - An arrival refills shelves and moves the Board at that moment; a raid raises P at the destination, posts a
     bounty and leaves recoverable goods at the camp.
   - No port, no ships; no staffed farms, no carts; notices post and expire; staged events post with their cause.
   - An endpoint deleted while a wagon travels is a clean drop on load, and its letters reopen or refund.
6. **`host_edit_tests`** (the robustness gate). Each fixture loads, stages something plausible and refuses nothing:
   no port; one market; a market's merchants deleted; a lone merchant outside every community; every farm deleted;
   every shop deleted; every carter deleted; zero households; a business label changed; an NPC deleted while it holds
   an account; an item deleted while a player holds it (quarantined); an unknown good in pressure, a rule or an
   override; a slider of 9999 or "abc"; a `status.json` override that sums to 140; a purse target of 10^9 (clamped);
   a contract whose town is gone (refunded).
7. **`save_compat_tests`.** Fixtures, each copied once with the user's say-so:
   - a frozen pre-79 DEV checkpoint, holding `minted`, `sunk`, `loop`, `steer`, `fund:relief`, `outside:wharf` and
     `farm:` accounts;
   - a PROD-shaped save from `dev-old` (the old map): `stores:` town treasuries, caravans in transit with letters and
     escorts taken, `bandits:` hoards holding player coins, NPC-posted contract escrow;
   - a world package made before doc 79 (players poured into the treasury);
   - a journal tail written by pre-79 code and replayed by post-79 code (it sets `society.minted`, `budgetDay` and so
     on);
   - a doc 78 §12.6 "Reset economy rules" copy from before doc 79 (only `steerLean`), which opens at prosperity 0.
   - Each loads. **Player purses and escrow survive to the penny.** Dead accounts are gone and staged accounts sit at
     their targets. The new state round-trips through wire and through the `world_store` Postgres tests.
8. **`street_tests`**
   - Thefts a day per market stay within the cap and rise as the slider falls; a 4-household hamlet sees a theft
     every few weeks, not daily; hungry thieves take food from a till.
   - Beggars are only destitute adults without a post, at most 2 a cell; the Restday dole sets `on_dole`; alms go
     only to NPCs.
   - Players empty a market's staple: that market's poor show more hunger the next day; the comfortable never do.
   - Help a beggar: the next day they don't beg, and a `pair.owed` line can fire.
   - The standing-still check: a player standing in a shop sees no stock change without a buyer in the room.
9. **Talk, voice and the Mind**
   - `wealth` and `class` come from the band; the price an NPC names equals the shown price; `world.<good>_price`
     says up only when the shown price moved; a clamped dear-season push is blamed on the season.
   - `business=failing` never pairs with "can't complain".
   - The `means` field stays under 300 characters and carries the shown price and pay word (`tools/test_npc_mind.py`,
     offline).
   - Every new Tracery list, ship name, event and notice passes the forbidden-word check (`talk_tests`,
     `voice_tests`).
10. **Package and DM**
    - `tools/test_world_package.py` rewritten (:171-178, :180-, :228, :305-314, :442-571, :703, :716-718): players
      gone, no player ids, staged accounts and pressure dropped, the host's rule overrides carried, the package loads
      on a fresh server.
    - `tools/test_studio.py`'s slider test retargeted at `prosperity` (Phase 9, with the route); `tools/test_world_sync.py:1340` (seasons,
      rules and services reaching the server) still passes; the edicts, ledger and year-log tests go.
    - The client tests next to `canTradeItem` (`state.ts:2456`) cover a 0 unit price, the dozen list and the new
      refusal strings.
11. **`realBalanced()` everywhere `conserved()` was:** society, crime, letters, projects, gathering, director,
    journal, aging, careers, wire, checkpoint, estates, roads.
12. **Kept and trimmed:** `shops_tests` (players buy and sell at the shown price; shelves refill; sold goods move on),
    `sim_fast` 1-269 (with `setUnseenStep`), `farm_people` placement (:284, :535), `estate`, `roads`.
13. **`world_check --stage FILE`:** one JSON line per game day: per band its share, hungry share and thefts; per
    market its price index, Times and cause; wagons, ships and events seen; faucets and sinks. For reading a soak,
    never for balancing. A script of about 100 lines replaces `econ_report`, `econ_weeks` and `people_summary`.

## 17. What goes

**Core** (about 7,800 lines):

| File | What goes | Lines |
|---|---|---|
| `Core/RatwCityLoop.cpp` | All of it, after `households()` (to Society), `postKind` (to Careers), `homeClass`, `shelfOf` and the seasons' `seasonal`/`priceFactor` (to a new `Core/RatwMarket.cpp`) move out | 3,138 |
| `Core/RatwSteering.cpp` | All of it | 1,399 |
| `Core/RatwWharf.cpp` | All of it | 470 |
| `Core/RatwFarms.cpp` | Farm money, the schedule, `crewScale` and the area switch's off state. Kept (about 200 lines): `FarmNow`, `buildFarms`' list, `farmWorkplace_`, `countFarmPosts`, `farmOfPost`'s data, the areas and their season groups | about 440 of 642 |
| `Core/RatwFarmPeople.cpp` | `staffFarms`' till and crew checks (:323-330); the `WharfFund` arrival purse (:527) | |
| `Core/RatwSociety.cpp` | The store room (:1355-1683) except `startingStock`, which moves; `foundShops`' mint (:1346-1350); minting in `reset`; the restore gates, rewritten as reconciles; the old-save moves; `moneySupply`/`conserved` (:357-367), replaced by `realBalanced`; `soldToday_`/`soldOut_` (:404-409); the Greyfen demo roster's own mint and sink (:933-969) | about 450 |
| `Core/RatwSociety.h` | `LoopState` (:245-375); `EconomySpec`'s `farms` and the loop fields (keeping `needsPaused`, `rules`, services, seasons and the three economy-line fields); the loop, farm money, Wharf and steering declarations (:755-1363); `moneySupply`/`conserved`; `TillFloat` | about 850 of 1,688 |
| `Core/RatwResidents.cpp` | The mints (:58-59, :79, :146, :177); `restockFromStore` behind `storeRule` becomes the morning restock | |
| `Core/RatwCareers.cpp` | The mints in `arrive` (:387-396); the `WharfFund` purse (:414-416). `items::producerFor` stays | |
| `Core/RatwWire.cpp` | The loop save and `readLoop`; `minted`/`sunk` | about 290 |
| `Core/RatwItems.h/.cpp` | `CityLoop::Steering`, `readSteering` (:394-492), `RATW_STEER_TABLE` (:911-923), most of `Rules[]` (:178-304); `houseBusinesses` (:802) moves to `market.json` | |
| `Core/RatwGame.cpp` | `wharf.credit` (:958-975); `applyExternalNpcStates`' cash path and its `restore()` (:1830-1847) | |
| `Core/RatwGameDev.cpp` | The store-room draw becomes a raw add into the till | |
| `Core/RatwDirector.cpp` | The `minted`/`sunk` export (:334-336) | |
| `Core/RatwGameJournal.cpp` | The minted/sunk/budget counters (:41-45, :181-190); the shadow-diff narrowed | |
| `Core/RatwGameTalk.cpp` | `talkRichLine`, `richLine_`/`poorLine_` (:23-50); the per-speaker scan (:121-127); `wealth_days` (:129) | |
| `Core/RatwTroubles.cpp`, `Core/RatwGameTroubles.cpp` | `foodDays` as the measure of wealth (:134-160), the trouble's coin cost (:190-196) and the help check (:463), rewritten by band | |
| `Core/RatwRoads.cpp` | the `stores:` loads, the two-towns requirement, real caravan loads; the road layer's `towns_` uses move to markets | |

The stale header comments at `RatwSociety.h:20-27` and `RatwResidents.cpp:15-19` are rewritten.

**Data:**
- `Data/Items/city_loop.json` (67 KB) goes. Each section's fate:
  - `seasons`, `services`, `wharf` names, `shelves` (`startingStock`), `luxuries` (which goods the rich buy and
    `times` touches), `houseBusinesses` (Houses' ownership, for bands and talk), `estates` (for the guarantees) →
    `Data/Economy/market.json`;
  - `wages` keeper words → the class word lists in `RatwCareers.cpp`; the pay amounts → pay words in `status.json`;
  - `food` (food days) → gone; `status.json`'s food rows replace it;
  - `patrons` and `markets` → gone, unless Phase 0's list finds a kept reader, in which case they move to
    `market.json` too.
- `Data/Economy/country_farms.json` keeps its farm defs (keepers, hands, kind, area, works areas), which
  `staffFarms`, `tools/worldgen/realm2_places.py:742-745` and `tools/test_realm2.py:167-197` read. Its money fields
  (yields, prices, schedule, `crewScale`, wages) go.
- `Data/Items/regions.json`: already dead; it goes.
- Kept: `communities.json`, `farm_people.json`, `workshops.json` (buildings), `crafts.json`, `businesses.json`,
  `items.json`, `ladder.json` (projects read it, `RatwGameProjects.cpp:36-37`).
- New: `Data/Economy/status.json`, `Data/Economy/market.json`. Both are defaults; host changes live in
  `live.economy.rules`.

**Tests** (about 8,200 lines): `steer_tests`, `farms_tests` (its staffing cases move to `life_status_tests`),
`wharf_tests`, `selfcorrect_tests`, `treasury_tests`, `econ_fix_tests`, `trades_tests`, `workshops_tests`,
`season_tests` (its `theFactorsByDay` idea is rewritten into `market_tests`), `store_room_tests`, the unregistered
`Tests/society_soak.cpp`, and their `add_test` registrations in `CMakeLists.txt` (:255-305). The 260 `conserved()`
calls become `realBalanced()`.

**`world_check`** (about 1,400 lines): `moneyReport`, `EconomyDetail`, `EconomyRecord`, `EconomyWatch`; the flags
`--economy*`, `--steer-table`, `--farms*`, `--rule-at`, `--loop-state`; `Society::setShortageWatcher`. **Kept:**
`--fast` (still `--unseen-step 30 --straight --coarse-ticks`), `--unseen-step`, `--straight`, `--coarse-ticks`,
`--day`, `Society::setUnseenStep` and `World::setStraightOffstage`: they are sim speed, not economy.

**Tools** (about 10,600 lines): `steer_accept`, `steer_calibrate`, `steer_floors`, `steer_measures`, `steer_runs`,
`econ_report` and its test, `econ_detail`, `econ_weeks`, `econ_diff`, `econ_table`, `people_summary`,
`farms_summary` and `test_farms_summary.py`, `wharf_rename_compare`, `city_loop`, `city_capacity` and
`test_city_capacity.py`, `country_industry` and `test_country_industry.py`, `supply_balance`, and `tools/econ_model/`
(already dead). In `item_catalog.py` (:199, :209), the requirement for craft and producer `hours` from `apply_ladder`
goes, not only the pointer.

**DM and Editor** (about 900 Python and 2,300 TS lines): `studio.py`'s steering helpers (:143-322), most of `Economy`
(:323-1042) and the drift view; `map_editor.py`'s steering and loop settings; `dungeon_master.py`'s `money()` and
`wharf_*` (its `stores:` read at :1011 now feeds the Prosperity tab's town treasuries); `SteeringPanel`,
`steering.ts` and its test, `WharfCredit`, `WharfPanel`, `FarmsPanel`'s wages, `RulesPanel`'s held rules;
`Editor/src/dm/EconomyTab.tsx` (106 lines) and `Editor/src/studio/EconomyControls.tsx` (150), rewritten for the slim
rules; `SeasonsPanel.tsx` (97), retargeted at `market.json`'s overrides; `Editor/src/lib/economy.ts` and its test,
trimmed of steer tags; `sync.ts`'s `economy` conflict block (:67-70, doc 66); the "economy is never touched" copy in
`UpdateFromLive.tsx:82` and `ServerTab.tsx:192`, checked and reworded.

**Artifacts:** `artifacts/economy`: 28 GB untracked on disk, 34 MB tracked. Deleting it is the user's call (D18).

## 18. What players keep

- **Their purses,** real to the penny, with every limit as today.
- **Buying and selling at NPC shops,** through the same protocol (`RatwGame.cpp:4904-4937`, :4100-4140), with three
  new ware fields (list count, smallest sell lot, trend). Prices still react, now to them: to their buying, their
  selling, the season, the harvest, wagons, ships and staged events.
- **Every player-to-player path:** stalls (`RatwStalls.cpp`), gifts, letters and enclosures, lending, game tables,
  Chapters and their treasuries, projects, contract escrow, story prizes, theft between players.
- **Ways to earn:** selling gathered, hunted and looted goods, rewards (factions, archive, festivals, bounties,
  escorts, story prizes), courier contracts, recovering raided cargo, crime, stalls. All of them now always pay,
  within each faucet's cap, because staged payers are topped up.
- **Ways to spend:** shops, repairs, lodgings, Chapter rents and buildings, fees, postage, fines, companion and Hold
  wages. They are the sinks, and the NPCs a player pays now show it.
- **Faction terms** (the surcharge as today, the discount capped per Chapter per week), **the maker's scent, wear,
  repairs, qualities, the 10% stall discount, `SellBackShare`.**
- **Apprenticeships** with NPC masters (`Society::apprentice`), unchanged.
- **The welcome** of 20p, 2 herbs and 3 meals, now never short, once per account per world per game week.
- **Things that come back:** caravans to escort and bandits to rob, bounties, courier carts, ship days, beggars to
  help, keepers who talk about business, staged events to follow.

## 19. Phases

"Plan" is the careful estimate. "Expected" is about 0.3 of it for familiar work (Python, DM screens, talk data, the
port) and about **0.45** for the phases that are new kinds of work or untangle threaded C++: Phase 0, the Board, the
bands, the cut, the roads, street life and staged events. The 0.3 comes from doc 77's record (0.12-0.26 on greenfield
Python), so it is generous for familiar work. **Reviews are counted at face value,** as doc 78 does, because doc 77's
reviews found 25-35 findings each. If a phase runs past its **expected** hours, stop and ask (memory
`session-budget`).

**Slice 1: the staged core**

| Phase | Work | Plan | Expected |
|---|---|---|---|
| 0. Loop off | Copy the frozen DEV and `dev-old` fixtures (the user's say-so) and write `save_compat_tests` first; move `postKind` and `households()` (with the homeless); keep the farm list and drop `staffFarms`' account checks; minimal food by class, the nightly purse and treasury set, the welcome as a faucet; one rule that switches the city loop, steering, Wharf and farm money off (building on `needsPaused`); run the full ctest suite with it on and **list every failure and every kept reader of `city_loop.json`, `house:` and `towns_`**; a perf baseline of the 17-19 and 23-25 windows on today's code (the user's go-ahead) | 6-9 | 3-4 |
| 1. The Market Board | `Core/RatwMarket.*`: markets found at load (ids, lone merchants, k-nearest routes); P, decay, the per-piece ladder, the per-holder decaying cap, net positions and the morning ripples (capped fan-out, upstream value weight); depth by catchment; the clamp with season and district inside it; I1-I5 in `quote`, floored sell lots, the per-holder backstop live; the goods-at-0 sale path; the budget (saved), the on-demand till top-up, the take allowance, the sell total; shelf targets and the morning restock; `quote` replaces `shopFactor` outright; the faction discount cap; the client's dozen list, `sellLot`, ▲/▼ markers, "BUYING TODAY" and `canTradeItem`; `board` saved; `market_tests` and `exploit_tests` | 18-24 | 8-11 |
| 2. Bands and staged accounts | `statusOf`, the score (home, bounded fortune, events, earning from players), the pool, the reasoned cut, precedence, saved `{band, since, liftedUntil}`; `status.json` with its checks and overrides; food from the shelf with short days and buy-ahead; holds, lean days, the goods trim; refunds capped; `classify` with the full prefix table, escrow sources and the faucet tallies (log only); the faucet caps; staged payers topped up (the `min(cash)` sites); the direct familiarity step; `wealth`/`class` facts and the Mind's `means`; `status_tests` and `life_status_tests` | 16-21 | 7-9.5 |
| 3. The cut | Delete the city loop (after what moves), steering, the Wharf, farm money, the store room, minting, the wire loop; restore's gates as reconciles, quarantine, read-past; journal narrowed; `world_check` stripped (sim flags kept); tests and tools deleted; `conserved()` → `realBalanced()`; `world_package` changed; Editor files trimmed; the succession and farm-post tests; docs marked superseded (section 21); notes and memory pointers marked history, with the user's say-so (0.5 h) | 20-28 | 9-12.5 |
| Review A | Two reviewers: the code (exploits, saves, faucets) and the look (does T1-T17 hold so far) | 6-8 | 6-8 |
| **Slice 1** | | **66-90** | **33-45** |

Phases 1 and 2 can run in parallel after Phase 0. Both touch `RatwSociety.cpp`: Phase 1 owns `quote`, `trade` and
`transfer`; Phase 2 owns `restore`, the accounts and the nightly set.

**Slice 2: the world comes alive**

| Phase | Work | Plan | Expected |
|---|---|---|---|
| 4. The roads | Markets drive the road layer (law stays on `towns_`); committed, saved departures; real carters as drivers; manifests filtered to the destination's shops; departure and arrival nudges; late with causes; raids, recoverable cargo, bounty caps; dropped caravans' letters and orphaned contracts; farm carts every morning with harvest loads and baselines; Marketday stalls from carts; regional restock and the back-room buffer; notices with weekday names; `world.news`; `trade_show_tests`, `host_edit_tests` | 20-28 | 9-12.5 |
| 5. The port | The committed ship timetable, the Wharf's crowd and tasks, import and export nudges toward baselines, the board timetable, `world.ships`. (A ship at the pier: +4 plan, +1.5 expected, if D8 says so) | 6-9 | 2-3 |
| 6. Street life | The expected theft count and targets, the till-food path in `steal()`, band-scaled takes, `worth()` from the Board; begging, the Restday dole, rough sleeping; errand-runners near players with per-cell caps; the standing-still rule for staged sales; `business` from sales, `on_dole`; the "short" trouble by band; the lifted state and its cap; `street_tests` | 14-19 | 6.5-8.5 |
| 7. Staged events and causes | The seeded event scheduler and its list in `market.json`; the slider's cause (DM text or the seeded list), `world.times_cause`, criers and notices; override notices; the harvest lean | 4-6 | 2-3 |
| 8. Talk | Price topics back with shown prices, the shown-price gate, cause attribution, `topic.item` widened, the keeper's trade answer, class-gated tags, the failing salience rule, 450-650 new lines (with the roads' facts Phase 4 sets: `caravan due`, late and its cause, `world.news`, `world.harvest`), the forbidden-word check | 14-20 | 4-6 |
| **Slice 2** | | **58-82** | **23.5-33** |

Phases 4-8 all edit `Data/Talk/facts.json`, `RatwAmbient.cpp` `sceneTopics`, `RatwGameTalk.cpp` and `RatwRoads.cpp`.
**They run in sequence by default.** If the user wants parallel sessions, the owner of each shared file is named
before they start, and a merge line adds about 2-3 plan, 1-2 expected (D30).

**Slice 3: hosts and gates**

| Phase | Work | Plan | Expected |
|---|---|---|---|
| 9. DM, Atlas and the database | Migration 0061; lenient Authoring with season lines; `world_store`, `live_edit`, `world_build`; the studio slimmed; host overrides of `status.json` and `market.json` in `live.economy.rules`; the Prosperity tab with the player-money report and the week's wagons, ships and events; the slider re-meant with its cause; band, harvest and Times overrides; reset prices; the Band picker; the `paid` relabel; Editor panels cut and retargeted (left by Phase 3, named in its Progress entry: the store room's, the loop's, the comforts', the Wharf's and the farms' money panels, routes and validators, and `country_farms.json`'s money fields); `dm_service` and the Director; DM tests | 16-22 | 5-6.5 |
| 10. Look and feel | A hard box: the user plays a scratch server against the T1-T17 checklist; numbers tuned (meal counts, depthBase, half-lives, shares, event rates); then stop and ask | 6-10 | 3-5 |
| 11. Gates | `--stage`; the 17-19 and 23-25 windows; the soak (the user's go-ahead); the PROD apply plan; the doc's Progress | 6-9 | 2-3 |
| Review B | Before PROD, if the user wants it | 6-8 | 6-8 |
| **Slice 3** | | **34-49** | **16-22.5** |

**All three: about 158-221 plan hours, about 73-101 expected,** with both reviews. Without Review B: about 152-213
plan, 66-93 expected. The stop-and-ask rule uses these numbers.

**What the reviews changed in the hours.** The first draft said 124-172 plan, 47-64 expected. The rise comes from:
Phase 0 (new, 6-9 plan); the 0.45 factor on the cut and street life (+3-4.5 expected); reviews at face value (+6-8
expected); the look-and-feel box (6-10 plan); staged events and causes (4-6 plan); and the believability work the
reviews asked for: real carters, committed schedules, regional restock, shelf-tied hunger, the dozen list (about 12-16
plan across Phases 1, 2, 4 and 6).

Order and dependencies:
- **0 → (1 and 2) → 3 is the critical path.** Food must come from bands before the city loop's shopping is cut, or
  every NPC starves; Phase 0 puts minimal food in first. `postKind` and the farm list must be in before the cut, or
  succession and farm staffing stop.
- Phases 4-8 follow Phase 3, in sequence by default. Phase 9 can start after Phase 2.
- Tuning waits for Phase 10 (memory `implement-before-balancing`).
- **Doc 78:** its M0-M4 and R1 don't touch the economy and can go first. Its M5-M7 (the settings allowlist, "an
  economy lever to try", "Reset economy rules", the `live.economy` copy) wait for Phase 9's migration 0061, or are
  built against the slim table from the start. If they go first on today's table, add 2-3 plan, 1-1.5 expected to
  Phase 9 for the rework (D22).

### Decision points

- **After Phase 0:** the first check-in. The failure list and the kept readers say whether Phase 3's hours hold. If
  they don't, re-plan Phase 3 before Phase 1 starts.
- After Phase 1: does the clamp leave the seasons enough swing? If not, D2 or the season tables change, never the
  clamp's bound.
- After Phase 2: show the user a census at −100, 0 and +100 on DEV's households (a read, not a simulation).
- After Phase 3: Review A, then the user tries a scratch server before slice 2.
- Phase 10 is boxed: at its expected hours, stop and ask.
- Before Phase 11's soak: ask.

## 20. Risks

| Risk | What we do |
|---|---|
| Every NPC starves between cutting the loop and feeding by band | Phase 0 puts minimal food in before anything is cut; a seeded 3-day test that a comfortable household never shows hungry |
| Succession or farm staffing stops when `payOf` and farm money go | `postKind` and the farm list rehomed in Phase 0; the 8-day succession and farm-post tests |
| The cut is bigger than planned | Phase 0's failure list measures it before Phase 3 is re-planned |
| Players find a money loop (hauling, seasons, rounding, stalls, qualities, faction discounts, a second character) | I1-I5 inside `quote`, constants not levers; sold lots floored; the per-holder backstop on rounded lots; the faction discount capped; every cap per account holder; property tests over small lots and both clamp ends |
| A faucet runs unlimited now that the world always pays (theft, stakes, the welcome, bounties, raided cargo) | A cap per account holder on each, in the faucet table; `realBalanced()` in every test; the Prosperity tab's made-against-sunk report |
| Players push Times to win bigger budgets or more thieves to report | Times reads no player-caused pressure |
| One player crashes a market for everyone | The decaying per-holder cap (±0.15), the daily sell total, the take allowance, depth's floor of 20, decay |
| Ripples ratchet prices for everyone at no cost | Ripples from the net daily position; value-weighted upstream; capped at 0.15; counted against the holder's cap |
| A small hosted world looks dead | Wagons, harvest carts, ships and staged events move prices whatever players do; regional restock makes wagons matter |
| Talk contradicts the shelf | Talk reads the shown price; up or down only when it moved by 1p; causes from what moved the clamped factor |
| A notice lies | Departures and ships are committed and saved when posted; only announced causes change them |
| Prices or bands move with no cause in the world | The timing rule; a cause for every slider move; override notices; the standing-still rule |
| A restart changes what players see | Budgets, holds, lean days, bands and committed departures are saved; load settles only what is missing |
| A host's edit stops a world loading | Lenient on read everywhere; quarantine for unknown items; `host_edit_tests`; `save_compat_tests` |
| Player escrow is lost when a host deletes a town or a road | Letters reopen or refund; orphaned contracts refund; tests |
| Hosts can't edit `Data/` | Defaults in `Data/`; host overrides in `live.economy.rules` |
| Slider whiplash | Smoothing at 10 a day, one band a week, percentile hysteresis; "now" only on purpose |
| Theft becomes a crime wave (Oblivion's Radiant AI) | An expected count per market, never more than 4 a day; food-first targets; lean days |
| Bandits punish rich players (RimWorld) | Camps follow the slider and season, never players' purses; strength and bounties capped |
| Keeper-customer bonds stop growing | The direct familiarity step each day; errand-runners near players |
| The event store floods, or the Mind remembers provisions as gifts | Provisions are raw stock moves with no WorldEvent |
| Off-screen carter deaths distort births | Raided carters come home robbed or hurt, never dead |
| Old saves won't load | Read-past fields, lenient restore, four kinds of frozen fixture |
| PROD's migration path | 0061 only drops and redefines; PROD applies 55-61 in order; the push is the user's |
| Doc 78 is built on today's economy table | Its M5-M7 wait for 0061, or the rework is budgeted (D22) |
| Parallel sessions collide on talk and roads files | Phases 4-8 in sequence by default; named owners and a merge line otherwise |
| Tuning eats the schedule, as in docs 64, 73 and 75 | One boxed look-and-feel phase; no balance runs; stop and ask at its expected hours |
| The day's cost growth over a year comes back | The soak watches the Board, departures, caravans, journal, tallies, holds and staged goods for growth |
| Losing something visible people liked | T1-T17 as the acceptance list for every phase; Review A looks at the look as well as the code |

## 21. What changes in docs 42, 61, 63-75, 77 and 78

Each superseded doc gets a banner at its top: "Superseded by doc 79 (2026-11-08) except where doc 79 says it stays."
The docs stay as history. `Docs/Hosting.md:8` ("their economy") is checked and reworded if needed. About 12
auto-memory entries describe the dead economy (stable economy, the Wharf, economy steering, the orchestrator, money in
circulation, industry of the realm and others); they are marked history with the user's say-so (Phase 3).

**Doc 42, money in circulation.**
- The brief, decisions and the rule against hoarding: history. The "rule against hoarding" now means only that a
  staged purse moves down as well as up.
- Phase 1 (town purses, the monthly reckoning): superseded; town treasuries are staged `stores:` accounts.
- Phases 2, 3, 3b and 3c (who pays wages, a living for every wolf, work out of town, industry outside the gates):
  superseded; nobody pays NPC wages. The posts themselves stay.
- Phase 4 (NPCs take contracts): the NPC-posted courier contracts come back, as a capped faucet.
- Phases 5 and 5b (towns and churches spending; Houses own businesses): the money goes; Houses' ownership stays (now
  in `market.json` `houseBusinesses`) as an input to bands ("a House head is rich") and to talk.
- Phase 6 (Restday at church): the schedule stays; the dole (section 10.2) is new.
- Phase 7 (caravans and regional goods): replaced by staged caravans (section 8) and the region factor.
- Phase 8 (watching the money): replaced by the Prosperity tab.
- "Starting money": the 20p welcome stays, as a capped faucet. The later rounds (the month's test, pools, spoilage,
  pressure, taxes, farmhands): history.

**Doc 61, industry of the realm.** `houses.json` and `specialties.json` no longer exist (b431ab1). The Houses'
businesses live on as `city_loop.json`'s `houseBusinesses`, moved to `market.json`, for bands and talk. The towns'
trades live on through the region factor, read from `workshops.json`, `communities.json` and `crafts.json`
`supplies`. Building materials: history. The farms become carts and harvests. Results and testing: history.

**Doc 63, a stable economy.** All research, history. Its "rich and poor stay, and stay put" lives on as the rank-based
bands; the seventh pass's lessons (hoarding, crashing markets) shaped the caps. The worth ladder stays only for
`projectHourValue`.

**Doc 64, bringing the model into the game.** Superseded whole: Phases 0-13 and every balancing round. What stays:
"A. The penny" (whole pennies); Phase 8's lives, skills and masters (now Careers); Phase 10's hunting for players;
Phase 11's Holds as keeps (Chapters' Holds).

**Doc 65, a supported economy.** Superseded whole. The businesses and posts it placed stay.

**Doc 66, worlds, Atlas and the DM.** Section 5 ("the economy rules erased") is the first erasure; doc 79 is the
second, and its "what their needs bought (food) is paused" is replaced by food by band. The Progress note on "the live
world's truth: NPCs and markets in conflicts" loses its market and purse conflicts (`sync.ts` :67-70): staged
accounts are derived. The rest stays.

**Doc 67, Ridgemere 2.0: the city.** Section 4 (businesses and jobs) and "jobs by class" stay; class now comes
through `postKind`. Section 6 (the internal store room, R35-R39) and its Atlas Economy controls are removed. "Food
rebalance": history.

**Doc 68, the Country and the city's loop.** Sections 1-8 (the loop as money flows, the Country as money, residents'
buying, wages, the treasury, carriage, the restocking rule, rent): superseded. Section 9 (Atlas, the DM and the
settings) is cut down to section 12 here. Sections 10 and 12 (stability gates, export trades): history. In Progress:
seasons stay (moved to `market.json`, inside the clamp); self-correcting rules go; "players in the shifting market" is
replaced by the Board; "births follow deaths; arrivals from the Country" stays, minus the purses from the Wharf fund;
the treasury's target goes.

**Doc 69, simulation speed.** Fast-forward, threads, determinism, births on the harness path, `--fast`,
`--unseen-step` and `--straight` all stay (sections 1-9). Section 10 (running an economy simulation) and "the
economy's levels and report": superseded by `--stage`.

**Doc 70, the realm.** The map stays. Section 7 (costs, the perf gate) gains the 17-19 and 23-25 windows.

**Doc 71, the Country's industries.** "The model: how the game pays and feeds a wolf" and sections 1-3 (the Country's
money, yields, reserves): superseded. Section 4's Phase C (real NPCs replace the virtual crews) stays as `staffFarms`,
on the kept farm list. Section 6 (the placement list) stays.

**Doc 72, pulling the Country into the world.** Sections 1, 3 and 4's money (what the Country sells and buys, who pays
its posts, the industries' accounts) and the land rent: superseded. The siting, the works, the communities' places and
farm tracks stay, as endpoints and routes. "The town sites, erased and kept": the communities become markets, plain
for law. Cheese as the rich's luxury stays, as flavour.

**Doc 73, the Wharf.** Section 2 (the rename) and section 3 (where it is: posts and places) stay. Section 4 (the
market valve) and section 5 (orders by wagon) are removed. Section 6 (controls) keeps only `shipsPerWeek` and
`neverImported`. Section 7 (reports) is removed. Sections 8-10 and the stabilizing rounds: history. Open question 5
(wagons on the road) is answered by section 8 here.

**Doc 74, steering the economy.** Superseded whole. Section 2's slider keeps its control and wiring with a new meaning
and gains a cause. Section 7's Never Hungry and Never Broke become clamps. Section 9 (money conservation) goes, and
`realBalanced()` takes its place in tests. Section 10 (the DM tab) is replaced by section 12 here.

**Doc 75, the economy controller.** Superseded whole. Section 3.8 (how admins see it) becomes the Prosperity tab.
Sections 3.6 (the supervisor) and 4.2 (Crown charters) go. Phase 7's edicts, ledger and year of log become "word on the
street" and the slim log.

**Doc 77, central accounts and hosted worlds.**
- Section 2 (:129-130): the purse is still `society.accounts[charId]`, but only player money is conserved. Per-player
  caps are keyed by the central account.
- Section 5.1 (:374-376): folding a dormant wolf back becomes a plain delete of its account (still not built;
  :1171-1172).
- Section 8 (:497-504): the package's strip is by deletion, staged accounts are dropped, pressure is reset (section 13
  here).
- Section 11 (:630-631): "`Society::conserved` holds after a thousand arrivals" becomes "`realBalanced()` holds; the
  package scan finds no player ids".
- Section 14, Q1 (:428, :701-703): wallets can't travel between worlds "because of conservation" loses its reason. It
  becomes a fresh question (D15). The Progress notes at :1102, :1135, :1375, :1474-1475 and :1494 that mention
  conservation are history.

**Doc 78, hosts run their own worlds.**
- Section 2's tab table (:279-280): Economy becomes the prosperity slider and its cause, the clamps, seasons, harvest
  overrides, Times nudges, band overrides, the pressure rules, the faucet caps, events and reset prices; "Money &
  calendar" loses Wharf credit and gains the Prosperity tab.
- Section 8's allowlist (:1132, :1135): Wharf credit goes; "the controller's floors are the safety net" becomes "the
  clamps, the caps and the rules in `quote` are".
- Section 12.2 and 12.6 (:1230, :1400, :1449): "Reset economy rules" copies a handful of fields; "the controller's
  floors meanwhile" becomes "the invariants hold whatever the rules say".
- Section 16 (:1650): the same change to the economy risk.
- M5, M6 and M7 (:1510, :1518) stand, built on the slim table (D22).

Also touched, not superseded: doc 62 (talk) gains the facts in section 11; doc 59 (lamps and homes) waits to be
rewritten as cosmetic (D13); doc 35's crafting, when built, follows I5.

## 22. Decisions for the user

Each has a recommendation. "Go with the recommendations" takes them all.

1. **D1. How many bands.** (a) Four (destitute, poor, comfortable, rich), the talk library's words, with class as a
   second axis; (b) five, adding "working", which reads as comfortable to existing lines; (c) six, as some games do.
   *Recommended: (a).* No content changes, fewer rows to tune. (b) can be added later without breaking (a).
2. **D2. Should the slider move player prices?** (a) Narrowly: staples a little dearer in hard times and luxuries in
   a boom, at most ±8%, inside the clamp; (b) not at all: it moves NPC wealth, hunger, theft and talk only.
   *Recommended: (a).* Players feel hard times, and the clamp keeps it safe.
3. **D3. Hauling between markets.** (a) Strict: buying from one shop to sell to another never pays (I3); trade's fun
   is selling your own goods where they're dear, stalls, escorts, recovered cargo and bandits; (b) a bounded trader's
   margin of up to X% on some routes, which turns hauling into a faucet. *Recommended: (a)* for hosted worlds; (b)
   can be a later rule with its own cap.
4. **D4. Cheap goods.** Bread, ore and 56 other goods cost 1p, so a single piece's price can't show a move. (a) List
   goods under 3p by the dozen ("12 for 13p"), sell lots rounded down with a smallest sell lot, and 0p lots refused;
   (b) raise staples and forage to at least 3p in `items.json` and keep single-piece lists. *Recommended: (a).* It
   keeps today's prices and makes T1 visible; (b) is a data change that can follow if gathering income feels thin in
   Phase 10.
5. **D5. Who buys raw goods.** For "dump iron ore" to happen a shop must buy it. *Recommended: the smithy, the
   ironworks and the foundry buy iron ore* (a data change in `crafts.json` `buys`). The ironworks and foundry already
   buy iron bars; the general store keeps its stone, clay and sand.
6. **D6. Ridgemere as markets.** (a) One market for prices, with bands ranked city-wide, the district factor and the
   district guarantees giving each quarter its flavour; (b) several (the Commons, the Wharf, the Heights).
   *Recommended: (a).*
7. **D7. Communities as markets.** They become markets for trade, caravans and bandits but stay plain for law.
   *Recommended: yes.* It answers doc 73's open question 5.
8. **D8. A ship at the pier.** (a) Ships as notices, crowds and talk only; (b) a visible ship (+4 plan hours).
   *Recommended: (a) first,* (b) later if wanted.
9. **D9. The Greyfen demo roster** (`decide()`, `RatwSociety.cpp:563-972`, used by 5 test files). *Recommended: keep
   it, without its own mint and sink (:933-969);* retire it separately later.
10. **D10. `live.business_settings`.** *Recommended: drop it* in 0061. Re-meaning it as "this shop is closed" can
    come later if hosts ask.
11. **D11. NPCs picking players' pockets.** *Recommended: out in v1.* Theft stays NPC against NPC and shops; a
    player-targeted version needs its own noticing and feedback.
12. **D12. Evictions and rough sleeping.** *Recommended: never evict;* show destitute residents sleeping rough on some
    winter nights.
13. **D13. Doc 59 (lamps and homes).** *Recommended: rewrite it later as cosmetic by band* (lit windows, hearth smoke,
    dimmer road lamps in hard times, lamplighters as an apprenticed post). Not in these phases.
14. **D14. Bandits.** *Recommended: camps scale mildly with hard times and the season, never with players' wealth,*
    with strength and bounties capped.
15. **D15. Player purses across worlds** (doc 77, Q1). Conservation no longer forbids it. *Recommended: leave it
    closed for v1;* it is a fairness question between worlds, not an economy one.
16. **D16. The faucet door.** (a) Log and alert only in play, enforced in tests through `realBalanced()`; (b) enforce
    in play at once. *Recommended: (a).* (b) would break paths nobody has registered yet. Later still, the call sites
    could move to `spend`/`earn` and NPC accounts go entirely; not needed now.
17. **D17. Smoothing.** The slider moves at most 10 points a day (a full swing in about 20 days), with a DM "now".
    *Recommended: yes.* 20 days reads well in a game week or three.
18. **D18. `artifacts/economy`.** 28 GB untracked, 34 MB tracked. *Recommended: delete the untracked runs; leave the
    tracked reports where they are as history.* The user's call.
19. **D19. PROD.** (a) Gets doc 79 when it moves to the new map; (b) gets it on the old map now. *Recommended: (a).*
    One migration path, and the old map's economy isn't worth porting.
20. **D20. Dead columns.** (a) Leave `live.economy`'s and `live.npcs`' dead columns in place, ignored; (b) drop them in
    0061. *Recommended: (a)* for now, safe on either migration path; drop them once PROD has moved.
21. **D21. Errand-runners near players.** Households' shopping trips shown where a player is near, at most 2 a cell
    and 12 a market, behind a rule. *Recommended: on.* It is the one place a player watches NPCs buy.
22. **D22. Order with doc 78.** (a) Doc 78's M0-M4 and R1 go when ready; M5-M7 wait for doc 79's Phase 9 (migration
    0061); (b) doc 78 goes first on today's table and Phase 9 absorbs the rework (+2-3 plan, +1-1.5 expected).
    *Recommended: (a).*
23. **D23. Where the welcome is counted.** (a) Once per central account per world per game week, later wolves that
    week getting 3 meals only; (b) once per character, as today. *Recommended: (a).* (b) is an unlimited faucet now
    that the treasury never runs dry.
24. **D24. Town treasuries.** (a) Keep `stores:<community>` as staged town treasuries (each set nightly between 200p
    and 2,000p); (b) fold every town into the one `treasury`, so town festival purses become realm money.
    *Recommended: (a).* Fourteen files use them; keeping them is less work and keeps towns distinct.
25. **D25. Staged events.** (a) On, 1-3 per market a week, with notices; (b) off. *Recommended: (a).* It is what keeps
    a one-player hosted world from looking dead.
26. **D26. Real carters as drivers.** (a) Yes: a carter is away from his post for each trip, and wagons are capped by
    carters; (b) the wagon stays a nameless character. *Recommended: (a).* It costs about 2-3 plan hours and makes T4
    true.
27. **D27. Regional restock.** (a) Imported goods refill only on arrivals, from a back-room buffer between; (b) every
    shop refills everything each morning. *Recommended: (a).* Without it wagons change nothing a player can see.
28. **D28. The faction discount.** (a) Keep it as coins back, capped per Chapter per week (30p); (b) pay it as
    standing instead; (c) fold it into the backstop, lowering sworn players' sell prices. *Recommended: (a).* It keeps
    the feature players know and bounds the loop to the cap.
29. **D29. Phase 0 first.** (a) Switch the loop off first and measure what breaks, then build the Board on one price
    path; (b) the first draft's order (Board beside the old loop, then bands, then the cut). *Recommended: (a).* The
    real unknowns are on the far side of the switch.
30. **D30. Phases 4-8.** (a) In sequence; (b) parallel sessions with named owners of the shared talk and roads files
    and a merge line. *Recommended: (a),* unless the user wants the calendar time back.

## Progress

- **2026-11-08: the user's decisions.** "Go with the recommendations" (all 30, D1-D30), and doc 79 before doc 78.

- **2026-11-08: Phase 0 done** (`671fe91`, branch `r2-staged`): the rule `staged` (test-only, default off), staged food
  and purses, `postKind` and `households()` moved, the pre-79 fixtures and `save_compat`/`save_compat_pg`. ctest 83/83
  with the rule off, 66/83 with it on; every failure and kept reader is in `79-phase0-failures.md`. Open: fixture (a)
  is DEV's fresh-boot save (no `loop`, steering, `fund:relief`, `outside:wharf` or `farm:` accounts); a richer one
  needs a copy of DEV's world run past a night (the user's go-ahead).
- **Perf baseline on today's code** (`cab65ee`, Release, 8 threads, DEV build 70 exported read-only, `world_check
  EXPORT --simulate A B --players 20`, 559 residents, laptop idle at load 0.4-2.0):

  | Window | mean | p50 | p99 | p99 after the first minute | worst after the first minute | worst |
  |---|---|---|---|---|---|---|
  | 6-8 | 2.58 | 2.52 | 5.79 | 5.55 | 20.97 | 31.66 |
  | 17-19 | 2.21 | 2.07 | 5.56 | 5.28 | 27.95 | 27.95 |
  | 23-25 | 1.61 | 1.48 | 4.71 | 4.02 | 9.64 | 45.21 (first minute) |

  Today's code already sits at or past section 15's bar (p99 at most 5.3 ms, worst at most 25 ms) in the 6-8 and
  17-19 windows, so the gate compares against these numbers: no window slower than this baseline beyond noise.
- **2026-11-08: Phases 1 and 2 built and joined** (each in its own worktree, checked adversarially with two repair
  rounds; branches `r2-staged-p1` `26dd898` and `r2-staged-p2` `8748a0d`; merged on `r2-staged` `561d4b1`..`84313ee`,
  then checked again: no problems left). The Market Board (`Core/RatwMarket.*`, `Data/Economy/market.json`): pressure,
  the per-piece ladder, the per-holder cap, morning ripples, depth, the clamp, I1-I5 and the backstop, shelves and the
  morning restock, the client's dozens, markers and "BUYING TODAY". Bands and staged accounts (`Core/RatwStatus.*`,
  `Core/RatwStagedAccounts.cpp`, `Data/Economy/status.json`): the score, the reasoned cut, precedence, food from the
  shelf, faucet caps and tallies, staged payers topped up, the welcome once per account holder per world per game
  week. One slider, in the Board, stepping 10 a day. Tests: ctest 89/89 with the rule off; with it on 71/89, the 18
  failures being Phase 0's 17 and `shops` (all deleted or rewritten in Phase 3); `exploit_tests` 782,451 checks,
  `market_tests` 1,685, `status_tests` 21,969, `life_status_tests` 2,190; the client 146.
  - Open for the user: the season swing against the clamp (recommend narrowing the season span); region factors
    (made here 0.95, far 1.08: bread 12p a dozen in Ridgemere, recommend keeping); the crafting cap strict until the
    Craft panel (recommend); Times still from the slider alone, so the bands' Times shift adds to the slider (for
    Phase 10's look and feel).
- **2026-11-08: Phase 3 (the cut) built, paused before its last check.** Committed `d06abb7`..`e5605a0`: the tools,
  Editor and docs cut; the cut in Core (the staged economy is the only economy; no `staged` switch); the tests for
  the one economy; `world_check` without the economy's reports; two rounds of fixes from two adversarial checkers
  (saves, money and exploits; leftovers and completeness). The user paused the session during the second round's
  re-check, so that re-check never reported. Checked by hand at the pause: a clean Release build, ctest 79/79, and
  `test_world_package`, `test_studio` and `test_map_editor` OK. **To resume:** re-run that last check (both lenses)
  on `8450efe..e5605a0`; add the running pre-79 DEV save (made 2026-11-08 at `cab65ee` from DEV's world in file mode,
  2 days 16 hours, holding `loop` and `outside:wharf`; now in
  `Tests/fixtures/pre79/running-pending/` with its README and log, 295 KB) to `Tests/fixtures/pre79/` and `save_compat`; apply the user's price
  decisions (narrow the season span; keep the region factors; the crafting cap strict); then Review A, which ends
  slice 1.
- **2026-11-08: Phase 3 built** (the cut; `r2-staged` `d06abb7`, `487f9db`, `cd5de64`, `e61b833`, then a repair round after
  two independent checks). The staged economy is the only economy: the rule `staged` is gone; the city loop, steering,
  the Wharf, farm money, the store room, minting and the wire loop are deleted; restore reconciles (dead accounts
  dropped, quarantine, read-past), never refusing a world over economy data; `world_check` keeps only the sim-speed
  flags; `conserved()` is `realBalanced()` everywhere; the world package deletes players and drops staged and dead
  accounts. The repair round: a package made before doc 79 imports again (its `live.business_settings` read past, a
  warning at the scan: `world_package.DEAD_TABLES`, with an end-to-end test on the frozen pre-79 package); a restore's
  quarantine moves are journalled at once (`Game::journalRestored`), so a crash before the next checkpoint neither
  doubles nor loses the goods; a dropped caravan's escorts reopen with its letters; Atlas's project checks warn of a
  bad dead setting instead of refusing the project; the dead `items::households()`, `SuppliesKept` and
  `StagedTreasuryMost` removed, `SellBackShare` is the Board's `SellShare`; the Core half of the business-label parity
  check is back (`shops_tests`); stale docs reworded. Tests: ctest 79/79 (three full runs; `world_behaviors`' off-thread route check now waits for the route thread, which had failed once under load); `save_compat` 17,403 checks; `tools/test_world_package.py` 43 with a server and the database, none skipped (DEV exported read only, `p3stage` throwaway worlds); `tools/test_world_sync.py` 52, none skipped, twice; the other tools' tests without the database; the client 146.
  - **Left to later phases, by owner** (section 17's rows not cut in Phase 3):
    - **Phase 9** (the studio, Atlas and the Editor): the store room's, the loop's, the comforts', the Wharf's and the
      farms' money UI and routes still work and control nothing (`studio.py`'s `economy/store`, `/businesses`,
      `/loop`, `/goods`, `/comforts` and `/farms` and their views; `map_editor.py`'s `STORE_DEFAULTS`,
      `LOOP_DEFAULTS`, the comforts, `FARM_*`, `ECONOMY_ALIASES`/`RULE_ALIASES` and their record lines;
      `model.mjs`'s store and loop defaults; `EconomyControls.tsx`'s store room and restocking rule, `WharfPanel.tsx`,
      `FarmsPanel.tsx`'s wages, schedule and scales; `item_catalog.store_goods`/`store_cost` and their tests, and
      `business_labels.json`'s `keepers`); `Data/Economy/country_farms.json`'s money fields (`defaults`, `basket`,
      `wharfBasket`, `switchTrades`, per-area `yieldScale`, `crewScale`, `priceFactor`, `wages`), which only that UI
      reads; and `/api/economy/steer`, which comes back carrying `prosperity` (Core reads no `prosperity` rule
      before Phase 9: `Society::setProsperity` has no caller outside tests). Until then the validators warn of a bad
      dead setting and write it at its default, and a change to one is still checked.
      Also Phase 9's, named here so the cut's accounting is whole (section 17's DM and Editor rows): the season
      record's shape (`economy.seasons` keeps `{supply, price, shelf, normal}` and the people manifest's `season` lines
      carry all four; Core reads only `price` and `shelf`, so `SeasonsPanel.tsx` shows supply and normal disabled
      until migration 0061 reshapes the record; its text, `RulesPanel.tsx`'s and the studio's, `map_editor`'s and
      `world_store`'s messages for the seasons, rules and services name `market.json` since the third repair round);
      `RulesPanel.tsx`'s held rules; `sync.ts`'s `economy` conflict block (:67-70); the "the economy is never
      touched/written" copy in `UpdateFromLive.tsx:82` and `ServerTab.tsx:192` (checked: still true, as
      `live.economy` is never synced; reworded when the economy's settings move into `live.economy.rules`); the
      comforts' strings, which still name `city_loop.json`'s defaults (the comforts control nothing); and
      `MoneyTab.tsx` with `dungeon_master.money()`, which become the Prosperity tab.
    - **Phase 4** (the roads): `RatwRoads.cpp`'s `stores:` loads, the two-towns gate and `sendCaravan`, which has no
      caller in play now (only tests send caravans). Its `caravan:` account is a dead prefix, so until Phase 4's
      account-free departures a caravan out on the road doesn't outlive a restart: it is dropped cleanly, its letters
      and escorts open again with their coins in escrow (`roads_tests`, `save_compat`).
    - **Phase 6** (street life): `foodDays` as the measure of wealth in troubles (`RatwTroubles.cpp`,
      `RatwGameTroubles.cpp`'s help check).
    - **Phase 8** (talk): `talkRichLine`, `richLine_`/`poorLine_` and the per-speaker `foodDays` scan with
      `wealth_days` (a fallback for a resident without a band).
    - **Data kept in `market.json` that Core doesn't read yet** (each `about` says so): `services` and `luxuries`
      for **Phase 6** (the staged sales at frequency by band, section 6.5; the Rules panel marks the services "not
      in play yet"); `wharf` (`never`, `imports`, `takes`, names only) for **Phase 5** (the ships' manifests;
      Atlas's Wharf panel reads them from `market.json` meanwhile); `houseBusinesses` and `estates` for **Phase 8**
      (talk: who owns what, a House's people; the bands find the Houses' people by `houses()` and the guarantees go
      by district since Phase 2), and Phase 9 drops either if talk doesn't take it.
  - **Second repair round** (after two more independent checks): an unregistered faucet now fails CI as section 9.4
    says. `Society::tally` counts a coin to a player as a faucet only when its reason is registered
    (`registeredFaucet`); an unregistered one is still paid and logged in play (D16), but it leaves `realBalanced()`
    false (`unregisteredFaucets()` counts it). The DM's `operator transfer` is registered. A test's own setup
    purses go through `Tests/test_purse.h` (`testPurse`: the coins taken into the gate's opening, a gate already
    broken left broken); no path of play was found unregistered across the whole ctest. `--refound-economy`
    (doc 64's Phase 1: it threw away the whole saved society, players' purses and escrow included) is deleted with
    its test, as nothing needs a re-founding now that restore reconciles. The pre-79 package's end-to-end test plants
    escrow (a resident's and a treasury's courier contracts, a bandits' hoard, as the old code wrote them) and checks
    it to the penny after a real server played it. `crafts.json`'s dead `tools`, `upkeep`, `farmUpkeep` and
    `institutions`, `pendingJournal()` and `items::kindsOf` removed; the data files' `about`s and the tests'
    comments reworded.
  - **Third repair round** (after the re-check of both lenses on `8450efe..e5605a0`): `test_studio`'s city loop test
    asserts the empty basket the Economy view gives without `city_loop.json` (the earlier "OK" had skipped every
    database test); `test_world_db.database_available()` takes the database's settings from the environment as well
    as `Database/.env` (a worktree has no `.env`), and `test_world_sync`, `test_live_map` and `test_one_server` take
    `RATW_TEST_SERVER` as `test_world_package` does, so a worktree's run reaches the database instead of skipping;
    that showed five more of `test_world_store`'s database tests failing since the cut, which now state the lenient
    economy (a bad dead setting loads, warned of, and a change to it is refused; a rule the game no longer has is kept,
    warned of and never written; the round trips use the staged economy's rules), and `test_live_map`'s Dev Console
    check, stale since commands were added before doc 79, lists every command (it now skips without a built client);
    the Editor's and the client's typechecks pass;
    `Craft::hours` and `Producer::hours`, their gate and `crafts.json`'s 149 `hours` keys removed (nothing has read
    them since the city loop went), with `item_catalog`'s checks of them and `ladder.json`'s calibration settings
    (`markup`, `skillWage`, `carryPerWeight`, `foundingDay`, `costAtPace`; only `catalogueDay` and the rungs are read);
    the seasons', rules' and services' messages retargeted at `market.json`; stale comments, the README's economy
    bullet, `country_farms.json`'s dead `listedIn` entries and UI hints reworded. Tests: ctest 79/79 (twice); all 34
    `tools/test_*.py` with the database and the worktree's server, none skipped but the `RATW_SLOW` world builds and
    `test_game_tables`' saves under `Saved/` (`test_world_package` 43, `test_world_sync` 52, `test_world_db` 49,
    `test_studio` 49, `test_world_store` 24); the Editor's 81 + 64 and its typecheck.
  - **Fourth repair round** (2026-11-10, after the paused re-check of both lenses was run again on `5dd5d51`: no money
    exploit, no weakened test, eight low problems). The running pre-79 DEV save is fixture (h),
    `Tests/fixtures/pre79/running/` (its log kept beside it), in `save_compat`'s list: decoded, restored against DEV's
    roster, round-tripped, the whole save into the world, the one pass's classes, through Postgres and on the full
    world; `theRunningSave` checks it holds the loop's state, `outside:wharf`, the estates and tills, and that the Wharf's
    and estates' accounts are dropped and the tills kept. A file-mode server wrote it, so `--check-loaded` compares the
    checkpoint by value, not as text (jsonb keeps no key order; the dev and dev-old fixtures came from Postgres already
    in its order). `save_compat_pg`, `market_pg` and `pg_client` exit 77 when skipped and ctest lists them as Skipped
    (`SKIP_RETURN_CODE`), so a "79/79" no longer counts a skipped Postgres test as passed. `facilityPrefixes()` no
    longer holds the old economy's `fund:`, `bank:`, `outside:`, `farm:`, `crew:`, `estate:` and `house:`, so
    `openAccount` refuses them (classify still calls them Dead, so a save's are dropped); `caravan:` stays until
    Phase 4; `status_tests` checks every Dead row of its table can't be opened. `newcomer_behaviors`' flake (an empty
    innkeeper reply when the machine is loaded) was the test's game clock following the wall clock: a reply landing in
    the first tick sent the next post within the chat's half-second limit ("Please leave a moment between posts"), and
    fewer ticks left the master stepping in unseen ("in sight: u1"). Each answer now gets the same two game seconds,
    then waits for a late reply with the clock all but still, and the test's worlds run by counts
    (`World::setDeterministic`); 30 runs out of 30 passed beside 40 busy loops on 20 cores (before, several of every ten such runs failed).
    Stale comments reworded (`farm_people_tests`' header, `test_studio`'s dead rule, the DAIRIES note,
    `economy.sync`'s and the manifest's economy records). Tests: ctest 79/79 three times (once without the database,
    three Skipped; twice with it and a throwaway `pg_client` database, none skipped); `save_compat` 23,084 checks, and
    23,100 with `RATW_PRE79_BUILDS` (dev, dev-old, package and running loaded whole); `save_compat_pg` 5 saves through
    Postgres; all 34 `tools/test_*.py` with the database and the worktree's server, none skipped but the `RATW_SLOW`
    world builds (`test_realm` 7, `test_realm2` 16) and `test_game_tables`' save under `Saved/` (`test_world_package`
    43, `test_world_sync` 52, `test_world_db` 49, `test_studio` 49, `test_world_store` 24, `test_dm_live` 1 with
    `RATW_SERVER`). **To resume:** the user's price decisions (narrow the season span; keep the region factors; the
    crafting cap strict), then Review A, which ends slice 1.
  - The client keeps its trade view for servers from before doc 79 (PROD is one), commented as such.
  - **Open for the user:** marking the memory pointers to the conserving economy as history (section 19's Phase 3
    row; it needs the user's say-so, so nothing was changed); `artifacts/economy` (D18).
- **2026-11-10: the user's price decisions applied** ("go with your suggestions", 2026-11-08, on Phase 2's three
  open questions), and the running pre-79 DEV save confirmed in place.
  - **The season span narrowed** (`market.json` `seasonSpanMost` ×1.4 to ×1.06; Core's default too). The rule the
    user took: at a season's ends, with each region factor, players' pressure always keeps at least ×1.10 of room up
    and ×0.90 down inside the fixed 0.80-1.28 clamp, for every season good. At ×1.4 it held for none of the swinging
    groups: fruit's winter far from its orchards (1.22 × 1.08 = 1.32) sat past the clamp, so players' buying moved
    nothing, and grain's cheap autumn where it is made had under 1% of room down. The binding group is `preserved`
    (90-100, all its swing on the cheap side). At ×1.06: room up at least ×1.136 (honey's winter, far) and down to
    ×0.893 (preserved's winter, made here). The swings are now small: grain 0.969-1.027, fruit 0.977-1.036, fish
    0.964-1.022, dairy 0.980-1.039, honey 0.985-1.044, preserved 0.943-1.0 (the shelves' abundance, `shelf`, is
    unchanged). The rescale moved into `market::seasonFactor` (the Board's `stagedSeason` calls it), and
    `market_tests`' new `seasonsLeavePressureRoom` pins the rule over every season group's goods, every half day and
    each season's middle day, at made 0.95, near 1.0 and far 1.08; with ×1.4 it fails (checked). `seasonsFitTheClamp`
    also checks the Board's fruit price follows `seasonFactor` day by day.
  - **Not inside the rule**, left as they are (each a narrow move of its own on top): the Sump's ×0.95 (with made
    here, preserved's winter reaches 0.851, room down ×0.94) and the Times' ×1.08 (a far staple in hard times is
    1.166 before the season, room up ×1.098). Bringing them in would leave the seasons no swing at all (×1.025); it is
    a question for Phase 10's look and feel, with the bands' Times shift. A world's own season records (the DM's
    Economy tab) are rescaled the same way, but an off-centre record (all 130, say) isn't held to the rule: Phase 9's
    validation.
  - **The region factors kept** (made here 0.95, a neighbour's 1.0, far 1.08; now pinned by the same test).
  - **The crafting cap kept strict** (I5, 1.6 × the inputs' cheapest buy price; `exploit_tests`' `theCraftingCap`),
    until the Craft panel.
  - **The running pre-79 DEV save** is fixture (h), `Tests/fixtures/pre79/running/`, in `save_compat` since the fourth
    repair round (`38c8118`): no players and no escrow (its row in the fixtures README, all 0, checked exactly), restored
    against DEV's roster without refusing, the loop's state read past, `outside:wharf` and the estates dropped, the
    tills settled, round-tripped through wire, the checkpoint and Postgres. Nothing more was needed.
  - Tests: a clean Release build (`build-f`); ctest 79/79 with the database (none skipped: `pg_client` run with a throwaway
    `s1stage` database, dropped after); `market_tests` 1,940 checks, `exploit_tests` 782,489, `save_compat` 23,084; all 34
    `tools/test_*.py` with the database and the worktree's server (`RATW_TEST_SERVER`, `RATW_SERVER`), none failing,
    skipped only the `RATW_SLOW` world builds (`test_realm` 7, `test_realm2` 16) and `test_game_tables`' save under
    `Saved/`.
  - **To resume:** Review A, which ends slice 1.
- **2026-11-10: Review A's kept findings fixed** (eight, each kept by at least two of three skeptics), each with a test:
  - **The shop list's cost.** I3's backstop is worked out once a row (`market::leastSplits`: the lot of each k once,
    then the least of two halves in whole pennies, not ~4,900 `boughtLot`s a call), the halving search reads that one
    table, and a piece "Not worth the counting" hands the shop list the lot its refusal found, so the search runs once a
    row, not twice. A general store's whole list: 1.18 ms to 0.41 ms a snapshot (Release, this laptop; provisioner 0.37
    to 0.15, apothecary 0.34 to 0.18, herbalist 0.33 to 0.10), every row identical. `exploit_tests`' `theBackstopTable`
    checks the table against the plain search for prices 1-250p across the clamp and that the list's smallest sell lot
    is still the least worth a penny; `game_load --layout shops` keeps players beside the keepers so the perf run sees
    the merchant block (`world_check` measures `World::snapshot`, which has none).
  - **The clocks after a restart.** `World::restore` sets the society's clocks from the calendar
    (`Society::resumeAt`: the Board's and the season's), so a trade before the first decide reads the saved day's
    budget, take, sell total and P, and is stamped now, not at day 0. `market_tests`' Board-through-the-game check (c)
    loads the save and trades before any tick.
  - **The "short" trouble by band** (6.7's gate brought forward): only a poor or destitute head is short; a comfortable
    one at the bottom of its purse range no longer says "Money's tight" and takes help the night's drift undoes.
    `life_status_tests`' `noShortTroubleAboveThePoor` (with every purse empty, and over a week of staged nights);
    `troubles_tests` and `storyline_tests` make their short household poor, and check a comfortable and a rich one
    with empty purses aren't short.
  - **A short day is half a day's food** (6.5): half the band's meals, at most half a day's need, so a run of the
    poor's short days now shows as hunger (`poorShortDaysShow`); destitute days are as before (one loaf). The bands'
    meal counts stay Phase 10's.
  - **The ripple's cause** names the ripple only where it leaves P on its own side; one that eats into a players' move
    keeps "travellers buying" (`market_tests`' `aRippleAgainstPlayers`).
  - **"That's more than I lay out in a day."** for a lot past the whole day's budget (a theriac at the apothecary at
    dawn); "spent all" only once something was bought.
  - **The keeper's trade answer** names two of the shop's own wares on its shelf at the price its list shows (the
    dozen for a good under 3p), meals and herbs first where it deals in them; "out of" only for those it deals in
    (`theKeepersAnswer`: a smith's answer). Phase 8 still writes it whole (7.8).
  - **Ripples and the restock:** the dawn pass stays (P is per market and shared by its shops; per-till ripples would
    double-apply or need new saved bookkeeping), and a keeper whose wares the dawn rippled is seen setting out the
    wares at the morning restock though no shelf needed filling (`rippled_`, not saved: after a restart the restock
    shows only what it sets out). Between dawn and opening an awake keeper away from the counter still quotes the new
    price: left as it is.
  - Tests: a clean Release build (`build-x`); ctest 79/79 with the database (none skipped: `pg_client` on a throwaway
    `s1stage` database, dropped after); `exploit_tests` 871,756 checks, `market_tests` 1,969, `life_status_tests` 2,244;
    each new check seen failing with its fix taken out; all 34 `tools/test_*.py` with the database and the worktree's
    server (`test_world_package` 43, `test_world_sync` 52, `test_world_db` 49, `test_studio` 49, `test_world_store` 24),
    skipped only the `RATW_SLOW` world builds and `test_game_tables`' save under `Saved/`; the client 146.

- **2026-11-10: Phase 4 (the roads) built** (`r2-staged` `a6bafd5` and the fixes after it; `Core/RatwTradeShow.cpp`
  beside `RatwRoads.cpp`). Slice 2 begins.
  - **Markets drive the road layer; law stays on `towns_`.** `World::roadMarkets()` are the Board's trade points (5.1)
    with where wagons load and unload (the busiest merchants' place and a merchant's spot there) and their treasury as
    their store; a cell's market is its community's or the one serving its homes (`roadMarketOf`). The road layer's uses
    of `towns_` moved (by grep): `tendRoads`' two-towns gate (now: any market; carts need one, wagons and bandits two),
    `roadRoutes_` (each market's nearest by path, both ways once) and the camps along them, `contractsNear`,
    `residentsTakeWork`, the NPC courier letters' two ends, the courier errand, the bounty patrols' guards, the robbery
    rumours, the escorts' and carters' rumours on arrival, and `Game::courierCells`. `lawTown`, `townOf`, `arrivalIn`,
    the squares, the projects and the newcomers stay on `towns_`. Communities are markets on the roads while plain for
    law (D7; `roads_tests`' `communitiesStayPlain` now says so).
  - **Committed, saved departures.** The Board gains `committed` (wagons posted two days ahead, and the morning's carts),
    `rolledThrough`, `nextDeparture`, `backRoom`, `harvestOverride`, `troubles`, `timesShift` and `news`, each written
    only when it has something and read leniently (unknown goods dropped, numbers clamped; a departure to a market that is
    gone dropped on load with its notices). Each ordered route rolls `hash(route, day) < frequency` once, at posting:
    city to a community every 4 to 2 days by the community's size (catchment 30 or more: 2), between communities 10 to 6,
    × 0.6..1.4 by the slider, × 0.8 in winter, × 1.5 when the route's biggest P gap is over 0.2, × the rule `caravanRate`
    (percent). A posted wagon keeps its promise through a DM change (`trade_show_tests`: `caravanRate` 0 after posting).
  - **Real carters (D26).** A carter is a held post of the carters' trade (`businesses.json` `carters`, or a title with
    "carter", "wagoner" or "carrier"), the market's where the post is, else where its holder lives. Wagons go only from a
    market with carters, at most one a carter a day and `wagonsOnRoad` (16) on the roads; the posted carter drives, else
    another of the yard's free; with none (or the roads full) the wagon is set back a day with its cause on the notice
    ("Set back: no carter to be had."), and given up after five days. The carter's errand is the wagon (`World::drivers()`;
    "on the Coldwater run — driving the wagon", "driving the wagon home to Ridgemere"), followed every pass, so he is
    away from his post for the trip; the wagon loads at his yard and waits two game hours for him and its escorts. The
    `errand` talk fact reads it as `caravan`.
  - **Manifests** (8.1): up to 5 goods, each one the destination's shops set out or buy (`TradePoint::bought` is new):
    the source's glut first, then what its workshops craft that the destination's don't, the largest P gaps, a luxury for
    a rich household, then the rest it sells; counts from the destination's shelf targets by the wagon's size (1-3). No
    account: an older save's wagon (its `account` the old economy's) comes home empty, its letters and escorts open
    again (`save_compat`'s road fixture now says so, to the penny).
  - **Nudges at visible moments** (7.4, 8.6): as it loads, the source's glut on its goods halved toward 0 and its tills'
    stock of them over target sent on (`Society::sendOn`); as it arrives, `ΔP = −0.2 × max(0, P_dest − P_source at
    departure)` (never up), the destination's shelves of its goods refilled and their back rooms filled, the keepers
    seen setting out the wares (`Society::arrivalRefill`), and `world.news` (and `world.news_from`, new facts) set to the
    source's top cause ("iron ore is cheap in Ridgemere", or its Times) for four days.
  - **Late with causes:** 8% of wagons meet a seeded mishap on the way (a broken axle, mud to the axles, a wheel off in
    a rut, a washed-out ford) that holds them 1.6-2.4 days; a storm's wait is "the weather". Due half a day out, a wagon
    a day past due is posted overdue with its cause (the far end's notice reads "Overdue: ...") and adds +0.05 a day on
    its goods there, at most +0.2; a late wagon is a trouble for that market's Times.
  - **Raids** (8.1, D14): no coins into the camp; the manifest becomes goods at `bandits:<camp>`, made there (a goods
    faucet, at most 40p of them by catalogue price), which whoever breaks the camp finds as a reward ("reward: recovered
    cargo", within the day's rewards cap; what the cap or a full pack leaves goes back to its carters, consumed as
    "recovered cargo"); the watch's clearing or a starved camp's scattering returns them to the staged side. +0.15
    on its goods at the destination at the raid, a trouble for its Times; the carter comes home robbed (his staged purse
    emptied) or hurt (bruised ribs), never killed. The bounty and escort are posted by the destination's treasury, the
    bounty at most `bountyMost` (60p); a camp's strength at most `campStrengthMost` (12, enforced each day); a road's camp
    gathers at `campGatherOdds()`: 0.34 × 1.25 at −100 to × 0.8 at +100, × 1.1 in winter, never by players' wealth.
  - **Dropped caravans and orphaned contracts** (`World::dropOrphanedRoads`, at every restore once the markets are
    found): a caravan whose markets are gone is removed, its letters reopened at its origin (or refunded to the writer
    with the origin gone too), its escorts reopened (or their pay back); open contracts whose town is no market, town or
    region any more refund their posters.
  - **Farm carts** (8.2): each staffed farm or works with its work room in the world sends a cart each morning to its
    community's market (else the nearest by path), its carter the farm's keeper or a hand ("taking the cart to East"),
    one way. Its area's harvest (poor, fair, good) is a seeded roll per area per season leaning with the slider, or the
    DM's override in the Board; it sets the cart's chance (40/70/100%) and load (×0.6/1/1.5, ×1.5 on Marketday), and on
    arrival pulls the cart's goods halfway toward its baseline (+0.15/0/−0.15), only down. A cart that doesn't come is a
    visible bump (+0.05 on its goods at its market, cause "harvest"). At most `cartsOnRoad` (48) carts at once; a cart
    from a day gone by (after a restart) never comes. Times (5.3) take each market's harvests (±15, averaged over its
    staffed farms' carts), raids (−10 each, at most −30) and late wagons (−5 each, at most −15) of the last 7 days, as
    `timesShift`, set each morning.
  - **Marketday stalls from carts:** a Marketday cart stocks its goods over the everyday target (×1.5, the stalls'
    stock; "cart stocks the stalls"), which the next wagon out sends on.
  - **Regional restock and the back room (D27).** A market's imports are the goods its shops set out that its
    workshops don't craft and its areas don't bring in (`TradePoint::crafted`, new: what a shop only supplies, a general
    store's nails, is no market's own make here, though 7.1's region factor still counts it as made) and that a route a
    carter can drive to it, or a staffed farm's cart, brings. Those refill only on arrivals, and between from the till's
    back room (filled to a shelf's worth on an arrival, a fifth less each morning; a back room never stocked yet is
    stocked at its first restock, so a world's first morning isn't bare). Every other good refills each morning, so a
    market no wagon or cart reaches (one market, every carter deleted) never runs bare.
  - **Notices with weekday names:** each wagon posted is pinned on both ends' boards by "the carters" (`Game::
    roadNotices`): "Wagon to West, Marketday: nails for the general goods store, and flour. Tam Reed driving. Escort wanted."
    and "Wagon from East, due Marketday: ..."; changed when set back or overdue, marked "Robbed on the road" at a raid,
    taken down on arrival. The caravan topic gains `topic.what` (its first good), `topic.who` (its carter) and a `late`
    kind.
  - **New rules** (`Core/RatwItems.h` and its mirrors in `market.json`, `map_editor.py`, the Editor's `economy.ts` and
    `model.mjs`): `caravanRate`, `wagonsOnRoad`, `cartsOnRoad`, `campStrengthMost`, `bountyMost`.
  - **Checked on DEV's data, read only** (DEV build 70 exported on 2026-11-08 and the running pre-79 save, loaded and
    ticked two seconds, nothing written): Ridgemere is the one market (its 7 carters; the Country's communities have no
    households yet in that save, and no farm is staffed), so no wagons or carts run and nothing waits for one; the old
    map's dev-old save gives six markets and its camps, with no carters. Both full worlds load in `save_compat` with
    `RATW_PRE79_BUILDS`.
  - **Left for later, by owner:** ships (Phase 5; until then a good only ships would bring refills each morning, as it
    has no visible supplier); staged events (Phase 7, and 16.5's "staged events post with their cause"); the talk lines
    for wagons due, late and their news, and `topic.kind` "due" (Phase 8: the facts are there); the DM's harvest
    overrides, the week's wagons on the Prosperity tab and `caravanRate` in the Rules panel's copy (Phase 9; the rule is
    already listed); the town projects' watchpost and waystation proposals still read the law's towns, of which Ridgemere
    2.0 has none (they are Town Works, not the roads: for Phase 9 or a later plan); the perf gate's windows and the soak
    with wagons and up to 48 carts (Phase 11, with the user's go-ahead: not run). (Corrected by the repair below: the
    departures are journalled, with the whole Board on every economy record; it was the roads beside them that weren't.)
  - Tests: `trade_show_tests` (new, 228 checks: committed and kept through a DM change, real carters away from their
    posts and set back without one, manifests, notices and weekday names, the departure's and arrival's nudges and never
    up, the shelves refilled, `world.news`, a raid's +0.15, cargo at the camp and found by the player who breaks it, the
    carter robbed or hurt, the bounty and strength caps, late with its cause and +0.05 a day to 0.2, imports thin
    between wagons and refilled on arrival, a market no wagon reaches refilling each morning, farm carts by harvest with
    their baseline and Marketday's stalls, no staffed farm no carts, an endpoint deleted mid-trip, D14's mild odds, the
    notices on the boards through the Game); `host_edit_tests` (new, 514 checks: the sixteen edits of 16.6, each
    loading, staging and refusing nothing); `roads_tests` 407 (wagons with manifests outlive a restart; Coldwater a
    market); `save_compat` 23,085 (23,101 with `RATW_PRE79_BUILDS`). ctest 81/81 with the database, none skipped (a
    throwaway `ratw_test_s2stage_*` database, dropped after); all 34 `tools/test_*.py` with the database and this build's
    server (throwaway names tagged `s2stage`), none failing, skipped only the `RATW_SLOW` world builds and
    `test_game_tables`' save under `Saved/`; the Editor's 81 + 67 and its typecheck.

- **2026-11-08: drafted.** From seven code maps, three designs and three judges' grafts. Plan only.
- **2026-11-08: revised** after four reviews (see Review). Plan only.

- **2026-11-10: paused during Phase 4's first repair round** (the user). Phase 4 (the roads) is built and committed
  (`a6bafd5`..`6d5e188`); its two independent checks found 16 problems and 11 missing items, saved in
  `79-phase4-checks.json`. The repair round's partial work is on branch `r2-staged-wip-repair4` (may not build).
  **To resume:** repair Phase 4 from those checks (starting from the WIP branch or afresh), re-check with both lenses,
  then Phase 5 (the port), then Phases 6-8 in sequence.

- **2026-11-10: Phase 4 repaired** (from `79-phase4-checks.json`, starting from the WIP branch's work). All 16 problems
  fixed, each with a test; of the 11 missing items, 4 built, 6 deferred to their phases as planned, 1 not code (below).
  - **Wagons:** the source's player-sold glut (a good its shops only buy: T3's ore, a stall's berries) goes out first,
    at most two goods of it. A market's imports by wagon from one source are at most 10 (its own makes first, then a
    seeded, stable choice); the rest refill each morning. Each manifest then takes the far end's thin imports, those
    not already on their way first and those brought longest ago next (the Board's new `broughtIn`, saved, kept 60
    days), so each import is on one of every three wagons at worst. While the DM's `caravanRate` or `wagonsOnRoad` is
    0 nothing counts as imported by wagon, and while `cartsOnRoad` is 0 nothing by cart, so those goods refill each
    morning (D27); the import list is found again when the rules change.
  - **Boards and notices:** a market plain for law (D7) has its board at its market's place
    (`Game::boardTownFor`: the law town's, else the road market's), and the carters pin each notice on the board a
    player at that market reads. The carters' notices don't count toward a board's 30 pins. The escort is taken at
    the board where the wagon sets out; the far end's notice says "Escort wanted: ask at the board in East." Goods
    are in plain words on notices, news and `topic.what` ("nails", never "nails (bag of 50)"; "nails are cheap", "cheese
    is"; "the general store"). Overdue with no known cause reads "Overdue: the wagon from East. No word of it yet." A
    wagon lost on a closed road or dropped records its departure and its notices come down; a posted wagon whose road
    a host closed is cancelled ("the road is closed"). A lost wagon's escorts reopen (or are refunded if the market is
    gone) and its carter is freed. A wagon's look names its guard (T4: a guard rides with it; no guard walks beside
    it unless an escort is taken).
  - **Carts:** a cart the roads have no room for (`cartsOnRoad`) doesn't go and bumps nothing. A rolled cart nobody at
    the farm is free to drive, or with no way through, is missed with its own cause ("short-handed", "road closed")
    and a "cart missed" event. A missed cart's bump is its share of +0.05 (+0.05 for a good only when all of its carts
    to that market miss), so many farms don't stack. The harvest leans a little with the season (autumn +0.08, summer
    +0.04, winter −0.08 on the roll). `world.harvest` (poor, fair or good: the mean harvest of the staffed farms whose
    carts come to the speaker's market, the same word that loads them; T11) is set in talk and listed in
    `facts.json`; its lines are Phase 8's. Camps gather with the far market's own Times shift as well as the slider
    (D14, 5.3), still within ×0.8 to ×1.25.
  - **The journal:** the Phase 4 entry above was wrong to say departures aren't journalled. The Board, with its
    `committed`, is journalled on every economy record, but the roads beside it weren't, so a crash between
    checkpoints could lose a wagon that set out (or run it twice). Now the roads go with any record whose departures
    moved, the Board with any roads record, and at each checkpoint the journal's shadow of the Board, its departures
    and the roads is reset to what the checkpoint holds (it held only the accounts before). `sendCaravan` copies its
    two markets before `ensureRoadMarkets` can rebuild them (a dangling reference in tests and the Dev Console).
  - **Not built, by owner:** ships, the Wharf's crowd and `world.ships` (Phase 5), and deleting a port with them; staged
    events with causes (Phase 7); the talk lines for wagons due, late, news and the harvest, and a "due" topic kind
    (Phase 8); notices for the DM's harvest overrides and the week's wagons on the Prosperity tab (Phase 9). **Not
    code:** on DEV as it stands, Ridgemere is the only market and no farm is staffed, so no wagons, carts, notices or
    imports run there, and everything refills each morning. T4, T5, T11 and T14 show only once the Country's
    communities have households with merchants and carters, or staffed farms. That is world data (or Phase 10's
    scratch play), not this phase's code, and NPCs are never generated to fill it.
  - Tests: `trade_show_tests` 475 checks (was 228: the glut on the next wagon and halved; every import in rotation;
    the rules at 0 refilling each morning and back; a plain market's board read by a player, with 32 carters' notices
    and a player's still pinned; the escort at the source; plain words; overdue with no word; carts not rolled for want
    of room, missed with their cause, two farms' bump shared; the harvest fact; the far market's Times on camps; a lost
    wagon's notices down and its escort reopened; a journal replay with the wagon on the road exactly once).
    `host_edit_tests` 550 (was 514: a road deleted with a wagon on it and one posted). ctest 81/81 with the database, none skipped
    (`save_compat_pg`, `market_pg` and `pg_client` run; a throwaway `ratw_test_s2stage_4r` database, dropped after; the
    Python pg tests' own names tagged `s2stage`); `roads_tests` 407, `save_compat` 23,085. Python: `test_dungeon_master`
    (47, it reads `facts.json`), `test_npc_mind` (31), `test_ambient_library`, `test_world_store` (24) and
    `test_world_package` (its 12 server cases skipped: they export DEV, not run here) pass.

- **2026-11-10: Phase 4, second repair** (three problems an independent check of the repair found, each with a test
  that failed before).
  - **A crash replayed a wagon's arrival, raid or lateness twice.** The roads were journalled only with a roads record
    or when the departures moved, but the world's tick moves the Board and the accounts beside them (a wagon in, robbed
    or overdue, a camp gathered, a bounty posted, the roads' day), and the next trade journalled those alone. Now the
    roads go with any economy record once they have moved a step since they were last written (everything but a
    wagon's place along its way, which goes with the next step or the checkpoint); a roads record carries the economy
    (a contract never without its pot); and every record carries the calendar, so a replayed morning isn't turned
    again (and a day turn, a trade and a crash no longer leave a save refused as "budget in the future"). Not
    covered, none of it a double effect: the carters' notices are documents kept by checkpoints (stale after a crash
    until their week is out); a record written by an older build carries no calendar.
  - **An escort went with whichever wagon to its town set out first.** An escort now names the market its wagon sets
    out from (`from` on the contract, saved; an older save's has none and keeps the old rule): the wagon's launch, the
    town guard's taking it after two days, the board where it is offered, the notices, a lost wagon's reopening and
    the arrival's pay all match the road, one escort a road. Still taken at its town's own board as well. One taken
    isn't put on a second wagon of its road while out with the first; one whose source market a host deletes is
    refunded at load.
  - **Cargo found at a broken camp was in no tally or cap.** It is a reward at the door now ("reward: recovered
    cargo", by its catalogue worth, to the player's holder), found only within the day's rewards cap, before the
    bounty, which is held to the room left; what is over the cap goes back to the staged side ("goes back to its
    carters").
  - Tests: `trade_show_tests` 566 checks (was 475: four crashes, each after a trade (set out, arrived, robbed, late),
    restarted from the file and the same again through Postgres by the new `trade_show_pg`, 29 checks, in a throwaway
    `ratw_test_s2stage_*` database; East's escort beside North's wagon to the same town; found cargo counted and
    capped). A clean Release build (`build-4r2`); ctest 82/82 with the database, none skipped (`pg_client` on a
    throwaway `ratw_test_s2stage_4r2`, dropped after); `host_edit_tests` 550, `roads_tests` 407, `save_compat` 23,085.
- **2026-11-10: Phase 4, third repair** (what two checks of the second repair found still open, each fixed with a test
  unless said otherwise).
  - **A crash while a snapshot was taken and not yet stored (or after one failed) replayed a wagon twice, lost one, or
    left a contract without its pot.** The journal's shadow was moved to the world's state when a snapshot was
    captured, without a record, but the store holds the checkpoint before until the new one lands (a forked child or
    a database's worker writes it, and may fail): a trade in between was journalled as a change from a state the
    store didn't have. Now what the world's tick moved since the last record (the accounts, the Board and its
    departures, the roads, the staged escrow sources, holds and bands) is journalled as a record of its own as the
    checkpoint is taken (`journalCheckpoint`, by the parent before the fork, before the store is handed a snapshot,
    and in `save()`), and the checkpoint covers that record. The journal is then true over either checkpoint; no
    shadow is reset without a record any more. The same record erases a staged escrow source added on a tick before
    a checkpoint and removed on a tick after it, which the journal used to leave behind (an older fault).
  - **An economy record cost about three times as much with wagons on the road.** The roads' steps were found by
    encoding the roads as a document, copying it, erasing each wagon's place and dumping it, with every record. They
    are compared as bytes now (`checkpoint::roadSteps`), and the document is made only when a step has moved. A
    notice pinned with 60 wagons out: 0.08 ms (0.97 after the second repair, 0.32 before it); with 200: 0.11 ms
    (2.99, 0.87). Measured, not a test.
  - **Words and events told the posted reward where the day's cap paid less.** `settleContract` returns what was paid;
    a bounty, an escort's pay and a letter's say so ("you are paid 7 pennies of the 20 ...: you have had all the
    rewards one day pays, and the rest goes back", or that all of it goes back), and a "contract done" event carries
    the coins paid.
  - **An older save's escort (no road named) kept the any-market rule.** It takes its road where it is taken (the
    market a wagon to its town sets out from) or from the first wagon it goes with; one a player holds while standing
    in another market waits for that market's wagon, and one whose holder is away from the world is kept. Still as
    before, by intent: an older escort taken at its town's own board, or held out on the road, goes with the first
    wagon to its town, which tells its holder where it waits.
  - **The carters' pinned notices went on asking for an escort that was taken, and never asked for one posted after
    a raid.** They are written again from the Board when an escort is posted, taken or settled (an escort's road is
    given to `postContract`, so it is known when its posting is told), and a wagon that has set out stops asking.
  - **A finder with a full pack (64 kinds) left cargo at a broken camp, its account open.** What cannot be taken goes
    back to its carters like what is over the cap, and the account closes.
  - Small: Phase 4's entry above (Raids) names found cargo as the code does; `save_compat_pg` and `market_pg` name
    their throwaway databases `ratw_test_s2stage_*`.
  - Tests: `trade_show_tests` 830 checks (was 566). Its crashes are ten now: the four, a camp scattering with its
    bounty, and each again with a player in before the checkpoint, a real forked snapshot that cannot be written, a
    purchase (the shops shut, the Board moved as one moves it) and a bounty taken where one is open, then the crash; one
    more on a store that is handed a snapshot and doesn't store it. With the old silent reset put back, the scatter
    case, four of the five window runs and the held snapshot fail (the wagon in twice, a taken bounty without its pot, a
    late wagon's push lost, a source without its pot); the fifth window run (a wagon setting out) passes either way: its
    road takes another step inside the window, which journals the roads whole. All ten go through Postgres
    (`trade_show_pg`, 100 checks, was 29). A clean Release build (`build-4s`); ctest 82/82 with the database, none
    skipped, run twice (`pg_client` on a throwaway `ratw_test_s2stage_4s`, dropped after); `host_edit_tests` 550,
    `roads_tests` 407, `save_compat` 23,085.
  - For Phase 5's merge: ships are journalled by adding them to `record()`'s comparison (and a new flag, if they get
    one, to `journalCheckpoint`'s); there is no per-field shadow reset to extend.
- **2026-11-10: Phase 5 (the port) built** (branch `r2-staged-p5`, cut from `r2-staged` at `dfdd566`, commit `49f3f3d`;
  `Core/RatwPort.cpp`, new). Ships are a committed timetable, never seen (D8: notices, crowds and talk, no ship at the
  pier). Phase 4's stopgap (a good only ships bring refilling each morning) is gone wherever a port's ships run.
  - **A port** (8.3) is a market with posts whose titles carry the Wharf's job words (the sea trade's words in
    `Society::postRules`, and the harbour's heads), found again whenever the road markets are (`World::findPorts`):
    by the market the post works in, else its holder's home's, else the nearest by path. The posts make the port,
    held or not; the residents who hold them are its dock hands. No such posts, no port, and nothing below exists.
  - **The timetable** is on the Market Board (`market::Board::ships`, with `shipsThrough`, the week each port is
    rolled through, and `nextShip`), so it is saved, restored and journalled with the Board like the roads' committed
    departures. Each morning every port's weeks are rolled through the next, so the next seven days are always
    committed; a week is rolled once and a posted ship never changes its day, hour, name or cargo. A week has the
    rule's count (`shipsPerWeek`, new: 0 to 6, default 2), one more or fewer by a score seeded by week and port that
    the slider and the season lean (winter −0.3, autumn −0.05, summer +0.1), so 1 to 3 at the default and fewer in
    winter; 0 is none, even with a port. Days are seeded (never Restday while another day is free), docking between
    7:00 and 10:00 and sailing 8.5 to 10 hours later (by 19:30). Names come from `market.json`'s new `wharf.ships`
    (16; no two of a name on a port's timetable at once). A port's timetable holds at most 16 ships (`ShipsAPort`),
    the Board's 256.
  - **Manifests** come from `market.json`'s `wharf` block: a ship brings up to 4 of the port's ship goods and takes up
    to 3 of the `takes` the port's shops sell, buy or make, both in rotation by the ship's number, a shelf's worth each.
  - **Ship day** (8.3, 8.6): at its hour the ship docks ("ship docks"). Each good it brings is pulled halfway toward
    the ship baseline (P −0.1) at the port's market, only down (cause "ship"); each good it takes halfway toward +0.1,
    only up. Every good the port waits for ships for refills, shelf and two shelves' worth in the back room. The Wharf's
    posts stay at their work with the ship's name on it ("unloading the Gull", "tallying the Gull's cargo", "seeing to
    the Gull's cargo"; reason "A ship is in."), and every cell holding a Wharf post reads `here.crowd` "crowd" while a
    ship is in. That evening it sails ("ship sails"): what the port's shops hold over their shelves of the goods it
    takes goes with it (`sendOn`, 7.5). A restart at any hour catches up from the saved timetable.
  - **A stormbound ship** (seeded by ship and day: 15% in winter, 6% in autumn, 3% otherwise; or called through
    `World::stormShip`) never comes: +0.1 on each good it was bringing (cause "ship late"), a "late" trouble for the
    market's Times, and "is stormbound and will not come", with "No ship this week." where no other came or is due
    that week. Kept on the timetable three days, a sailed ship one.
  - **Imports (D27):** a port's ship goods are the `wharf.imports` (less `never`) that its market's shops set out and
    its workshops don't craft. While ships run they refill only when a ship docks, with the back room between, exactly
    as wagons' and carts' imports do. With the rule at 0, or no port, they refill each morning as before.
  - **The notice** (`Game::portNotices`): one notice a port, signed "the Wharf office", on the board a player at the
    port's market reads (`boardTownFor`, as the carters' notices): "Ships at Ridgemere. The Gull is in, unloading
    butter, and sails this evening with handbell and hide. Due: the Heron on Marketday with butter, taking tallow."
    (goods in Phase 4's plain words, `goodWords`, which are singular for some: Phase 8's wording). Posted again
    whenever its words change and each morning, inside a notice's 280 characters (a full week falls back to names and
    days), not counted in a board's 30 pins, taken down when there is nothing to tell or the port is gone.
  - **Talk:** `world.ships` (in, due, late, sailed, no) and `world.ship_name` for speakers at a port's market, listed
    in `facts.json`; nothing is set where there is no port. The lines themselves are Phase 8's.
  - **A port deleted** (5.1: "Delete the Wharf and there are no ships"): at the next load (`World::dropOrphanedShips`)
    or pass of the roads its ships and its week's roll are dropped, its notice comes down, its goods refill each
    morning again, and nothing else of the save is touched. The rule set to 0 drops the ships still due (one already
    in still sails); set back, the timetable is rolled again from that day.
  - **Money:** ships have no account and pay no coin. A player selling what a ship takes is paid by the port's shops
    as on any day, under the day's sell cap and each shop's budget, every coin a registered faucet; `realBalanced()`
    holds through ship days, storms and a deleted port.
  - **Saves:** a Board from before the ships reads as no ships, and a port found at load gets its timetable from that
    day (the pre-79 fixtures and both full worlds load: `save_compat`). The mirrors of the new rule: `RatwItems.h`,
    `market.json`, `map_editor.py`, the Editor's `economy.ts` and `model.mjs`.
  - **Checked on DEV's data, read only** (DEV build 70 exported, loaded and run two game hours by `world_check`,
    nothing written): Ridgemere is a port (its Wharf posts in `w_ridgemere_0_1`); two ships are posted on the first
    pass ("Ships at Ridgemere. Due: the Long Tide on Riverday with butter, taking handbell and hide."); real money
    balanced. Of the 12 imports only **butter** waits for ships there: it is the only one Ridgemere's shops set out
    and its workshops don't craft.
  - **Perf, the three windows** (Release, 8 threads, DEV build 70 exported read only, `--players 20`; this build
    against its base `dfdd566` on the same export, runs alternating, two each and four each for 23-25; ms):

    | Window | build | mean | p99 | p99 after the first minute | worst after the first minute | worst |
    |---|---|---|---|---|---|---|
    | 6-8 | base | 2.66 | 6.06 | 5.72 | 22.8 | 386 |
    | 6-8 | Phase 5 | 2.66 | 6.15 | 5.75 | 23.0 | 381 |
    | 17-19 | base | 2.22 | 5.57 | 5.26 | 35.3 | 385 |
    | 17-19 | Phase 5 | 2.18 | 5.45 | 5.27 | 36.1 | 383 |
    | 23-25 | base | 1.68 | 4.82 | 4.23 | 54.0 | 388 |
    | 23-25 | Phase 5 | 1.70 | 4.90 | 4.32 | 53.7 | 385 |

    The port adds nothing measurable (runs of one build differ from each other by more: 1.66 to 1.74 in 23-25). **But
    the branch as it stood before this phase is past Phase 0's table,** and that is not the port's: the run now has 765
    residents (206 arrive from the Country during it; Phase 0's had 559); every window's first minute has one tick of
    about 385 ms (tick 10, in the schedules; Phase 0's worst was 28 to 45 ms); the day's turn in 23-25 has a 54 ms
    tick (9.64 then); and 17-19's worst after the first minute is 35 ms (a place streaming in; 27.95 then). Means
    are within 0.1 ms of Phase 0's and p99s within 0.4 ms. These came in with Phases 1-4 or the world's arrivals and want
    looking at before Phase 11's gate.
  - **Not built, by owner:** the talk lines for ships due, in, late and sailed (Phase 8: the facts are there); the
    DM's side (Phase 9): the week's ships on the Prosperity tab, calling a storm on a ship (`World::stormShip` has no
    command yet), `neverImported` as a DM goods list (the `never` list in `market.json` stands in) and `shipsPerWeek`
    in the Rules panel's copy (the rule is already listed); tuning the baselines, storm odds and ship goods against
    hunger (Phase 10's scratch play); the soak with ships (Phase 11, with the user's go-ahead: not run, and no economy
    simulation was run for this phase). 8.3's map-edge "outside" endpoint for a world with no port is not built and
    is in no phase's row: such a market refills each morning (8.3's "otherwise"). A ship at the pier is D8's "no".
  - **For the user to decide:** (1) On DEV only butter waits for ships, and butter is a food households eat, so a
    winter week with a stormbound ship thins it; whether that is wanted, or the ship goods should be a hand-picked
    list, is Phase 10's to settle with play. (2) Every docking refills all of the port's ship goods, while the price
    nudges go only to the goods named on that ship's manifest. (3) The notice is on the town's board signed "the
    Wharf office", not on a board of the Wharf's own. (4) "Ships paying players" was read as players selling a ship's
    `takes` to the port's shops on ship day; a ship itself never pays.
  - Tests: `port_tests` (new, 3,356 checks: a port found and its timetable, each ship as first posted through three
    weeks, never on Restday, fewer in winter, the rule at 5, at 0 and at its most; the same world the same ships; the
    Board's round trip and a Board from before the ships; save and restore mid-timetable and with a ship in, in file
    mode, and through a crash between checkpoints by the journal; ship day's nudges inside the clamp and never the
    wrong way, the refill, the dock hands' tasks, the crowd, the facts, the sailing; imports thin between ships and
    refilled on ship day, each morning with the rule at 0; storms seeded over 400 mornings and called; a world with no
    port; a port deleted in the world and under the Game with its notice; a player selling to the port's shops on ship
    day at the day's cap and the shop's budget, no unregistered faucet, `realBalanced()`; the notice's words and the
    world-text rule on them) and `port_pg` (the timetable saved to PostgreSQL by `game.save_checkpoint`, loaded back the same and
    restored by a second process: 12 checks). ctest 83/83 with the database, none skipped (a throwaway
    `ratw_test_s2stage_p5`, dropped after; `port_pg`'s own `ratw_test_s2stage_*`); `trade_show_tests` 475,
    `host_edit_tests` 550, `roads_tests` 407, `market_tests` 1,969, `save_compat` 23,085 (23,101 with
    `RATW_PRE79_BUILDS`: DEV and dev-old exported read only, both full worlds loaded). All 34 `tools/test_*.py` with the
    database and this build's server (666 tests, none failing; throwaway names tagged `s2stage`; skipped only the 23
    `RATW_SLOW` world builds and `test_game_tables`' save under `Saved/`); the Editor's 81 + 67 and its typecheck.
- **2026-11-10: Phase 5 repaired** (branch `r2-staged-p5`, on `76e1699`). Three independent checks of the port left
  30 problems and a list of what the plan asked for and the phase had not built. Each was looked into against the
  code; each one fixed has a test that fails on `76e1699` and passes now (run against a copy of that commit), unless
  it says otherwise below. No economy simulation was run.
  - **What the notice and the talk say is now true** (1, 2, 5, 6, 9, 10, 11, 12, 16):
    - "No ship this week" is said only of a week in which none came, is in or is still due (it was said with a ship
      due that day). A sailed ship stays on the timetable through its week's end for that.
    - A ship is "in, unloading" and the Wharf a crowd only while a dock hand is at that work: a ship brought in at a
      quiet pier (the DM's, or a restart at night) reads "is in" and no crowd. No ship comes on Restday; ships dock
      8:30 to 11:00 and sail 17:00 to 19:30, and "this evening" follows the hour (it was said at 15:30).
    - `world.ship_name` names the ship `world.ships` speaks of, in the code's order (`World::shipFacts`): the one in,
      else one a storm kept out these two days, else one due today, else one that sailed since yesterday, else one due
      within two days (it named yesterday's). (Corrected at the merge: this line had the order wrong.)
    - A full week no longer drops ships silently: every ship due in seven days is named or counted ("and 2 more by
      Marketday"), all inside a notice's 280 characters at any rule and with several storms; a due line lists all a
      ship takes or none of it.
    - There is a notice whenever there is a ship to tell of (between ships: "The next is the Gull, on Marketday of
      next week."), and the board's scent line for it is "It smells of tar and salt, and of many paws." (it read "It
      carries no scent at all. Someone took care.").
    - A price's cause reads "the Gull came in", "the Gull bought a load" or "the Saltwife is stormbound" (the raw
      words "ship" and "ship late" showed). Cargo is in the plural where a thing is counted by the piece ("hides",
      "small pelts", "handbells"), by rule, with `market.json`'s new `wharf.cargoWords` for the rest.
  - **What makes a port** (4, 15, 26): a market with a post of the Wharf's own words, now data (`wharf.posts` in
    `market.json`: its office, its harbourmaster, the hands unloading ships, the cranes, Ridgemere's overseer by his
    own title; Core holds no Ridgemere title any more), with the sea trade's other posts there (carriers, store
    keepers, talliers) the port's too. A town with a long store and no wharf is no port. A post is of the market its
    work room is in, else the nearest by path, never its holder's home's: one Wharf was two ports. A tannery or
    smokehouse with "Wharf" in its name, and the Watch's round of the wharf, are no dock posts. Between ships a hand
    whose title is "unloading ships at the piers" is shown "minding the cargo at the piers" (reason "No ship is in.").
    On DEV: 38 posts (15 unloading, 4 at the cranes, 7 carrying from Sump Stairs, 3 at the Wharf Office, 8 keeping and
    tallying the stores and the books, the overseer).
  - **The timetable holds up** (3, 17, 18, 19, 20, 21, 28):
    - The rule set to 0 takes the last notice down (it stayed up).
    - A port's 16 slots full: the oldest ship sailed or stormbound makes room, so a week to come is always rolled
      (a week's ships were skipped).
    - A crash on ship day: the save says the ship is in, the shelves are set out again from the back rooms at the
      restore, piece for piece (they came back empty with the ship in).
    - A calendar jump: ships more than a day gone are dropped quietly, none docks late, real money stays balanced
      (eight docked at once).
    - A ship's cargo follows its port and day, not the Board's ship counter, so the same world shows the same cargo
      whatever else was rolled.
    - A save that lies is put right at load: a ship "in" or "sailed" on a day to come is due again; two ships of an
      id, or two a port a day, keep the first.
    - A week has at most six ships (it could have seven at rule 6, one on Restday).
  - **Imports** (13, 25): a port's ship goods are `market.json`'s `wharf.imports` its shops set out and its workshops
    don't make, and now also whatever else its shops set out that nothing in the realm makes and no wagon or cart
    brings. On DEV that is 16 goods waiting for ships (butter and 15 that had kept the old morning refill: blanket,
    chamomile, crate, cushion, wooden floor bowl, hay, horse tack, lavender, mead, mint, oil lamp, theriac, tick comb,
    valerian, yarrow; `world_check --ship-day` prints them as `ship goods:`. Corrected at the merge: the list here
    was the test fixture's, not DEV's). A ship carries up to four, half of `market.json`'s own where there are others, and
    leads with a different good from day to day, so a notice no longer reads "with butter" every time. What a Country
    farm makes stays the carts'.
  - **The forbidden word** (14, 29): one check in Core (`market::forbiddenWord`), used by the loader for a host's ship
    names (`market::shipNamesOf`: a bad name is left out, the rest kept) and by the tests on every notice and event
    line. The check over all new talk lists is Phase 8's, with the lists.
  - **Cleaned up** (22, 27): the unused `stormsCalled_` is gone (`World::stormShip` acts at once); the roads' and the
    port's copies of the seeded lot and the word list are one header (`Core/RatwSeeded.h`), with `RatwTradeShow.cpp`'s
    own copy left for the Phase 4 merge.
  - **Perf** (23, 24): `world_check --ship-day` brings a due ship in at the window's start, so the gate has a ship
    in. 6:00-8:00, DEV build 70 exported read only, 20 players, 24,000 ticks, Release, 8 threads, one run each:

    | Window 6-8 | mean | p99 | p99 after the first minute | worst after the first minute | worst |
    |---|---|---|---|---|---|
    | no ship in | 2.72 | 6.14 | 5.74 | 23.1 | 395 |
    | a ship in (`--ship-day`) | 2.63 | 6.15 | 5.77 | 23.1 | 399 |

    A ship in costs nothing measurable; the slowest tick after the first minute is 7:00's schedules in both (23 ms),
    under the 25 ms the check saw on a ship day, since no ship docks before 8:30 now. Real money balanced in both.
  - **Rejected, and why:**
    - 26, in part: Sump Stairs' seven porters stay dock hands. `Society::postRules` puts "carrying from" in the sea
      trade, and on DEV those posts work in `w_ridgemere_0_1`, the piers' own cell.
    - 27, in part: ships stay a list of their own beside `committed` (5.2 amended above): a ship has no route, wagon
      or carter, and has a day, an hour and a cargo a departure has not. One save, one journal, two lists.
    - 28, in part: the rule stays the middle of the week's count (rule ± 1, so 1-3 at the default), and weeks are
      rolled through the next so seven days are always committed. 8.3's note above says so; exact counts are the
      user's call.
    - 30: an 80-160 ms tick in `RatwAuthoring` and `ensureTradePoints` is not the port's (it is there with no port).
      Noted for Phase 11's gate, with the 395 ms tick 10 already listed above.
  - **Left for later, by owner:**
    - Phase 6: import shelves thinning between ships as households buy (8): shelves fall only when someone buys, and
      the staged buyers are Phase 6's.
    - Phase 7 (staged events, 8.4): a ship lost as a story of its own (8.6's row is built as "stormbound"), and "a
      ship's rich cargo".
    - Phase 8: the talk lines for ships and the Wharf's crowd, and the forbidden-word check over every new list.
    - Phase 9: the DM's storm command and the week's ships on the Prosperity tab (`World::stormShip` and
      `World::dockShip` are there for them), Atlas's Wharf panel, `neverImported` as a DM list (`wharf.never`
      stands in), and a map-edge "outside" endpoint for a world with no port.
    - Phase 10: how far a ship moves a price (7: about a penny on most goods at ±0.1), the storm odds and the list of
      ship goods, with play.
    - Phase 11: the soak with ships, with the user's go-ahead.
  - **Seen and not fixed** (Phase 4's helper, the other check's to merge): `Society::sendOn` moves only wares a shop
    sets out, so a ship does not take what players sold to a shop that only buys that good.
  - **For the user to decide** (each built one way, cheap to change): (1) the wider imports rule above, which puts 16
    goods on DEV at a ship's mercy instead of one, or a hand-picked list; (2) the week's count as rule ± 1 with at
    most six, or exact; (3) ship names as a checked list in `market.json`, or a Tracery grammar; (4) the notice on the
    town's board signed "the Wharf office", or a board of the Wharf's own, which this game has no kind of yet;
    (5) Sump Stairs' porters as dock hands.
  - Tests: `port_tests` 8,142 checks (3,356 before; 13 new tests, and `--only NAME` and `--each` to run them one by
    one); `port_pg` 12; ctest 83/83 with the database, none skipped (a throwaway `ratw_test_s2stage_p5r`, dropped
    after); `trade_show_tests` 475, `host_edit_tests` 550, `roads_tests` 407, `market_tests` 1,969, `save_compat`
    23,085, `game_tests` 3,631. All 34 `tools/test_*.py` with the database and this build's server: 668 tests, none
    failing, 25 skipped (the 23 `RATW_SLOW` world builds, one that needs a save under `Saved/`, one the built client).
    `test_world_package` has two new ones: the strip keeps a timetable, and a package with ships planted in DEV's
    save (exported read only) is imported and loaded by a real server, which keeps the ship as posted, puts a ship
    "in" before its day back to due and drops one for a port the world has not got. The Editor's files are
    untouched. Release build clean (no error, no warning in the phase's files).
- **2026-11-10: Phases 4 and 5 merged** (`r2-staged-p5` at `5e6ccaa` into `r2-staged` at `c126934`: merge `de5e918`,
  then the work below in one commit). The only conflict was this section; `CMakeLists.txt`, `Core/RatwGame.h`,
  `RatwRoads.cpp`, `RatwTradeShow.cpp`, `RatwGameJournal.cpp` and `RatwCheckpoint.cpp` merged as text.
  - **Ships in the journal.** Phase 5 was built on the journal before Phase 4's third repair. Under that repair's
    scheme nothing new is followed field by field: the ships are part of the Market Board (`ships`, `shipsThrough`,
    `nextShip`, with `backRoom` and `restocked`), and `Game::record` writes the Board whole (`set society.board`)
    with any record of the economy or the roads whenever it differs from the shadow. So every ship step on a world
    tick (a ship docks, sails, is kept out by a storm, a morning rolls a week, a port is deleted and its ships
    dropped) rides the next record, and `journalCheckpoint()` writes it as each checkpoint is taken (before a fork,
    before `saveInBackground`, in `save()`): a snapshot captured and not landed loses nothing, and one that lands
    has the same in the journal up to its number. A replay sets the Board, it does not add to it, so a step is never
    applied twice; the roads' first pass after a restart finds the ship as the record left it and catches up only
    what the clock passed while the world was down (`tendPort`).
  - **The tills a ship touched go with it** (leftover F). The Board said "in" and the back rooms full while the
    shelves were the checkpoint's, so after a crash a delivery was part lost and a sailed load was back on the
    shelves. Now `Society::arrivalRefill`, `sendOn` and `stagedRestock` note each till whose shelves they moved
    (`takeTillsMoved`), and the same record that carries the Board writes those tills whole
    (`set society.accounts.<till>`). This is wider than ships on purpose: a wagon or cart in or out and the
    morning's restock move shelves with the Board in the same way. Phase 5's repair set shelves out again from the
    back rooms at every load to mend this; that is gone (`Society::setOutFromBackRoom` removed), so a restart moves
    nothing and a shelf players bought bare stays bare till its morning (`aCrashOnShipDay` is `aRestartOnShipDay`).
    **Still not journalled:** what a player's trade or a resident's buying moves on a staged till's shelves (Phase
    1's design: the player's side is journalled, the till's is the next checkpoint's), so a crash can still put back
    a shop's stock as of the last ship step or checkpoint, whichever is later.
  - **Crash cases** (`port_tests`, `shipDayThroughACrash`): a ship docks, sails, is kept out by a storm, and a
    morning rolls a new week (`ship_docks`, `ship_sails`, `ship_storm`, `ship_morning`), each after a checkpoint,
    then a player's trade journalled, then the game gone without saving; and each again as a `_window` case (a
    snapshot captured that doesn't land, a purchase, then the crash). After the restart the port is the port of the
    crash: the same ships and statuses, prices, shelves and back rooms, the step done once, the next passes doing
    none of it again, `realBalanced()`. All eight go through Postgres (`port_pg`: the checkpoint by
    `game.save_checkpoint`, the records as rows of `game.journal`, read back as the server reads them; 216 checks
    beside the timetable's 12; its database is `ratw_test_s2stage_<hex>`, dropped after). A port deleted is
    `aPortDeletedThroughACrash` (file mode: the save loaded into a world without its Wharf, the Board without the
    ships in the next record, a crash never bringing them back) beside Phase 5's `aPortDeletedUnderTheGame`; the
    drop on a tick of a running game has no staged crash of its own (the Board goes whole either way).
  - **Leftovers of Phase 5's last check:**
    - **A, fixed** (`market_tests`, `aClockBehindTheBoard`): after a crash the Board (journalled) can be on the new
      day while the restored clock is before dawn; the day's budgets and caps then read as a fresh day and counted
      sales against nothing (a probe paid 554p past shop budgets of 40p and the 400p `playerSellMost`). A Board day
      at or ahead of the market's day is today (`Society::boardsDayIsToday`, in `budgetLeft`, `takenToday` and
      `soldToday`); the faction discount's week and the restock's day read the same way. `rewardsADay`, `theftADay`
      and `stakesADay` needed nothing: their day (`budgetDay`) only moves forward, so a clock behind it keeps counting
      against the same day's tallies.
    - **B, fixed** (`aTimetableThatLies`): `nextShip` is raised above the highest ship id at decode and at restore
      (`market::mendShips`).
    - **C, fixed, with one thing kept on purpose** (`aTimetableThatLies`): `mendShips` holds a saved timetable to
      what the game could have rolled: a name that doesn't fit `shipNameFits` is replaced from the world's list (or
      "a ship"); cargo keeps known goods once each, counts 1 to 999, at most five, nothing of `wharf.never` brought;
      hours inside the day; an unknown status is due; sixteen ships a port (the oldest sailed or stormbound go
      first) as well as 256 in all. At restore a ship of a day past next week's end is dropped and `shipsThrough` is
      held to next week. Names are checked for the forbidden word as letters anywhere, any case
      (`market::forbiddenInName`: "Seadog" is refused; so is any honest name that holds those three letters);
      `forbiddenWord` stays whole-word for cargo words and notices. **Kept:** a "storm" on a day still to come inside
      the rolled weeks. `World::stormShip` called ahead (the DM's storm, Phase 9) makes exactly that and has already
      moved the price, so undoing it at load would un-call a real storm; only one past next week goes.
    - **D, fixed in memory** (`theRuleOffAndOnTheSameMorning`): the ships the rule stops are kept the day
      (`World::shipsStopped_`); the rule back the same day brings back those whose hour is still ahead, the same
      ship, id, name and cargo, posted once. Not saved: a restart or a crash between the two loses today's ship as
      before, and a ship whose hour passed with the rule off does not come.
    - **E, corrected** in "Phase 5 repaired" above: DEV's 16 ship goods (re-derived on DEV build 70 exported read
      only; `world_check --ship-day` now prints them as `ship goods:`), `world.ship_name`'s order, and 23 ms.
    - **F, fixed:** the tills above.
    - **G, fixed** (`aShipTakesWhatAShopOnlyBuys`): `Society::sendOn` takes whatever a till holds of the good over
      its shelf target, set out or only bought from players. It is the roads' helper too, so a wagon or cart setting
      out takes such stock as well (7.5 says a departure takes what a market has over).
  - Each new test fails with its fix taken out (B: the next id 1 under its ships; D: today's ship gone; G: 24 left
    of 24; A, F and the crash cases as their entries say).
  - Tests: a clean Release build (`build-m45c`; no warning in the files touched); ctest 84/84 with the database, none
    skipped, run twice (`pg_client` on a throwaway `ratw_test_s2stage_m45`, dropped after; no `s2stage` database
    left); `port_tests` 9,064 checks (8,142), `port_pg` 12 + 216, `market_tests` 2,001 (1,969), `trade_show_tests`
    830, `shops_tests` 132, `roads_tests` 407, `host_edit_tests` 550, `game_tests` 3,631, `exploit_tests` 871,756,
    `save_compat` 23,085. All 34 `tools/test_*.py` with the database and this build's server (throwaway names tagged
    `s2stage`, none left): 668 tests, none failing, 24 skipped (the 23 `RATW_SLOW` world builds and
    `test_game_tables`' save under `Saved/`; `test_live_map`'s fourth test sits below its `unittest.main()` and runs
    only through `python3 -m unittest`, as it was run here). The client's 146 tests and its typecheck; the Editor's
    81 + 67 and its typecheck (the client's browser tests start a server and were not run). No perf run: `world_check` has no journal, and
    the port's tick is as Phase 5 measured it.
  - **Unsure, for the next check:** whether a storm called ahead should survive a load (C); whether the name rule
    should refuse honest names holding the letters; that a till journalled whole at a ship step carries its money
    as of that step (balanced in every crash case here, but only ship steps and trades were staged); D across a
    restart; and the trade-moved shelves above, which stay Phase 6's or a later repair's.
- **2026-11-10: Phases 4 and 5 merged, repaired** (`r2-staged`, on `15b85a3`). Three independent checks of the merge
  (crashes and the journal; money and caps; speed) left 13 findings. Each was looked into against the code; each one
  fixed has a test that fails before its fix and passes after, unless it says otherwise. No economy simulation was run.
  - **Escort pay told and then lost in a crash, fixed** (`trade_show_tests`, `anEscortsArrivalPayThroughACrash`). A
    wagon coming in on a world tick paid its escort and told her so, and nothing journalled it until the next economy
    record: a crash put the wagon back on the road, the replayed arrival found her gone, and the pay went back (82p at
    the crash, 70p after it, the escort "expired"). Now whatever a world tick tells players (`World::takeNotices`: an
    escort's pay, a courier's at the hand-over, a camp's bounty and found cargo) is journalled first as one economy and
    roads record, and each notice waits for that record as a command's reply does (`Game::tick`). A player is told only
    what a crash can't take back.
  - **Every board notice dropped at every load, fixed** (`noticesSurviveARestart`). `Game::lettersLoad` kept only
    documents with someone they are to, and a notice is to no one: a player's paid notice and the carters' were gone
    after any restart (docs 54/55's fault, not these phases'). A notice is now kept by its board. And at every start
    the carters' notices are made to say what the Board and the roads say by the journal (`Game::mendRoadNotices`): a
    wagon still to set out has both, written afresh (an escort taken after the checkpoint is no longer asked for); one
    on the road keeps them without asking for an escort; a robbed one keeps its far end's; one come in or gone keeps
    none. **Seen, not fixed** (older, doc 54): a notice itself is not journalled, so a crash inside the 30 seconds after
    it is pinned loses it while its penny stays paid.
  - **A torn journal tail, then more records, then a second crash, fixed** (`journal_tests`, `aTornTailThenMoreRecords`).
    The file journal now cuts a torn last line before it appends (`cutTornTail`), so the next record is never glued
    onto the torn bytes and a second crash before the next trim no longer leaves a journal the world refuses to start
    on. File worlds only, as the finding said.
  - **A crashed database server's snapshot left in `/tmp`, fixed** (`deadServersSnapshotFoldersSwept`). A server
    making its snapshot folder first removes the `ratw-snapshots-<pid>` folders of this user's servers that are gone
    (`game::sweepSnapshotFolders`); a running server's, or another user's, is left. (The test fails with the removal
    taken out; before the fix there was no sweep at all.)
  - **The save renamed without being on the disk, fixed, no test** (a power cut cannot be staged here). A file world's
    save is now written, `fsync`ed, renamed, and its folder synced, before `Game::save` trims the journal; the forked
    snapshot syncs the folder after its rename as well.
  - **A Board far ahead of the clock froze every cap and the market's morning, fixed** (`port_tests`,
    `aBoardAheadOfTheCalendar`). At a restore the Board's days are held to the restored calendar's
    (`World::holdBoardToCalendar`), as the ships' weeks already were: a Board day more than one ahead of the market's
    day (one ahead is A's case, kept) runs that day's morning on the next pass; the slider's day the same; the
    discounts' week, each till's restock and the roads' rolled days no further than the calendar allows. The test
    loads a save with the Board 30 days ahead: each of the next five days has its morning, a fresh budget, the slider's
    step and the shop's restocks, real money balanced.
  - **Speed** (findings 7-10; DEV build 70 exported read only, `world_check --simulate A B --players 20`, Release,
    8 threads, one run each, the machine at load 3-4 from other work). All of it was route searches made cold, work
    done again for nothing, or a budget looked at too seldom:
    - **Route searches about five times cheaper, the same routes.** `World::firstSteps` (one cell's cheapest first
      steps to everywhere, behind every route) ran over maps of names and every door record of a loaded cell (a city
      cell has over a thousand seams): 1.5-1.8 ms cold on DEV. It now runs over each cell's place in id order and
      keeps each cell's ways until the doors or roads change (`routeNode`, `routeRank`): 0.34-0.41 ms. The steps are
      the same, ties included: checked against the old search on every cold search of a 6-8 run on DEV (400, no
      difference; a scratch build ran both and compared). This took tick 110's chain finish (117 cold searches, about
      200 ms with the budget check below) to under 20 ms, ensureTradePoints' routes at tick 20 from 143 ms to 23, and
      7:00's Country errands (about a millisecond each, no route search: these were cold first steps) off the list.
    - **The markets and the roads found again only when they could have moved** (findings 8 and 10).
      `ensureTradePoints` and `ensureRoadMarkets` are keyed on `Society::placesRevision()`, the roster's revision less
      its beds' moves: a home's interior streaming in places its beds and moved the roster about 120 times a morning.
      Rebuilds in a 6-8 run: 5 (41-44 before). The 95-106 ms tick (a rebuild on a cache a dead end had just emptied)
      did not appear in any run, and a cold rebuild now costs about a fifth of what it did.
    - **The day's market squares worked out only when they change** (finding 9). `World::square` checked once a day
      emptied the squares, and the Country's seven loaded two cells each at the next plan (55-58 ms). A square is now
      checked when the roster's places move, against its market's cell and spot and the posts and homes in its
      cell (`squarePrint`), and worked out again only if one differs. 23-25's day turn is 12.6 ms.
    - **The errands' budget looked at every 4 residents, not 32** (finding 9's 7:00 tick). Never in a deterministic run,
      where `spent()` is false.
    - **A fresh world's first staffing spread and made cheaper** (finding 7's tick 10, about 400 ms). `staffFarms` looks
      at a trade's own people (`poolOf`), and works out a home's class and a person's tie to the Heights once
      (`topClassOf`, `upperOf`): 89 ms to 9, the same 206 placements, in the same order, to the same homes (compared on
      DEV). The cart ends of farms nobody works whose community is no market are not searched for by path (they send
      no cart; found when a post there is held, at the latest the next day's rebuild): 156 ms gone. And a host makes at
      most `World::ResidentsAPass` (12) of the residents asked for a pass, the rest on the next passes in order
      (`Society::takeRequests(most)`; the database server's path too): 206 arrivals in one tick took 135 ms.
    - **The beds and stores of a home streaming in** are given to the society for that home alone (`setBedsOf`,
      `setHomeStoresOf`), not as whole copies for each of the 61 homes at 18:00: about 8 ms of that tick.
  - **Perf table** (ms; Phase 0 is its own entry's; "merged" is the speed check's re-measure of `15b85a3`, ranges over
    its runs):

    | Window | build | mean | p99 | p99 after the first minute | worst after the first minute | worst |
    |---|---|---|---|---|---|---|
    | 6-8 | Phase 0 | 2.58 | 5.79 | 5.55 | 20.97 | 31.66 |
    | 6-8 | merged | 2.71-2.73 | 6.01-6.17 | 5.66-5.73 | 22.9-24.6 | 397-423 |
    | 6-8 | repaired | 2.64 | 5.84 | 5.45 | 12.2 | 39.9 (tick 19) |
    | 6-8 `--ship-day` | merged | 2.71 | | 5.78 | 24.3 | |
    | 6-8 `--ship-day` | repaired | 2.68 | 5.77 | 5.42 | 11.9 | 39.6 (tick 19) |
    | 17-19 | Phase 0 | 2.21 | 5.56 | 5.28 | 27.95 | 27.95 |
    | 17-19 | merged | 2.22 | 5.48 | 5.24 | 34.7 | 411 |
    | 17-19 | repaired | 2.19 | 5.15 | 4.95 | 23.2 | 40.4 (tick 19) |
    | 23-25 | Phase 0 | 1.61 | 4.71 | 4.02 | 9.64 | 45.21 (first minute) |
    | 23-25 | merged | 1.71-1.72 | 4.98-5.08 | 4.35-4.45 | 57.1-57.9 | 398 |
    | 23-25 | repaired | 1.63 | 4.85 | 4.28 | 12.6 | 40.3 (tick 19) |

    Every window is at or under Phase 0 after the first minute except 23-25's worst (12.6 against 9.64: the day's
    careers pass, now over 765 residents) and its p99s (+0.14, +0.26). **Left for Phase 11's gate, with the reason:**
    the first minute's worst, about 40 ms against 31.66 and 27.95 in 6-8 and 17-19 (23-25 is under its 45.21). It is
    tick 19 on a fresh world: the trade points found again as the Country's arrivals come in (their farmsteads' first
    route searches, about 17 ms) and the bands over the new households (8 ms). A world from DEV's export staffs its
    Country once (the server saves it), so on a running server this is a start's first minute, once.
  - **Finding 11 (the gate's means and p99s), no code change.** Its own reading holds: most of it followed 487f9db's
    staffed Country (765 residents, Phase 0's 559). With the fixes above the means are within 0.06 ms of Phase 0's and
    the p99s after the first minute at or under it in 6-8 and 17-19. Whether to take the gate's baseline again with
    the staffed Country, or keep Phase 0's, is the user's call before Phase 11.
  - **Finding 12 (ship and journal costs outside the gate), nothing to fix.** The port costs nothing measurable (the
    `--ship-day` rows). `world_check` has no Game and no journal, so `Game::record` is not in the gate; a world tick
    that tells a player something now makes one record (about 0.05-0.1 ms, measured by the check), only on such ticks.
  - Tests: a clean Release build (`build-rep-clean`, no warning in the lines touched); ctest 84/84 with the database,
    none skipped, run twice on it (and twice on the working build; `pg_client` on a throwaway `ratw_test_s2stage_rep`,
    dropped after; the `_pg` tests' own `ratw_test_s2stage_*`; no `s2stage` database left); `trade_show_tests` 869 (830), `port_tests` 9,103 (9,064), `journal_tests` 34,
    `market_tests` 2,001, `game_tests` 3,631, `farm_people_tests` 4,879, `roads_tests` 407, `host_edit_tests` 550,
    `shops_tests` 132, `save_compat` 23,085, `trade_show_pg` 100, `pg_client` 24. All 34 `tools/test_*.py` with the
    database and this build's server (throwaway names tagged `s2stage`, none left): 668 tests, none failing,
    24 skipped.
- **2026-11-10: Phases 4 and 5 merged, repaired, checked** (`r2-staged`, on `8399ecc`). Two independent checks of the
  repair left twelve findings. Each was looked into against the code; each fixed has a test that fails with its fix
  taken out and passes with it (checked one by one on this build). No economy simulation was run.
  - **The arrivals' places, corrected** (finding 1; `trade_show_tests`, `residentsWaitingAtACheckpoint`). The repair's
    "the same 206 placements, to the same homes" held for home cells only, and results after the 6-8, 17-19 and 23-25
    windows are not `15b85a3`'s. The cause was older than the cap: `takeResidentRequests` found every place of a pass
    before any arrival was made, so two arrivals for one home could be given one tile (the 206 of a fresh world's first
    pass); with twelve a pass, the later batches saw the earlier ones' homes, and eight of the 206 moved. Now each tile
    found is kept from the rest of its pass, so no two share one and the places don't hang on how many a pass makes
    (made one a pass and all at once: the same tiles). Results still differ from `15b85a3`'s: its shared tiles are gone,
    and the arrivals come over ticks 9-26, not at tick 9, as the cap means. The cap stays: the 206 at once are 135-144 ms
    (measured again here: `Society::addResident` builds the posts and reconciles the careers once for each, about 85 ms
    of it), and making that cheaper is Phase 11's if anyone's.
  - **Residents asked for and not made at a checkpoint, fixed** (finding 5; the same test). A checkpoint now makes those
    still waiting first (`Game::journalCheckpoint`, `makeResidents(all)`): the society's requests are in no save, and a
    database world's arrivals have their rows placed as they are made. `arrivalFor`'s "already sent for" now counts a
    farm placement too. (Seen, not looked into: on the strip, a world from files, a child born before a save is not
    there after a restart.)
  - **A later tick's notice overtaking one held for its record, fixed** (finding 6; `aLaterNoticeWaitsItsTurn`, a store
    whose journal commits only when let). Every tick notice now goes through `beginHolding`/`endHolding`, so one with
    nothing to wait for still waits behind anything of that player's still waiting.
  - **The journal-before-tell record on every tick with a notice, fixed** (finding 8; `aFightLineIsNotJournalled`). Its
    cost was about ten times the repair's "0.05-0.1 ms": 0.4-0.9 ms on DEV for a record with nothing to write. A notice
    now says whether it speaks of something the tick moved (`World::notice(..., moved)`: an escort's pay, a courier's, a
    raid, a purse robbed, a camp's bounty and found cargo, a thing worn out), and only such a tick records first; a
    fight's lines and the like are told as they come. The ordering above holds either way.
  - **Board notices journalled** (findings 7 and 12; `aNoticeAndItsPennyThroughACrash`, `port_tests`
    `aShipsNoticeTakenDownStaysDown`). Every record of the economy or the roads now carries the boards' notices whole
    when they differ from the last record's (`set people.boardNotices`; `Game::lettersLoad` takes those over the
    checkpoint's own). A player's notice and its penny go through a crash together (the doc 54 loss the repair left);
    one taken down stays down (`unpost` records); the Wharf office's notice taken down after the checkpoint no longer
    comes back for its seven days. And at a start (`mendRoadNotices`) a ships' notice whose port has nothing to tell
    comes down: a tick's step is journalled in the tick's record and the notice it changes after it, so a crash between
    the two leaves them apart. Either fix alone passes the ship test; with neither it fails. No save format change: the
    checkpoint keeps its notices in `documents` as before, and `boardNotices` is only ever a journal record's.
  - **A robbed wagon's far notice after a crash, fixed** (finding 9; `aRobbedWagonsNoticeAfterACrash`). At a start a
    raided wagon's far-end notice says "Robbed on the road: " and comes down within two days, as the raid's event makes
    it, when the raid is the journal's alone.
  - **The markets found again when a dead end is found or forgotten, fixed** (finding 3; `stream_tests`,
    `aDeadEndFindsTheMarketsAgain`: a tent in c_1_0, Pip home by it, the dead end forgotten, nobody's place moved).
    `ensureTradePoints` is keyed on the dead ends' revision too (`World::deadEndsRevision_`), and the roads' markets
    with it.
  - **Market squares checked each day again, fixed** (finding 4; `gathering_tests`, `aCampOnTheSquare`). A square is
    checked at each day's turn as well as when the roster's places move; `squarePrint` covers what stands on its
    ground (a Chapter's camp: `obstacles`); and its stalls and crowd keep off every door's tile, open or shut
    (`World::inDoorway`), so what a door was doing when the square was found no longer decides it. Only a square whose
    print differs is worked out afresh, so the day's turn loads nothing new.
  - **The journal's folder synced, fixed, no test** (finding 10; a power cut cannot be staged here). The trim's rename,
    and the journal file's first line (made, or emptied by a trim), are followed by an `fsync` of its folder.
  - **The snapshot folders, fixed** (finding 11; `journal_tests`, `deadServersSnapshotFoldersSwept`). A server's
    folder is now its own (`ratw-snapshots-<pid>-<six letters>`, `mkdtemp`), so no later server takes over a dead one's
    by its pid; and the server holds an `flock` on its `.lock` for as long as it runs, which the sweep tries before it
    removes anything, so a live server in another pid namespace sharing `/tmp` is left alone. A folder with no lock is
    an older build's (removed as before) or one being made (left a minute).
  - **The 25 ms tick after the first minute, found and fixed** (finding 2; `stream_tests`,
    `aDeadEndFindsTheMarketsAgain`'s first check). Not the port's: it comes in any 6-8 window, in some runs and not
    others (three quiet runs of `8399ecc` with `--ship-day` had none, worst after the first minute 12.7-13.0; a plain
    6-8 run of this build before the fix had a 30 ms one, and so did one probe in three). It is tick 3279-3289 (6:16):
    the roster's revision moves with nothing the markets rest on changed (after the fix no rebuild comes then), the
    markets are found again, and their market-to-market routes, cold, cost 22 ms of the 24.7 (timed inside
    `ensureTradePoints`).
    Now a move of the roster finds them again only when something they are found from has changed: the households and
    the regions of their homes, the merchants' posts, tills, trades and shelf goods, the farm crews, the map's regions
    and the dead ends (a print of them; `World::tradePointsPrint_`). DEV's 6-8, deterministic, one thread: the same
    people, society and map digests with the skip and without it. Four short runs after it (6:00-6:24, with the place
    check) had no such tick.
  - **Perf table** (ms; DEV build 70 exported read only, `world_check --simulate A B --players 20`, Release, 8 threads,
    one run each on this build, load 1.2-2.6 with another session's `world_check` on one core; Phase 0 and "repaired"
    from the entries above):

    | Window | build | mean | p99 | p99 after the first minute | worst after the first minute | worst |
    |---|---|---|---|---|---|---|
    | 6-8 | Phase 0 | 2.58 | 5.79 | 5.55 | 20.97 | 31.66 |
    | 6-8 | repaired | 2.64 | 5.84 | 5.45 | 12.2 | 39.9 (tick 19) |
    | 6-8 | checked | 2.85 | 6.23 | 5.90 | 12.3 | 41.6 (tick 19) |
    | 6-8 `--ship-day` | repaired | 2.68 | 5.77 | 5.42 | 11.9 | 39.6 (tick 19) |
    | 6-8 `--ship-day` | checked | 2.64 | 5.88 | 5.50 | 11.9 | 38.7 (tick 19) |
    | 17-19 | Phase 0 | 2.21 | 5.56 | 5.28 | 27.95 | 27.95 |
    | 17-19 | repaired | 2.19 | 5.15 | 4.95 | 23.2 | 40.4 (tick 19) |
    | 17-19 | checked | 2.13 | 5.09 | 4.94 | 22.1 | 36.9 (tick 19) |
    | 23-25 | Phase 0 | 1.61 | 4.71 | 4.02 | 9.64 | 45.21 (first minute) |
    | 23-25 | repaired | 1.63 | 4.85 | 4.28 | 12.6 | 40.3 (tick 19) |
    | 23-25 | checked | 1.61 | 4.67 | 4.11 | 13.4 | 38.6 (tick 19) |

    Each window is within a run's noise of the repair's. 6-8's mean and p99s are higher, run beside the other session's
    `world_check` (the ship-day run after it, as busy, is at the repair's). 23-25's worst after the first minute is the
    day's careers pass at midnight (12.6 ms of schedules), as before: the squares' check at the day's turn costs nothing
    measurable there.
  - Tests: a clean Release build (`build-s2fix-clean`; no warning in the lines touched); ctest 84/84 with the
    database, none skipped, run twice on the working build of the same code (`pg_client` on a throwaway `ratw_test_s2stage_fix`, dropped after; the `_pg` tests' own
    `ratw_test_s2stage_*`; no `s2stage` database left); `trade_show_tests` 919 (869), `port_tests` 9,111 (9,103),
    `journal_tests` 39 (34), `stream_tests` 285, `gathering_tests` 128, `market_tests` 2,001, `game_tests` 3,631,
    `farm_people_tests` 4,879, `roads_tests` 407, `host_edit_tests` 550, `shops_tests` 132, `exploit_tests` 871,756,
    `save_compat` 23,085. All 34 `tools/test_*.py` with the database and this build's server (throwaway names
    tagged `s2stage`, none left; `test_live_map` through `unittest`, `test_dm_live` with `RATW_SERVER` set): 668
    tests, none failing, 24 skipped (the 23 `RATW_SLOW` world builds and `test_game_tables`' save under `Saved/`).

- **2026-11-11: Phase 6 (street life) built** (branch `r2-staged-p6`, cut from `r2-staged` at `8399ecc`; the society's
  half and the world's in `Core/RatwStreet.cpp`, new). No economy simulation was run; no server was started; DEV was
  only exported read only (build 70) for the perf windows.
  - **Theft, an expected count a market** (10.1): each day `Society::theftDay` sums each market's households' band theft
    weights (`status.json` rows' `theft`), times `street.theftBase` (0.03) and its Times (`street.theftTimes`, ×1.5 hard to
    ×0.7 boom): `floor(E)` thefts and one more by a seeded lot for the rest, at most the new rule `theftCap` (4). The
    thieves are drawn by a seeded, weighted lot from the market's adults (16 or more; never a guard, one held, one
    following a leader, nor a lifted household's), by band (a rich one only when sly, `street.slyRich`), by honesty
    (`street.theftHonesty`: frank 0.2, plain 1, guarded 1.5, sly 3; the voice table's trait, which the Game hands the
    society with the quarters) and twice if hungry; each with a seeded hour, 8:00 to 20:00. From its hour a thief
    (`World::chooseMarks`) goes for a target that fits its want: hungry (55 or more, nothing to eat), a keeper of its
    market whose till holds food; else someone of a higher band in its market, not of its household, where most people
    stand. The approach, the guards' eyes, witnesses, warrants, gaol and pursuit are unchanged; one not on its mark by
    21:00 lets the day go; one that tried today (its incident, after a restart) doesn't try again. Never a player (D11).
    Residents in no band (a world without the staged households: Greyfen's demo roster in `crime_tests`) keep the old
    rule. In 160 households: 2.5 a day at −100, 0.8 at 0, 0.3 at +100 (`street_tests`); a hamlet of four: a theft on 8
    days of 365.
  - **The till-food path, the takes, `worth()`** (10.1, 6.5, 7.8): a hungry thief at a food shop takes one or two of the
    cheapest food on its till's shelves (`steal()`'s new path: the till pays, the keeper is the one robbed, the till's
    shelves are journalled with the next record). A pocket's coins go by the victim's band (`pocket`: destitute 0-1p
    to rich 5-25p; one in no band 1-5p as before), a player's within `theftADay` and the lean days as before. The Watch
    counts stolen goods at their quoted price where the one robbed buys (`Society::marketLot`, the Board's pressure in it;
    the catalogue's where its market is unknown), not the old meal 6p, herbs 2p, sword 40p and everything else 0.
  - **Begging, the dole, rough sleeping** (10.2, D12): on Marketday and Restday, 8:00 to 18:00, the destitute adults
    without a post or a master, not lifted, beg at their market's chapel steps (where the chapel's first door to the
    open air comes out) and by its notice board (the square), at most `street.beggarsACell` (2) a cell, by id; the task
    "begging" and the talk fact `job=beggar` while at it, so the existing beggar barks are theirs, and a beggar near a
    player barks first every other look. Each game minute a comfortable or rich resident within 3 tiles gives a penny by
    its band's `alms` chance (0.15, 0.3), once a day a pair, resident to resident (a staged move): never to a player.
    On Restday the hour after the service, the first clergy post held in each community hands its destitute and poor
    households `street.doleDays` (a day's food a head) in bread, a raw add into their wallets, trimmed with the rest, once a
    Restday a household; the priest is seen "handing out the dole" at the chapel steps; `on_dole` is true a week. Some
    winter nights (a third, by a seeded lot) the destitute with no home or a communal one sleep "by the warm wall" of a
    bakehouse, forge or kiln in their market (indoors or by its fire in the open; else its chapel), lying down there.
  - **Errand-runners and the standing-still rule** (6.6, 7.7, D21): a household's food at its hour from a till a player
    is in the cell of, or the next (the stage's cells, `World::tillWatched`), never moves the stock unseen: a member free
    to go (no post at its hours first) walks to the counter (task "fetching goods", so `errand=goods`), stands
    `World::RunnerSeconds` (8), and the shelf drops by the real pieces (`Society::runnerBought`); an empty shelf sends it
    home without, a short day for the poor. Only to an open shop (its keeper keeping it), at most 2 runners in a cell and
    12 in a market, behind the new rule `errandRunners` (1). With no runner to send, the food waits for the next hour; still
    watched at 17:00, the household is fed from elsewhere and the shelf left as it stands (a bare one still a short day for
    the poor). A runner not at the counter within a game hour gives up. Once a day a household.
  - **Staged buying besides food** (6.5, and Phase 5's leftover, import shelves thinning between ships): at its hour a
    household buys one of its market's everyday wares (non-food, set out by some shop there) by its band's new `shops`
    chance a day (0, 0.15, 0.35, 0.6), and one of `market.json`'s `luxuries` (cheese) by its `luxury` chance a week (the
    comfortable 10%, the rich 40%), raw moves from the shelf into a member's wallet, trimmed at night. So shelves fall only
    when someone buys, and an import's thins between its wagons or ships. At a watched till such a buy waits for the
    player to go, and goes undone at the shutting (no rich buyer or servant is shown). The `services` (`market.json`, a
    world's `service` records turning one off or changing its `every`) are done at `every` times the band's `services`
    (0, 0.25, 1, 1.5; a patron's only for the rich) at a business of their kind in the market: a sale for its business,
    nothing on a shelf. The Rules panel's "not in play yet" note is gone.
  - **Business** (10.3): each shop's pieces sold a day (staged buyers' and services) and players' purchases apart, saved;
    at the day's turn its word for the day gone: failing with a glut on its wares (mean P under −0.15); else thriving
    with a ware sold out before noon, a day's sales of half its shelves' targets or more, players' three days over its
    market's middle, or good or boom Times; else failing when two days' sales are under a twentieth of its targets, or
    hard Times for a luxury shop; else steady. "none" for a non-keeper and before a shop's first day's end (so a fresh
    world's talk is as it was). Three days failing running: the keeper's household's `failing` event (−0.3). The talk
    fact `business` reads it (the salience rule against "can't complain" is Phase 8's).
  - **The short trouble by band, the lifted state** (6.7): a poor or destitute head not lifted is short (no `foodDays`);
    its coins are a fortnight (`refillDays`) of its band's food a head at its market's price, each head's day never more
    than a day's need (`Society::bandFoodCost`), at least 10p; its days what it holds against that day's cost. A gift
    solves it when worth its coins (`troubleGiven` takes the gift's worth; the plain reckoning stays for a household in no
    band). A player's gift to a poor or destitute household worth a week's band food lifts it a week, a month's a month;
    the trouble's help lifts it its fortnight; at most half its market's destitute share at once (never fewer than one;
    `Society::liftByHelp`). Lifted: no begging, no short days (as before), no thieving, `hungry=no`; the one lifted owes
    the helper the help's worth both ways in their bonds, so the `pair.owed` lines can fire (T15). `fed_household` fame
    once a week per account holder and household (`Society::fedFameRoom`, saved).
  - **Saves and the journal:** `StagedState` gains `dole`, `business` and `fed`, each written only when it has
    something and read leniently (bad entries dropped or clamped, never refused); journalled with the staged rest
    (`wire::stagedDiff`, `stagedRest`), pruned nightly. Every move of a till's shelves by a staged sale, a till theft or a
    player's trade notes the till (`Society::noteSale`, `noteTillMoved`), written whole with the Board in the next record:
    the trade-moved shelves that Phases 1 and 4-5 left ("Still not journalled" in the merge's entry) are journalled now. A
    tick tells no player anything new, so nothing waits on `takeNotices`. Not journalled, as the rules have it: the
    staged wallets the dole, the alms and the buys move (a crash can lose a household's bread, never a player's
    anything), an NPC's theft of an NPC on a tick (its incident goes with the next Crime record, as before), and what
    waits in memory (a runner on the way, a buy waiting: the household's two days on hand cover it).
  - **Fair to players, money real:** no NPC targets a player (D11); alms and the dole are residents' only; a player's
    theft from a staged pocket is a registered faucet within `theftADay`. No new faucet: `realBalanced()` holds in every
    new test.
  - **New rules and data:** `theftCap` (0-20, 4) and `errandRunners` (0-1, 1) in `RatwItems.h`, `market.json`,
    `map_editor.py` (a switch), the Editor's `economy.ts` (and its test's fixture) and `model.mjs`; `status.json`'s rows
    gain `shops`, `luxury`, `services`, `alms`, and a `street` block (`theftBase`, `theftTimes`, `theftHonesty`, `slyRich`,
    `doleDays`, `beggarsACell`), all read by `status::read` with clamps; `market::Data` reads `luxuries` and `services`.
  - **Tests:** `street_tests` (new, 7,834 checks: the day's thieves within the cap and by the slider, a hamlet's, the
    same lots each time; a hungry thief's food from a till; a rich pocket's take within the cap and goods worth their
    quoted price; beggars only destitute adults without a post, two a cell, alms to residents only and never to a player
    beside them, none on a working day; help lifting a beggar (no begging the next Marketday, the debt of thanks); the
    Restday dole and `on_dole`, nothing for a player; rough sleepers some winter nights only; a staple bought out leaves
    the poor fed less and the comfortable the same; the standing-still rule in the society (a watched till untouched
    without a runner, a runner's real pieces) and in Greyfen (a player in Wren's shop all day: every drop with a runner at
    the counter, two at most in the cell); households buying everyday wares; business words and the failing event; the
    dole once a Restday and a week of `on_dole`; lifts by a week's and a month's food, the cap, never a comfortable
    household; fame once a week by holder; the short trouble by band; the new state through the wire, an older one read as
    none, a bad entry dropped; four crashes (the dole, a lift, a day's business and a player's purchase, the households'
    buying on ticks), each after a checkpoint, then a player's record, then the game gone, restarted the same in file
    mode) and `street_pg` (the four through PostgreSQL by `game.save_checkpoint` and `game.journal`, 27 checks, a
    throwaway `ratw_test_s2stage_*` database, dropped after); each crash case seen failing with its journalling taken out
    (the dole's diff, the trade-moved till). `save_compat` 23,107 (23,085: every pre-79 fixture has no street state, and a
    Phase 5-shaped save, the street's keys taken out of a checkpoint with them, reads as none and plays on; one with them
    comes back the same through the checkpoint and the world). Changed for the phase: `crime_tests` 85 (its hungry
    thieves are the day's planned ones now: Greyfen's theft base made high), `fame_tests` 80 (runners only to an open
    shop kept its talk check quiet; no change to the test), `troubles_tests` 66 (the short trouble's coins by band), the
    Editor's rule fixture (the two new rules). A clean Release build (`build-p6c`; no new warning); ctest 86/86 with the
    database, none skipped (`pg_client` on a throwaway `ratw_test_s2stage_p6`, dropped after), twice (once on the working
    build); `life_status_tests` 2,244, `status_tests` 21,962, `market_tests` 2,001, `port_tests` 9,103,
    `trade_show_tests` 869, `exploit_tests` 871,756, `game_tests` 3,631, `shops_tests` 132, `host_edit_tests` 550,
    `roads_tests` 407, `journal_tests` 34. All 34 `tools/test_*.py` with the database and this build's server (throwaway
    names tagged `s2stage`, none left): 668 tests, none failing, 25 skipped (the 23 `RATW_SLOW` world builds,
    `test_game_tables`' save under `Saved/`, `test_live_map`'s built client; `test_live_map` through `python3 -m
    unittest` in `tools/`). The client's 146 and its typecheck; the Editor's 81 + 67 and its typecheck.
  - **Perf** (DEV build 70 exported read only, `world_check EXPORT --simulate A B --players 20`, Release, 8 threads;
    Phase 6 against its base `8399ecc` built from the same tree, runs alternating; the machine at load 1.4-3.7 from other
    sessions; the runs at load 4.8-12 left out; ms, ranges over the runs):

    | Window | build | mean | p99 | p99 after the first minute | worst after the first minute | worst |
    |---|---|---|---|---|---|---|
    | 6-8 | repaired (its table) | 2.64 | 5.84 | 5.45 | 12.2 | 39.9 |
    | 6-8 | base today (4) | 2.71-2.96 | 5.95-6.95 | 5.57-5.98 | 11.7-29.8 | 38.8-79.0 |
    | 6-8 | Phase 6 (4) | 2.69-2.85 | 5.90-6.36 | 5.55-6.01 | 10.9-13.0 | 37.1-39.7 |
    | 17-19 | repaired | 2.19 | 5.15 | 4.95 | 23.2 | 40.4 |
    | 17-19 | base today (6) | 2.15-2.31 | 5.09-5.68 | 4.92-5.34 | 22.1-24.3 | 36.8-40.9 |
    | 17-19 | Phase 6 (6) | 2.14-2.35 | 5.14-6.38 | 4.96-6.23 | 22.5-24.5 | 36.8-41.7 |
    | 23-25 | repaired | 1.63 | 4.85 | 4.28 | 12.6 | 40.3 |
    | 23-25 | base today (4) | 1.69-1.90 | 4.74-5.84 | 4.11-5.17 | 12.8-13.2 | 35.7-41.9 |
    | 23-25 | Phase 6 (3) | 1.72-1.87 | 4.75-5.17 | 4.19-4.49 | 12.4-13.2 | 38.9-40.3 |

    Phase 6 sits inside the base's own spread in every window but one run's p99 in 17-19 (6.38, after the first minute
    6.23, at load 2.2; its pair's base 5.48 and 5.22; the five other runs 5.14-5.62), and the base itself measures up to
    0.1-0.3 ms over the repaired table today (the machine, not the code: the base is the repaired code). One more 17-19 pair, with the
    errand-runners switched off by a temporary build, measured the same as with them (2.22 both times, against 2.14 and 2.20 with them): the
    runners near the 20 walking players cost nothing measurable. The worst first-minute tick is tick 19's, as the repaired
    entry says. `world_check` has no Game and no journal: the trade-moved tills a record now carries are not in the gate.
  - **Left, by owner:** Phase 7: none of this phase's. Phase 8: the talk lines for begging beyond the existing
    `job=beggar` barks, the dole, the business words (the `business=failing` salience rule against "can't complain"), and
    `wealth_days` (still `foodDays`, the talk's fallback for one in no band). Phase 9: the DM's side (the street's
    `status.json` block and rows and the two rules through `live.economy.rules`; thefts, beggars and the dole on the
    Prosperity tab). Phase 10: every number here (the theft base and Times, the shops, luxury, services and alms chances,
    the dole's days, the business thresholds, the runners' caps), with play. Phase 11: the soak with street life (not
    run).
  - **Not built, and why:** a rich buyer or a servant shown buying near players (a buy besides food waits for the player
    to go instead: no visible change without a buyer, as the rule asks); alms as a visible gesture (a purse's penny);
    NPCs picking players' pockets (D11: out).
  - **For the user to decide** (each built one way, cheap to change): (1) the short trouble's coins on a day's need, not
    the band's two meals (a fortnight of those was 925p for a household of five in `troubles_tests`); (2) the business
    thresholds above; (3) a market's thefts go no further than 21:00 and begin at 8:00; (4) a beggar's places: the chapel
    steps and the notice board (on DEV, Ridgemere's cathedral or chapel door and its square).
- **2026-11-11: Phase 6 repaired** (`r2-staged-p6`). Three independent checks left 26 findings (some the same one found
  twice) and a list of what was missing. Each was looked into against the code; each one fixed has a test that fails
  with its fix taken out and passes with it (each was seen failing so). No economy simulation was run; no server was
  started; DEV was only exported read only (build 70) for a probe of a Marketday morning (seconds of ticks) and the perf
  windows.
  - **The beggars, the same few at any slider and most of them Watch guards, fixed** (`beggarsAndAlms`). Two places a
    market at two a cell held DEV's city to four beggars (a DEV city cell is a quarter of the city), in id order. Beggars
    now also stand before the market's shops, at most `beggarsACell` at one place, places `World::BeggarApart` (8) tiles
    apart, in a seeded order of the day. DEV's patrol guards written with no work spot load as civilians with no post (the
    loader's rule), so nothing knew them as the Watch: `ResidentSpec::watch` keeps what they were written as, and they
    never beg, steal, or are a pickpocket's mark (`guardsNeverThieve`). On DEV build 70, a Marketday morning: 9 beggars at
    −100, 6 at 0, 2 at +100, none of the Watch (before: 4 at each). Three beggar lines that put a housed beggar under a
    bridge, and the dole "at dusk", reworded.
  - **Luxuries never bought, a third of shops failing in ordinary times, the failing event every night, services always
    thriving, poor wolves taking steel swords, the record stepping back a day: fixed** (`businessInOrdinaryTimes`,
    `wareBuyingByBand`, `failingOnceARun`, `businessRecordNeverStepsBack`, `businessFromSales`). Cheese is a luxury before
    it is the rich's food. The foods besides the staples (meat, greens, preserves) are everyday wares; a household buys
    its band's `shops` pieces a day (0, 0.3, 1, 2), each of those its band can afford, picked by its shelf's target; the
    poor's services are the barber's only (`market.json`'s `poor`, so the Rules panel's hint is now true). The business
    word is judged against each shop's ordinary trade (10.3's repaired note): in a market of eleven shops, 2% failing at 0,
    15% at −100, 42% thriving at +100 (before: every butcher and greengrocer failing, every service shop thriving, and
    every shop thriving at +100). The failing event once a run; a sale between midnight and dawn counts in today's.
  - **The Restday dole, an instant city-wide add, fixed** (`doleQueue`, `restdayDole`): a queue at the steps with a player
    near, given at once out of sight, the rest at its end (10.2's note).
  - **Thefts** (`hungryThievesAtTheTill`, `liftedThievesStop`, `aFreshWorldsFirstDay`): a hungry thief robs a till only
    while its keeper keeps the shop; "hungry" is less than a day's food in hand (two days held made it never so); a
    household lifted after the draw steals no more that day; restitution of a till's food goes back on its shelf; a fresh
    world's first day has its thieves (the empty plan was kept for the day, so no perf window had a thief; on DEV the
    plan is now made on the first day: 4 thieves at −100, 1 at 0 and +100). **Rejected:** that the planned thefts mostly
    fail: the odds are the old ones, as 10.1 asks ("unchanged: everything after the target is chosen").
  - **Rough sleepers inside bakeries and on one tile, fixed** (`roughSleepers`): outside, in the warm building's doorway's
    shelter, side by side, at most `beggarsACell` to a wall.
  - **Errand-runners:** one giving up past the shops' last hour left its household unfed, the comfortable too: fed from
    elsewhere at once now (`runnerGivingUpLate`). Built (6.6, 7.7, missing): a runner goes to another open shop selling
    its household's food when its own is shut; a buy besides food at a watched shop sends a household member to the
    counter (`buyersAtAWatchedShop`; in Greyfen's working hours nobody is free, so its buys wait for the player to go).
  - **Help with the lift cap full, fixed** (`helpWhenTheCapIsFull`): the help isn't taken ("too many households here are
    living on help already"), and a gift lifts nobody and solves no trouble; a gift lifts by its worth as the help does (a
    fortnight's food a fortnight: `liftsAndTheirCap`).
  - **The dole's mark and fed fame through a crash, fixed** (`streetThroughACrash`, `streetStateRoundTrips`): neither is
    journalled any more, so each goes with what it marks (the dole's raw bread, a deed kept only by the checkpoint); a
    household whose bread a crash took is given it again within the dole's hours, once.
  - **Alms in the ledger and the journal, fixed:** a staged move between two purses. **Cleanups:** `marketLot` starts its
    ladder by `Society::ladderStartIn` (what `ladderStart` uses); the shelf-key rule is `Society::keptAtZero`, used
    everywhere it was copied. `street_tests` removes every copy and save it made (the `.lock`, `.art`, `.reports` too),
    passed or failed. **Not done:** the 360 older `ratw-street-*` entries already in `/tmp` were left (removing them was
    not allowed from this session).
  - **Left, by owner (unchanged):** Phase 8: the begging, dole and business talk lines, the `business=failing` rule against
    "can't complain", `wealth_days`. Phase 9: the DM's side. Phase 10: every number. Not built: alms as a visible gesture
    (a client gesture, not the server's to show), NPCs picking players' pockets (D11). The dole goes to lifted households
    too (the plan says neither way). DEV's patrol guards written with no work spot (the check counted 28) still score as households without an earner
    (a data question for the user: they are kept from begging and theft, not from the bands).
  - **Tests:** a clean Release build (`build-p6rc`; no new warning in the files touched); ctest 86/86 with the database,
    none skipped, twice on the working build and once on the clean one (`pg_client` on a throwaway
    `ratw_test_s2stage_p6r`, dropped after; no `s2stage` database left); `street_tests` 12,997 (7,834; the final check counted 13,331: the crash cases' loops run until the shelves move, so the
    count varies a little run to run),
    `street_pg` (four crashes through PostgreSQL), `save_compat` 23,107, `life_status_tests` 2,244, `status_tests` 21,962,
    `market_tests` 2,001, `port_tests` 9,103, `trade_show_tests` 869, `game_tests` 3,631, `crime_tests` 85,
    `troubles_tests` 66, `fame_tests` 80, `shops_tests` 132, `host_edit_tests` 550, `roads_tests` 407, `journal_tests` 34,
    `exploit_tests` 871,756. All 34 `tools/test_*.py` with the database and this build's server (keys tagged `s2stage`):
    667 tests, none failing, 25 skipped. The client's 146 and its typecheck; the Editor's 81 + 67 and its typecheck.
  - **Perf** (DEV build 70 exported read only, `world_check EXPORT --simulate A B --players 20`, Release, 8 threads; the
    as-built Phase 6 (`build-p6c`) against the repaired (`build-p6rc`), runs alternating, two each, the machine at load
    1.1-1.7; ms, ranges over the runs):

    | Window | build | mean | p99 | p99 after the first minute | worst after the first minute | worst |
    |---|---|---|---|---|---|---|
    | 6-8 | as built | 2.67-2.67 | 5.76-5.87 | 5.38-5.45 | 10.8-21.2 | 36.3-37.5 |
    | 6-8 | repaired | 2.63-2.67 | 5.77-5.78 | 5.40-5.42 | 12.0-12.1 | 38.0-38.0 |
    | 17-19 | as built | 2.15-2.18 | 5.15-5.20 | 4.94-4.96 | 22.0-22.2 | 39.4-39.5 |
    | 17-19 | repaired | 2.15-2.16 | 5.10-5.15 | 4.94-4.95 | 22.7-23.0 | 39.4-40.0 |
    | 23-25 | as built | 1.67-1.69 | 4.64-4.79 | 4.14-4.17 | 12.4-12.7 | 37.5-37.6 |
    | 23-25 | repaired | 1.60-1.72 | 4.50-4.70 | 4.03-4.22 | 12.8-13.3 | 35.9-37.5 |

    The repaired build sits inside the as-built one's spread in every window, now with the day's planned thieves in it
    (their target search and the till path) and a city's households buying wares each hour. The worst first-minute tick
    is tick 19's, as before. `world_check` has no Game and no journal: the tills a record carries are still not in the
    gate.
- **2026-11-11: Phase 6 repaired again** (`r2-staged-p6`, the final check's seven findings and two notes). Each one
  fixed has a test that fails with its fix taken out and passes with it (each seen failing so); the two DEV-only ones
  were also confirmed on DEV build 70 exported read only, by a scratch probe outside the repository (seconds of ticks
  on a Marketday morning, six winter nights and a Restday's dole at −100, 0 and +100; before and after). No economy
  simulation was run; no server was started.
  - **The Watch off the street and out of the poor bands, fixed** (`theWatchInTheBands`, `theWatchOffTheStreet`). One of
    the Watch, posted or not (`ResidentSpec::ofTheWatch`), is an earner in the bands (its town pays it), and a household
    with one of the Watch in it is at least comfortable under any guarantee but the DM's override (6.3's repaired note,
    **for the user's nod**: posted guards are kept comfortable too, so a postless one is never better off). Besides the
    beggars, thieves and marks: never a rough sleeper (`placeSleepers`), never in a community's dole list or queue,
    `giveDole` refuses a Watch household (`Society::watchHousehold`), so even the DM's override gives it none; a postless
    guard at its rounds is never sent as a runner. On DEV: the 16 postless guards were destitute or poor (0: 5 and 8;
    −100: 9 and 4; four posted ones poor at −100 too), one slept by a warm wall at −100, and 5 (0) and 17 (−100) of the
    Watch were on the dole; now all 30 are comfortable at −100 and 0 (two rich at +100), none sleeps out, none is on the
    dole. Every street and band path was looked through: the bands, beggars, thieves, marks, sleepers, the dole's list,
    queue and gift, runners and buyers (`freeMember`), the short trouble and the lifts (by band, so they follow).
  - **Rows on one tile on raised ground, fixed** (`streetOnRaisedGround`, Greyfen's streets raised to 2.5). The tiles
    beside a place are found by `World::freeBeside`: ground someone could stand on (`standable`) and step to from the
    place's own (`passable` from its tile), not taken already, the place first and then ring by ring (a queue along the
    steps first); in a cell not loaded, by the order alone. Beggars, sleepers and the dole's queue use it, the queue's
    places worked out each hour round the priest's tile and the beggars'; the chapel door's mouth is left to the priest.
    On DEV, Marketday: at 0, 6 beggars on 6 tiles (before 5 on 3), at −100 9 on 9 (10 on 5); the Restday dole with a
    player at the steps, the queue, the priest and the beggars on 13 tiles for 13 at 0 (before 14 on 4) and 16 for 16 at
    −100 (17 on 5).
  - **The beggar cap's words** (for the user's nod): kept as at most `beggarsACell` (2) at one place, places 8 tiles
    apart; `status.json`'s `about`, `RatwStatus.h` and 10.2 say so now (10.2's "2 a cell" marked as changed).
  - **A crash after the dole's hours, fixed** (`streetThroughACrash`'s new `dolelate` case, file and PostgreSQL through
    `street_pg`): on a Restday past the dole's hours, a community whose dole this run never closed (a restart) gives its
    unserved households their bread then, once (the priests' helpers' round, as at the hours' end).
  - **The runner's fallback shop, fixed** (`runnersToTheNearest`): `World::runnerShop` (split out of `sendRunner`) takes
    the open shop of the market with the household's food nearest its door by the street (a shop's door, a stall's spot,
    in world tiles), not a hash's pick.
  - **`roughSleepers`' bound, fixed:** it counts each sleeper's winter nights and bounds the four's sum to a third
    within three spreads (24-56 of 120; 38 now), each some nights and not all; it fails with every night slept out.
  - **The beggars on DEV, a data question for the user (no code change: the bands' rules work as 6.1-6.3 say).** With
    the Watch gone from them, DEV's beggars are (at 0) two postless "hands" at the barracks, `rm_hilde_dripby` (61, no
    work label) and `rm_piet_muddale` (58, "working at Cinder Wheelwrights"), authored as workers but holding no post, so
    each a household of one with no earner; and four elders, 60-80, living alone or as a pair with no earner
    (`rmx_ingrid_kendrick` and `rmx_una_thackeray` in their `_house` homes, which read as cottages: "house" is not one of
    the house words). "No earner in the home" is a reason for destitute (6.3), and a lower-class cottage or flat implies
    poor, one band from destitute. To change it: give the two hands their posts (a data fix), or count those past
    retirement age as not needing an earner (a pension or kin's keep: a rule change for the user to decide).
  - **Noted, not changed:** an NPC's till theft journals the till's shelves with the next record; its incident goes with
    the next Crime record and the thief's staged wallet is not journalled (as the journal rules have it for a theft of an
    NPC by an NPC on a tick).
  - **Tests:** a clean Release build (`build-p6vc`; no warning in the files touched, 125 in all as before); ctest 86/86
    with the database, none skipped, twice on it and once on the working build (`pg_client` on a throwaway
    `ratw_test_s2stage_p6v`, dropped after; no `s2stage` database left); `street_tests` about 24,600 checks (24,489-24,668:
    the crash cases vary as above), `street_pg` five crashes through PostgreSQL (49 checks), `save_compat` 23,107,
    `life_status_tests` 2,244, `status_tests` 21,962, `market_tests` 2,001, `game_tests` 3,631, `crime_tests` 85,
    `troubles_tests` 66, `fame_tests` 80. Changed for the repair: `aFreshWorldsFirstDay` (Greyfen's barracks guards weigh
    as comfortable among the thieves now: its comfortable theft weight made high, `theftBase` being clamped at 1). All 34
    `tools/test_*.py` with the database and this build's server (keys tagged `s2stage`): 668 tests, none failing, 25
    skipped. Perf not measured again: the placements are hourly and the queue's places a few dozen tile checks an hour.
  - **Not done:** other suites' leftover files in `/tmp` (`ratw-books-*`, `ratw-fame-*`, `ratw-troubles-*` and others,
    not street life's) were left; removing them was not allowed from this session.
- **2026-11-11: Phase 6 merged** (`r2-staged-p6` at `aa4c4de` into `r2-staged` at `60293db`: merge `b8de188`, then
  `603a46f`). No economy simulation was run; no server was started; DEV was only exported read only (build 70) for the
  perf windows.
  - **The merge.** One conflict, in `World::ensureTradePoints`: Phase 6's street hooks (a watched till, a runner, a
    buyer: `setStreetHooks`, set again when the world has moved) come first, then the markets are found again only when
    the roster's places or the dead ends' revision moved (`tradePointsDeadEnds_`, the repair's), as both sides meant; the
    print check (`tradePointsPrint_`) and `++tradePointsBuilt_` stay after it. Everything else merged by itself: the
    tills a staged sale, a till theft or a restitution moves (`noteSale`, `noteTillMoved`) and those an arrival or a
    departure moves share `takeTillsMoved`, written whole with the Board in any record of the economy or the roads;
    `stagedRest` carries `business` beside the Phases 4-5 rest (the dole's mark and fed fame still go by the
    checkpoint, as Phase 6's repair has it); the boards' notices and the residents made at a checkpoint are as the
    repair left them.
  - **Checked under the merged rules, two changes, each with a test seen failing without it:**
    - *A till theft seen is a move* (`street_tests` `aTillTheftSeenIsMoved`). Phase 6 adds no notice of its own, but a
      player who sees a hungry thief take food from a till is told of goods the tick moved (the till's shelves, journalled
      with the next record). That witness notice now says so (`World::notice`'s `moved`), so the host journals the economy
      before telling it and holds it in order, as a raid's; a crash no longer leaves a player having seen food taken that
      is back on the shelf. A pocket theft's notice moves nothing journalled (staged purses, as the journal rules have
      it) and is told as it comes; every tick notice goes through `beginHolding`/`endHolding` either way. (The incident
      itself still goes with the next Crime record, as before.)
    - *The street off the doorways* (`beggarsOffTheDoors`, and door checks added to `beggarsAndAlms`, `doleQueue` and
      `roughSleepers`). `World::freeBeside`, which places the beggars, the dole's queue and the rough sleepers, asked only
      whether a tile could be stood on, so a door standing open when the hour's places were found was a place: with
      Greyfen's Long House door open and four a place, the third beggar at the chapel steps stood in its doorway (where
      the door shut on it, and a player closing it is told someone is crossing the threshold). It now keeps off every
      door's tile, open or shut (`World::inDoorway`), as a square's stalls and crowd do since the repair.
  - **Checked, nothing to change:** the arrivals' given tiles (`takeResidentRequests`, `freeSpotNear`'s `given`) are
    homes to sleep at and Phase 6 places nobody through them: beggars, the queue and the sleepers stand by `freeBeside`,
    the runners and buyers at a counter, the priest at the steps (a door's arrival, never its tile); an arrival given no
    home is one of the homeless Phase 6 lets beg or sleep by a warm wall, as built. The squares checked at each day's turn
    are what `boardSpot` reads for the beggars' "at the market". The errand-runners and the standing-still rule are as
    built (`runnersToTheNearest`, `standingStillInTheShop` pass on the merge).
  - **Perf** (DEV build 70 exported read only, `world_check EXPORT --simulate A B --players 20`, Release, 8 threads, one
    run each on the clean build, the machine quiet (load 0.3 at the start); ms; the rows before from the entries above):

    | Window | build | mean | p99 | p99 after the first minute | worst after the first minute | worst |
    |---|---|---|---|---|---|---|
    | 6-8 | Phases 4-5 checked | 2.85 | 6.23 | 5.90 | 12.3 | 41.6 (tick 19) |
    | 6-8 | Phase 6 repaired | 2.63-2.67 | 5.77-5.78 | 5.40-5.42 | 12.0-12.1 | 38.0 |
    | 6-8 | merged | 2.59 | 5.77 | 5.52 | 11.4 | 36.8 (tick 19) |
    | 6-8 `--ship-day` | Phases 4-5 checked | 2.64 | 5.88 | 5.50 | 11.9 | 38.7 (tick 19) |
    | 6-8 `--ship-day` | merged | 2.54 | 5.72 | 5.45 | 10.9 | 37.0 (tick 19) |
    | 17-19 | Phases 4-5 checked | 2.13 | 5.09 | 4.94 | 22.1 | 36.9 (tick 19) |
    | 17-19 | Phase 6 repaired | 2.15-2.16 | 5.10-5.15 | 4.94-4.95 | 22.7-23.0 | 39.4-40.0 |
    | 17-19 | merged | 2.16 | 5.16 | 4.96 | 22.7 | 39.6 (tick 19) |
    | 23-25 | Phases 4-5 checked | 1.61 | 4.67 | 4.11 | 13.4 | 38.6 (tick 19) |
    | 23-25 | Phase 6 repaired | 1.60-1.72 | 4.50-4.70 | 4.03-4.22 | 12.8-13.3 | 35.9-37.5 |
    | 23-25 | merged | 1.69 | 4.69 | 4.18 | 13.1 | 36.7 (tick 19) |

    Every window is within the two sides' own runs. 17-19's worst after the first minute is a place loaded (14.1 ms of
    streaming), 23-25's the day's careers pass at midnight (12.3 ms of schedules), as before. `world_check` has no Game
    and no journal: the record before a till theft's notice is not in the gate (it comes only on a tick with a till
    theft a player saw).
  - **Tests:** a clean Release build (`build-p6mc`; 125 warnings, as before, none in the lines touched); ctest 86/86 with
    the database, none skipped, twice on it (and once without the database on the merge alone, `build-p6m`, before the
    two changes; `pg_client` on a throwaway `ratw_test_s2stage_p6m`, dropped after; no `s2stage` database left);
    `street_tests` 25,752 (the crash cases vary run to run, as before), `street_pg` five crashes through PostgreSQL (49
    checks), `save_compat` 23,107, `trade_show_tests` 919, `port_tests` 9,111, `journal_tests` 39, `stream_tests` 285,
    `gathering_tests` 128, `market_tests` 2,001, `game_tests` 3,631, `crime_tests` 85, `troubles_tests` 66,
    `fame_tests` 80, `life_status_tests` 2,244, `status_tests` 21,962, `farm_people_tests` 4,879, `roads_tests` 407,
    `host_edit_tests` 550, `shops_tests` 132, `exploit_tests` 871,756. All 34 `tools/test_*.py` with the database and
    this build's server (keys tagged `s2stage`, none left; `test_live_map` through `unittest`, `test_dm_live` with
    `RATW_SERVER` set): 668 tests, none failing, 24 skipped (the 23 `RATW_SLOW` world builds and `test_game_tables`'
    save under `Saved/`); `artifacts/screenshots/dev-console-fight.png`, rewritten by a tools test, restored. The
    client's 146 and its typecheck; the Editor's 81 + 67 and its typecheck (`node_modules` linked from the main checkout,
    removed after).
  - **Not done:** other suites' leftover files in `/tmp` (`ratw-projects-*`, `ratw-troubles-*`, `ratw-fame-*`,
    `ratw-game-test-*` and others; none of street life's) were left, as before. A player carried to the gaol on a tick
    (`takeIntoCustody`) is told so as it comes though the fine and restitution moved their coins: older than Phase 6 and
    not a street notice, so left for whoever next looks at the journal rules.
- **2026-11-11: Phase 6 merged, checked** (`r2-staged`, on `9c41d61`). Two independent checks of the merge left ten
  findings; eight fixed, each with a test seen failing with its fix taken out and passing with it, one rejected and one
  partly a data question. No economy simulation was run; no server was started; DEV was not opened.
  - **A crime a player is told of is journalled first, fixed** (`street_tests`' crash cases `tilltheft` and `custody`, file
    and PostgreSQL through `street_pg`; `aTillTheftSeenIsMoved` now checks the flag). `World::notice` takes a second flag,
    `crime`, beside `moved`, and `Game::tick` journals `Crime` with the economy and the roads (or alone) before telling a
    tick's notices, as for a move. Set by every witness notice (a till's theft, a pocket's, an attempt, a blow or a
    struggle heard), by a player carried to the gaol (`takeIntoCustody`, which is also a move: the fine and what is made
    good leave their own coins; the finding older than Phase 6), by "You are wanted" and by the release. Before: a player
    beside a till told "You see ... steal from ..." lost the incident in a crash right after (the record before the notice
    carried the shelves, not the crime), so had nothing to report; a player told they were dragged to the gaol came back
    with their coins, the warrant open and out of the gaol.
  - **The witness's words, fixed** (in `tilltheft`): "You see Piet steal from Imogen.", "You see Piet try to steal from
    Imogen.", "You see Piet attack Imogen." (a `sight` beside the residents' `claim`, which beliefs and rumours keep).
  - **Runners at a stall's counter, fixed** (`runnersAtTheStall`, a strip city with built stalls, the keeper's shop in the
    next cell). `World::counterSpot` (one for `sendRunner` and `sendBuyer`): at a stall the keeper keeps, the ground across
    the stall from the keeper (not the keeper's x + 1, the stall's own solid tile, where a runner stood still its hour and
    gave up), else the shop's serving spot; then each runner a tile of its own (`freeBeside`, the keeper's tile and the
    other runners' in that cell taken), so two to one shop never share one; a keeper at a stall in its shop's own cell is
    at its stall too now (the old rule asked for another cell).
  - **Beggars off the square's crowd and stalls, fixed** (`beggarsAndAlms`): the square's crowd and stall spots are taken
    before the beggars are placed, so the board's beggars stand beside the crowd, never on its spots (where the crowd
    pushed one onto another's tile). Students at their lessons were not looked at (no spot list to keep off).
  - **The dole queue's front beside the priest, fixed** (`doleFrontBesideThePriest`): the beggars at the chapel steps keep
    off the queue's first place (the tile beside the door's mouth, as `tendDole` finds it), so the front stands beside the
    priest and the beggars beyond it.
  - **Rough sleepers off a door's mouth, fixed** (`roughSleepers`: never on a door's arrival tile, some beside one): a
    warm wall's door mouth and the chapel's are taken before the sleepers lie down, as the chapel's is for the beggars.
  - **The Watch never an errand-runner, fixed** (`theWatchRunsNoErrands`): `freeMember` turns away every one of the Watch,
    posted or not, at its rounds or resting; its household sends another or waits for the next hour, as one with nobody
    free does. **For the user (Phase 10's data):** two households at −100 bought beams from the builders' yard as
    everyday wares; the catalogue's everyday wares are a data question, not changed.
  - **Rejected: a thief's till food vanishes in a crash.** The till's shelves are journalled, the NPC thief's purse is
    staged and comes back from the checkpoint: goods, not money (`realBalanced()` holds), and the same as a staged sale's
    (the buyer's staged purse is never journalled either, 9.2). Journalling staged purses on a tick is what 9.2 rules
    out; the incident now survives (above), and the Watch's restitution then makes good what the thief still has, in
    goods or worth.
  - **Tests:** a clean Release build (`build-p6fc`; 125 warnings, as before, none in the lines touched); ctest 86/86 with
    the database, none skipped, twice (`pg_client` on throwaway `ratw_test_s2stage_p6f` and `_p6f2`, dropped after; no
    `s2stage` database left); `street_tests` 27,029 checks (the crash cases vary run to run, as before; seven crash cases
    now), `street_pg` seven crashes through PostgreSQL (71 checks), `save_compat` 23,107, `journal_tests` 39,
    `game_tests` 3,631, `crime_tests` 85, `trade_show_tests` 919, `roads_tests` 407, `port_tests` 9,111, `market_tests`
    2,001, `status_tests` 21,962, `life_status_tests` 2,244, `troubles_tests` 66, `fame_tests` 80, `battle_tests` 558.
    All 34 `tools/test_*.py` with the database and this build's server (keys tagged `s2stagep6f`, none left): 668 tests,
    none failing, 24 skipped (the 23 `RATW_SLOW` world builds and `test_game_tables`' save under `Saved/`);
    `artifacts/screenshots/dev-console-fight.png`, rewritten by a tools test, restored. Perf not measured again: the beggars' and sleepers' places are hourly (a market's crowd and stall spots, a
    hundred tiles, added to a set), a counter is found as a runner is sent, and the crime's record comes only on a tick
    whose notices tell of a crime, which `world_check` (no Game, no journal) never has.
- **2026-11-11: Phase 7 (staged events and causes) built** (`r2-staged`, on `5afe23c`; `Core/RatwEvents.cpp`, new, beside
  the roads' and the port's files, whose machinery it reuses). No economy simulation was run; no real or scratch server was started (the tools' tests
  start their own); DEV
  and dev-old were only exported read only (builds 70 and 34) for the perf windows and `save_compat`'s full worlds.
  - **The scheduler and its list** (8.4, D25): `market.json`'s new `events` block, twelve kinds (8.4's seven: a House
    wedding, a fire at the works, a good catch, blight in the orchards, the festival's buying ahead, a lame ox holding a
    farm's cart, a ship's rich cargo; and a ship lost, the mill wheel stopped, the drovers in, the salt pans flooded, a
    brewing too many), each with its words, notice, crier's line, sign, goods' targets (within ±0.3), days (1-7), Times
    points (within ±20), days posted ahead, hour, weight and needs. Each market's week is `perWeek` (2) one more or fewer by
    a seeded score, × the new rule `eventRate` (percent, 0-300): 1-3 at the default, about six at 300, none at 0. A kind
    once a week at a market, one event a day, only goods its shops sell or buy, the days a festival's or a ship's event
    needs kept clear. Committed a week ahead on the Board (`events`, `eventsThrough`, `nextEvent`), as the ships are: the
    same world rolls the same events, and an event's words are filled as it is rolled, so a host's later edit of the list
    changes none already committed. A world with no port, no wagon and no farm (one market, no carters) has its events
    of the goods its shops deal in, never a ship's or a cart's.
  - **Each with its cause** (8.4, 8.6's row, T13): as it begins its goods are pressed halfway toward their targets
    (`setPressure`, cause "event": P stays in its range, the factor in the clamp, a player's own push capped as ever) and
    again each morning while on; a good catch is set out where a shop sets fish out (`arrivalRefill`, its tills journalled
    with the Board). Its notice is pinned ahead (`ahead` days) or as it begins, signed by its kind's sign, on the board a
    player at its market reads (as the carters' notices: never one of a board's 30 pins; its scent "of ink and of the
    market's many paws"), and comes down when it is over or cancelled. Its crier's line is cried at the market's square in
    the daytime, where a player in the square's cell hears it (the festival criers' `CrierCall`; the crier is the town's
    official, innkeeper or priest, or the same of those who work at the market; none, the notice alone). A crowd where it
    says (`here.crowd`), and talk's `world.event` (its kind), `world.event_words` and `world.event_when` (now, soon, past),
    in `facts.json`. Over after its days, talked of two days, then dropped; one whose days passed while the world was down
    is over quietly and makes room for the weeks to come (found by a festival's week after a calendar jump: fixed with a
    test).
  - **Ships** (Phase 5's leftovers): a ship lost is a story of its own (`Ship::lost`, saved: "is lost at sea" on the
    Wharf office's notice, `world.ships` "lost", its cargo a storm's +0.1 dearer and the event's more, the price list's
    "the Gull was lost at sea"); a rich cargo rides a ship on the timetable, begins as it docks and is called off if a
    storm keeps it out.
  - **The slider's cause** (8.5, T16): `Society::setProsperity(target, now, cause)` takes the DM's words (plain, 80 at
    most, never the forbidden word; else the seeded cause stands in and it returns false), and with none a move a new way
    takes one of `events.causes` for its way (three each way: the Crown's war levy, rot in the granaries, the hard
    winter; the silver strike in the hills, the peace with the north, the rich harvest), seeded by the day; a move on the
    same way, or the same words sent again (a sync), keep the cause. The Board keeps it (`sliderCause`); every market is
    told once (a notice by "the crier", its cry). `Society::timesScore` (5.3) adds the events on and the DM's nudge to the
    slider and the roads' shift, and `timesCause` names the largest part on the score's side; `world.times`,
    `world.times_cause` and the Mind's Means ("Times here: hard, since the Crown's war levy.") say it. The harvest roll
    leans ±0.08 the slider's way while it moves.
  - **Override notices and the DM's hooks** (12, Phase 9): `World::nudgeTimes(market, points, days, words)` and
    `overrideHarvest(area, word, days, words)` post their notices unless the new rule `overrideNotices` (1) is 0;
    `addStagedEvent(market, kind, day, hour)` (this week or the next, needs as its kind must have them) and
    `cancelStagedEvent(id)` for the Prosperity tab; `setProsperity`'s cause for the slider. No command, route or panel yet:
    Phase 9's. The two rules are mirrored in `RatwItems.h`, `market.json`, `map_editor.py` (`overrideNotices` a switch),
    the Editor's `economy.ts` (and its test's fixture) and `model.mjs`.
  - **The price list's cause words** (7.1's tooltip): "a good catch", "dear still, though a good catch", and the roads'
    and farms' causes in words where the raw word showed ("the wagon came in", "the wagon was robbed on the road", "the
    wagon's late", "a good harvest", "a short harvest, or the cart didn't come").
  - **Saves and the journal:** everything an event, a cause or a nudge moves is on the Market Board, which every record of
    the economy or the roads writes whole when it differs and `journalCheckpoint` writes as a checkpoint is taken; the
    tills a catch sets out go with it (`takeTillsMoved`); the boards' notices go whole with the same records. No tick
    notice: nothing waits on `takeNotices`; the crier's lines are voices, not saved (a cry not yet cried when the server
    stops is not cried after it). At a start `mendRoadNotices` puts an event's notice up or down by the Board. Every new
    Board field is written only when it has something and read leniently (`mendEvents`: a lying event's words with the
    forbidden word drop it, statuses and numbers mended, one of an id, 16 a market, 256 in all, the next id above every
    one); at a restore events of a market gone or past the weeks rolled are dropped. A Board from before Phase 7 reads as
    none, and its events are rolled from that day.
  - **Money:** events never touch money, bands' targets or posts; no faucet in them; `realBalanced()` holds through three
    weeks of events at 300% with a player trading, and buying a good where an event made it cheap and selling it back
    anywhere never pays.
  - **The forbidden word:** every host list (events, causes, the cause templates) as the loader takes it: an id by the name
    rule (`forbiddenInName`, the letters anywhere: "seadog_feast" is out), words, notices, cries and signs by whole words
    (`forbiddenWord`); a filled event whose words fail is not made; the DM's cause by the same.
  - **Changed for the phase:** `trade_show_tests` and `port_tests` run with `eventRate` 0 (they pin the roads' and the
    port's own steps, which an event's pressure, refill or lost ship would move; a good catch at West filled its shellfish,
    so the wagon's manifest changed); `gathering_tests`' "the board is bare" means bare of the players' own notices.
  - **Left, by owner:** Phase 8: the talk lines for events, the slider's causes, the Times and lost ships (the facts are
    there), and the price topics' cause words. Phase 9: the DM's side (commands and the Prosperity tab over the hooks
    above, the slider re-meant with its cause through `economy.sync`, `eventRate` and `overrideNotices` in the Rules
    panel's copy, events and causes in `live.economy.rules` and the world package's scan). Phase 10: every number (the
    kinds' targets, days, Times points and weights, `perWeek`, the harvest lean). Phase 11: the soak with events (not run).
  - **For the user to decide** (each built one way, cheap to change): (1) an event's Times points count in its market's
    score (5.3), so in the week it is on it can tip the bands' cut, though 8.4 says events never touch bands; (2) "city"
    kinds (the House wedding) only at the realm's largest market; (3) a move of the slider back toward 0 takes a cause of
    its own way ("up"), even while the times are still lean; (4) a market with no official, innkeeper or priest has no
    crier, only the notice; (5) a good catch sets out fish only where a shop sets them out (a stall that only buys them
    from players has none to set out); (6) the DM adds events only this week or the next (the weeks a restore keeps).
  - **Tests:** `events_tests` (new, 3,342 checks: the list and its checks, the host's bad entries left out; 1-3 a week,
    the same world the same events, the rule at 0 and 300; an event begun, pressed, refilled, in the Times and the talk,
    the crowd, the list's cause, inside the clamp, the player's push capped, pressed again, over, dropped, called off; the
    slider's cause, the DM's words and the forbidden ones, told once, `times_cause`, the Means, the harvest's lean; the
    nudge and harvest override with the rule on and off; a rich cargo and a lost ship, a storm calling one off; a lame ox;
    a world with nothing but a market; a festival's buying ahead; money real; the wire, a save, a Phase 6 save, a lying
    save; the notices, signs, scent and the crier through a game; five crashes, each after a checkpoint and again with a
    snapshot that doesn't land (an event begins, one is over, a morning rolls the week, a ship lost, the cause told),
    restarted the same in file mode, each seen failing with the Board's events kept out of the journal) and `events_pg`
    (the ten through PostgreSQL by `game.save_checkpoint` and `game.journal`, 280 checks, a throwaway
    `ratw_test_s2stage_*`, dropped after). A clean Release build (`build-p7f`; 125 warnings, as before, none in the lines touched); ctest
    88/88 with the database, none skipped, twice on it (once more on the working build; `pg_client` on throwaway
    `ratw_test_s2stage_p7*` databases, dropped after; no `s2stage` database left); `port_tests` 9,111, `trade_show_tests`
    919, `street_tests` 27,796, `market_tests` 2,001, `host_edit_tests` 614 (550: the fixtures' markets now have events),
    `roads_tests` 407, `exploit_tests` 871,756, `game_tests` 3,631, `journal_tests` 39, `gathering_tests` 128,
    `save_compat` 23,107 (23,123 with `RATW_PRE79_BUILDS`: DEV build 70 and dev-old build 34 exported read only, every
    fixture loaded whole). `events_tests`' count varies a little with what the seeded weeks roll (3,331-3,342). All 34
    `tools/test_*.py` with the database and this build's server (keys tagged `s2stagep7`), twice (once on the working
    build before the last two small changes in `RatwEvents.cpp`): 668 tests, none failing, 24
    skipped (the 23 `RATW_SLOW` world builds and `test_game_tables`' save under `Saved/`; `test_live_map` through
    `unittest`, `test_dm_live` with `RATW_SERVER`); `artifacts/screenshots/dev-console-fight.png`, rewritten by a tools
    test, restored. The client's 146 and its typecheck; the Editor's 81 + 67 and its typecheck (`node_modules` linked from
    the main checkout, removed after).
  - **Checked on DEV's data, read only** (DEV build 70 exported, loaded and run 21 mornings by a probe outside the
    repository, seconds of ticks each, nothing written): Ridgemere is the one market and a port; its weeks rolled a House
    wedding, a brewing too many, the drovers in, the mill wheel stopped, blight in the orchards, a fire at the works and a
    ship's rich cargo (11 events in three weeks); real money balanced.
  - **Perf** (DEV build 70 exported read only, `world_check EXPORT --simulate A B --players 20`, Release, 8 threads; Phase 7
    (`build-p7c`, the same code but for the rolling of a ship's event over its whole week, a weekly step) against its base
    `5afe23c` built from the same tree, runs alternating, two each; the machine quiet (load 1.0-1.4, but 7.7 and 3.1 at the
    first two, right after a build); ms, ranges over the runs; 765 residents):

    | Window | build | mean | p99 | p99 after the first minute | worst after the first minute | worst |
    |---|---|---|---|---|---|---|
    | 6-8 | Phase 6 merged (its table) | 2.59 | 5.77 | 5.52 | 11.4 | 36.8 |
    | 6-8 | base today | 2.60-2.61 | 5.74-5.79 | 5.40-5.44 | 12.0-12.6 | 38.4-39.2 |
    | 6-8 | Phase 7 | 2.61-2.62 | 5.73-5.86 | 5.32-5.49 | 12.2-12.5 | 39.1-39.6 |
    | 6-8 `--ship-day` | base today | 2.63 | 5.79 | 5.48 | 12.2 | 37.0 |
    | 6-8 `--ship-day` | Phase 7 | 2.67 | 5.78 | 5.39 | 12.1 | 38.9 |
    | 17-19 | Phase 6 merged | 2.16 | 5.16 | 4.96 | 22.7 | 39.6 |
    | 17-19 | base today | 2.11-2.16 | 5.20-5.21 | 4.98-5.03 | 21.8-22.4 | 38.3-39.3 |
    | 17-19 | Phase 7 | 2.14-2.18 | 5.18-5.18 | 4.97-4.98 | 21.9-22.1 | 39.3-39.7 |
    | 23-25 | Phase 6 merged | 1.69 | 4.69 | 4.18 | 13.1 | 36.7 |
    | 23-25 | base today | 1.64-1.64 | 4.45-4.69 | 4.00-4.22 | 13.0-13.0 | 36.4-40.5 |
    | 23-25 | Phase 7 | 1.58-1.66 | 4.38-4.76 | 3.92-4.26 | 12.7-12.7 | 36.0-38.9 |

    Phase 7 sits inside the base's own spread in every window: an event's steps are a few dozen Board writes a day, and the
    Times score's walk over a market's events is a few entries. The worst ticks are the first minute's tick 19, 17-19's
    place streaming in and 23-25's day turn, as before. `world_check` has no Game: the notices, the crier and the journal's
    records are not in the gate.
- **2026-11-11: Phase 6's beggars to the user's decision** (branch `r2-staged-beg`, cut from `r2-staged` at `5afe23c`;
  `8915fd8`). The user's words (10.2): "I think up to 5 beggars per cell is good, but only in the worst conditions. Beggars should be
  in the NPC list, typically someone from the lower caste that doesn't have money for food. Old people living with
  family can be an option but not the only choice." No economy simulation was run; no server was started; DEV was only
  exported read only (build 70) for a scratch probe outside the repository.
  - **The most in a cell, built** (`beggarsACellByConditions`): `status.json` `street.beggarsInACell`, 5 at −100, 3.5 at
    −50, 2 at 0, 1 at +50, 0.5 at +100 (straight lines; never rising, 2 to 21 points from −100 to 100, each 0 to 8, or
    the default whole with a warning), the market's Times moving its slider by `timesShift` as the bands' column moves,
    a fraction one more on that share of the cells by a lot of the cell and the day (`status::beggarsInACellAt`). An
    ordinary market, whose Times follow the slider: 5, 3.9, 2, 0.9, 0.5. `beggarsACell` (2 at one place, places
    `World::BeggarApart` apart) stands under it; `World::placeBeggars` gathers places until the cells' room holds the day's
    list and fills each place only while its cell has room.
  - **Who begs, built** (`beggarsByWant`, and `beggarsAndAlms` reworded to it): `Society::foodWant`, a household's want of
    food: the destitute always (1 + its shortfall), the poor when its purses and the food its members carry beyond
    today's would not buy a day's food a head (`NourishADay`, in its own food) at its market's price (its shortfall, a
    share of the day's cost); none for the comfortable and rich, a lifted household or the Watch's. Judged on the
    morning's purses and larders at the staged day's turn (`Society::stagedLarders`, not saved: worked out from the
    restored ones after a restart, and now for a household made since). Today's food counts as in hand because the staged
    ration keeps every household near two days' (6.5): judged on the larder alone, no poor household is ever short (on
    DEV none was, at any slider). Adults (16 or over) with no post and no master, never the Watch, never lifted, as
    before. The draw: a seeded lot of the day weighted by want times `beggarsClass` (lower 1, middle 0.5, upper 0.25);
    each group (an elder past its own retiring age, `Society::retireAge`, or of working age; by home kind: house,
    cottage, flat, room, communal or none) keeps `beggarsSpread` (0.5) of its weight for each one drawn from it already;
    and while anyone else in want waits the elders are at most `beggarsElders` (half) of a market's beggars, at least one.
    In the test town at slider 0 (two a cell, four of ten in want a day, over eight begging days): two elders and two
    others each day, nine of the ten seen, from cottages, the lodging and the flat; with `beggarsSpread` and
    `beggarsElders` at 1 it fails (three elders of four on the first day).
  - **Through a crash** (`streetThroughACrash`'s new `begs` case, file and PostgreSQL through `street_pg`; Greyfen with its
    lane, `StreetCrash::lane`): Idle 1 destitute at the checkpoint begs on the Marketday after it, a checkpoint there;
    after the restart she begs again, every beggar is in want and may beg, no cell over its most.
  - **On DEV (build 70, exported read only; a scratch probe outside the repository):** each slider on a fresh load, seven
    night turns jumped through (seconds of ticks each; a day's eating taken from each wallet before each, which the jumped
    hours skip), then a Marketday at 10:00. Beggars by cell, and who:

    | Slider (Times) | Most a cell | Beggars | By cell | Who |
    |---|---|---|---|---|
    | −100 (hard) | 5 | 8 | 5, 2, 1 | all 8 destitute, no earner: Hilde Dripby 61 and Piet Muddale 58 (lower class, the barracks, alone), Aud Kemp 80, Ivar Netherworth 65, Piet Lumley 60 (middle, cottages, alone), Sigrun Wickham 80 (middle, a flat), Ingrid Kendrick 68 and Una Thackeray 62 (middle, cottages, each with a child of 10-12 at home) |
    | −50 (lean) | 3.9 | 7 | 4, 2, 1 | the same but Ivar Netherworth |
    | 0 | 2 | 4 | 2, 2 | Hilde, Piet Muddale, Aud Kemp, Sigrun Wickham |
    | +50 (good) | 0.9 | 4 | 1, 1, 1, 1 | the same four, one in each of four cells |
    | +100 (boom) | 0.5 | 1 | 1 | Piet Muddale |

    Purses 0-6p against a day's food of 4p a head; no poor household short of a day's food at any slider (one at −50,
    with no adult free to beg). Every beggar is past its own retiring age (Piet Muddale's is 58): of DEV's adults with no
    post and no master in a household in want there are no others (the rest of its 24 destitute households at −100 are
    children of 8-15 with a post, each a household of one), so the elders'
    share has nobody to give way to and the class weight puts the two lower-class hands first. **For the user:** DEV's
    beggars stay elders until the world has lower-class adults with no post (the filling out to come), or a rule lets the
    working poor beg on Restday (not built: never one at a post, as the task set). DEV's children living alone as
    destitute households look like a data slip, left as found.
  - **Tests:** a clean Release build (`build-begc`; 125 warnings, as before, none in the files touched); ctest 86/86 with
    the database, none skipped, twice (`pg_client` on a throwaway `ratw_test_s2stage_beg`, dropped after);
    `street_tests` 27,010 and 27,274 checks (the crash cases vary run to run, as before; eight crash cases now),
    `street_pg` eight crashes through PostgreSQL (83 checks), `save_compat` 23,107, `status_tests` 21,962,
    `life_status_tests` 2,244, `market_tests` 2,001, `game_tests` 3,631, `crime_tests` 85, `trade_show_tests` 919,
    `port_tests` 9,111, `journal_tests` 39, `troubles_tests` 66, `fame_tests` 80. Perf not measured again: the larders are
    judged once a day (two quotes a household), the draw is a sort of a market's few in want each hour.
  - **Not changed:** 16.8's test list still says "at most 2 a cell" (outside this change's sections).

- **2026-11-11: Phase 8 (talk) built** (branch `r2-staged-p8`, cut from `r2-staged` at `12ee59d`). No economy simulation,
  no paid model call, no real or scratch server (the tools' tests start their own); DEV and PROD untouched. The as-built
  note is in section 11.
  - **Price topics back, at the shown price** (11, T1, T17): `Society::priceMove`/`priceMoves` read the shop list's own
    two ladders, so a move exists only where the list's price differs from its price at rest by a penny, and
    `topic.price` is that list's price in words ("fourteen pence the dozen", "seven pence", "a penny") for the
    overhearing traveller's own ladder. The market's three largest moves are `talk.prices` topics and set
    `world.<good>_price` (34 goods in `facts.json`, the Game drops others); `price_cause` comes back for a traveller's own
    buying or selling, named as the teller knows them. `topic.item` is text (herbs and meal keep their lines), with
    `topic.what` and `topic.is` in the plural where a good is counted by the piece.
  - **Cause attribution:** `topic.cause` and `topic.cause_words` from the Board's cause, turned the move's way; a cause
    the move belies is `still` ("though the wagon came in"). Writer P's audit changed 101 of the older price entries
    (55 by criteria): every line that named a cause (winter, hoarders, the caravans, the roads, the drovers, the catch) is
    gated on it or reworded; no figure, "twice" or "double" is left; "#topic.price# a bundle" and the like fixed (herbs are
    listed by the dozen); the price_cause scenes are the "down" ones. Seven older caravan scenes with no kind no longer
    speak for the news kind (one is the arrived kind only).
  - **The keeper's trade answer** (7.8) adds the ware whose shown price moved most, with its cause ("Nails are dear just
    now: the wagon's late."), and a keeper's prices are never replayed "as I said". **The Means** ends with the market's
    largest move and its shown price ("Salt dear, fourteen pence the dozen: the wagon's late.").
  - **New topics** in `sceneTopics`: the caravan's `due` (posted, or on the road here: `topic.when`, `topic.what`,
    `topic.who`) and `news` (the last wagon's) kinds and `topic.why` for a late wagon's mishap; `talk.ships` (in, due,
    late, lost, sailed); `talk.event`; `talk.times` (only in Times that aren't ordinary, as the Phase 7 checker asked);
    `talk.harvest`.
  - **Salience** (10.3): `settings.json`'s `salience` and the engine's `Library::muted`: words a speaker never says while
    its facts hold, any line holding them passed over for another alternative, a rule with none left not said. A failing
    shop never "can't complain" (nor "couldn't be better", "run off my feet", "sold out"...); the destitute never boast.
  - **Bands in talk:** `talkRichLine`, `richLine_`/`poorLine_` and the per-speaker food-days scan are gone for anyone in a
    band; the scan and `wealth_days` stay only for one in no band (a demo roster). `pair.richer` within a band compares
    the households' scores (half a point). The "short" trouble says itself by band (`troubles.json`'s `saidBy`).
  - **The wording** (Phases 4, 5, 7's singular goods): the carters' notices, a wagon's look, the events' `{goods}`, the
    wagons' news and `topic.what` say goods by `World::cargoWords` ("hides", "handbells", "nails"), as the ships' notices
    already did.
  - **New lines: 894 spoken lines in 296 entries** in four files (`Data/Talk/rules/economy8_*.talk`), over the plan's
    450-650 (the drafts were trimmed; the user may want them trimmed further by ear):

    | File | Entries | Lines | By concept (entries/lines) |
    |---|---|---|---|
    | `economy8_prices.talk` | 83 | 246 | talk.prices 42/144, talk.price_cause 6/21, barks on `world.<good>_price` 14/29, answers 21/52 |
    | `economy8_roads.talk` | 67 | 208 | talk.caravan 15/45 (due, late with its why, news, arrived, left), talk.ships 13/39, talk.harvest 9/27, Wharf and harvest barks 5/16, answers 25/81 |
    | `economy8_times.talk` | 87 | 259 | talk.times 19/57 (band x Times x class: Houses and the Council for the rich upper class, land and mills for the rich middle and lower and the towns'), talk.event 31/91 (all twelve kinds, now/soon/past, and generic), barks 3/9, answers 34/102 |
    | `economy8_street.talk` | 59 | 181 | beggars' barks and passers-by, the dole by class and the priest, business barks and talk, rough sleepers and the hungry (barks 26/81, talk 14/42, a helped beggar's greeting 1/4, answers 18/54) |

    The library: 3,759 scenes and 4,730 rules; `talk_report.py` still estimates about 117 hours before an everyday
    conversation repeats (the new lines are events, not everyday talk).
  - **The forbidden word:** `talk_tests`' `theForbiddenWord` reads every line of `Data/Talk`, `troubles.json` and
    `market.json` with `market::forbiddenWord`, and no "hound" as a word (horehound is a herb). Three older scene ids carry
    the animal's name (`ev_beast_dog`, `st2_cat_dog`, `m7_wx_wet_dog`); ids are never read by players and players' heard
    lists keep them, so they stay.
  - **Tests:** `talk_tests` 65,757 checks (new: `salience`; `theStagedEconomysTalk`: no figure in any price, Times, event,
    ship or harvest line, nothing after a price, a travellers' move never put down to the roads, the sea or the season
    over 240 conversations each way, a late wagon's move put down to it, every new topic spoken with every symbol filled,
    a failing keeper never "can't complain"; `theForbiddenWord`); `market_tests` 2,022 (new `talkSaysTheShownPrice`: a
    loaf moves P but not the dozen and nothing is said, ten loaves move the dozen and talk names the list's price for
    each traveller's own ladder, the cause and the traveller who pushed it, a wagon's fall, "still"; `theKeepersAnswer`:
    the moved ware with its cause, `world.nails_price=up`, no price fact the library doesn't list); `trade_show_tests` 919
    (the notices' and news's goods now in cargo words); `voice_tests` 75. A clean Release build (`build-p8`); ctest 88/88
    with the database, none skipped (`pg_client` on a throwaway `ratw_test_s2stage_p8`, dropped after); all 34 `tools/test_*.py` with the database and this build's server (keys tagged `s2stagep8`, no `s2stage` database left): 668 tests, none failing, 25 skipped (the 23 `RATW_SLOW` world builds, `test_game_tables`' save under `Saved/`, `test_live_map`'s built client); `talk_report.py` run (the counts above); the
    client's 146 and its typecheck; the Editor's 81 + 67 and its typecheck (`node_modules` linked from the main checkout,
    removed after).
  - **Not built, and why:** `market.json`'s `houseBusinesses` and `estates` are not read (the `house` fact and the
    bands' guarantees cover a House's people): Phase 9 may drop them. A glut as a failing shop's cause in words (talk has
    no fact for which test made a shop fail or thrive). Season as a cause (it isn't P; the span is ×1.06).
  - **Left, by owner:** Phase 7's repair: a lost ship's `world.times_cause` reads "the wagons late on the road" at the
    port (the checker's finding; not changed here); the Means' "Times here: lean, since <cause>" still splices an event's
    words after "since". Phase 10: the lines by ear (the 894, and whether the cause and price rules take too much of their
    tier: `e8p_sc_upper_trade_up` takes most of its niche), the 34 goods with price facts.
  - **For the user to decide:** (1) the shown price said "fourteen pence the dozen" rather than "twelve for fourteen
    pence"; (2) talk names the price of the nearest shop that sets a good out (the speaker's own, then its cell's), so in
    Ridgemere a Sump speaker may name a Heights shop's district price where the Sump has none; (3) the event words that
    read badly mid-sentence in `market.json` (Phase 7's data, left as it is): "the drovers in with their herds", "a
    brewing too many", "the buying ahead for the festival", "a lame ox on the cart from East"; the new lines frame them
    after a colon or "talk of", never after "since".
- **2026-11-11: Phase 7 repaired** (branch `r2-staged-rev`, cut from `r2-staged` at `aba9f4b`, beside the Phase 8 talk
  repair in `r2-staged-rtalk`). The Phase 7 check's ten findings and its missing items, and the Phase 8 builder's notes on
  the event words and the season, each fixed with a test where it is code (`events_tests`' `phase7Repaired` and
  `theRepairOnTheBoards`, more checks in `shipsInTheEvents`, `port_tests`' `eventsPostWithTheirCause`; the festival
  crier's check seen failing without its fix). No economy simulation, no real or scratch server; DEV and PROD untouched.
  - **(1) A ship's rich cargo** is of the kind's own goods first (silk, spices, brandy, wine where its market deals in
    them), then the ship's; its notice reads "The Gull is in with a rich cargo: ..." once she is (a kind's new
    `noticeOn`, filled as it is rolled and saved on the event while it is due; the board's pinned notice changes with it).
    The goods are in cargo words (Phase 8's `cargoWords`: "crates", "hides").
  - **(2) A lost ship never sails again soon:** her name goes on the Board's new `lostShips` (name and day, a year,
    64 at most, written only when there are any, read leniently) and the timetable names other ships first; with every
    name lost within the year, the one lost longest ago. How often ships are lost (three in 95 days at Ridgemere) is
    Phase 10's weight.
  - **(3) A harvest override's notice** says what the area's farms grow ("A good season for flax, flour and hemp.", the
    DM's words before it), never the area's id (on DEV, a good's name: "the fish"), and comes down when the override
    lapses (its notice expires then, and a start takes down one whose override is gone). A nudge's notice expires with it too.
  - **(4) Left, the user's decision:** an event's Times points move the whole Times band (the staples' factor, thefts,
    budgets, the band cut). Noted open in 5.3.
  - **(5) The ships' troubles in their own words:** a stormbound ship is a trouble "storm" and a lost one "lost" (each
    counted in the Times as a late wagon is, as before), said as "the storms keeping the ships out" and "the ship lost at
    sea", never "the wagons late on the road". A save's troubles of either kind are kept.
  - **(6) `world.times_cause` only where the times are not ordinary;** the event words rewritten as noun phrases (8.4's
    note): "the broken mill wheel", "the drovers and their herds", "the flooded salt pans", "the brewers' glut", "the
    festival trade", "the lame ox that held up the {from} cart", so the Means' "Times here: lean, since the brewers' glut."
    reads.
  - **(7) The wedding's cry** is "Hear ye! A House wedding today, and the stewards buying wine, mead and brandy!" (its
    own goods, the day it begins); the season (`seasons` on kinds and causes, market.json); no kind twice at a market
    within seven days (across a week's end too). A wedding three weeks running at the city is Phase 10's weight. One line
    of talk (the talk session's file, `economy8_times.talk`'s `e8t_ev_salt`): "What's a winter without salt?" became
    "Every cook in town is after salt, and the pans give none.", so salt's line never speaks of winter in summer.
  - **(8) Phase 9's hooks:** `addStagedEvent` holds a kind's needs (the realm's largest market for a House wedding, the
    day before a festival for its buying, a port) and seasons, as the scheduler does (`makeEvent`); a restore holds a
    nudge's and a harvest override's `until` to a year from the calendar (a damaged save's 10^12 lapses); a comment on the
    hooks says Phase 9's commands must journal the Board after them. The commands are not built.
  - **(9) Robustness:** an event posted ahead whose days pass in a calendar jump is over with its record, so its notice
    comes down; an id the game never gives ("e05", one of 17 digits) is dropped at decode, so the next id is above every
    one kept; at a market's bound the oldest over or cancelled make room, and a week the bound cuts short is not marked
    rolled (rolled again the next morning); the rule `eventRate` at 0 calls off the seeded events still due (the DM's
    stay); a staged cry holding the crier gives way to the festival's crier and a legend's, its lines waiting again. A
    cry not cried when the server stops is still not cried after it (voices are not saved; as documented).
  - **(10) Fit:** the staged cry's crier is `crierOf`'s (a second form of it for a market plain for law, so "preach"
    counts as a priest's word there too); one `market::fillWords` for the events' and the causes' blanks; the seeded cause
    by `seeded::lot`; a saved event holds four goods at most, as one is made.
  - **Missing items:** 16.5's "staged events post with their cause" in `port_tests` (with events on, at the port);
    16.9's forbidden-word check of the shipped events, causes and ship names stays in `events_tests` and `port_tests`
    (the lists' own tests) rather than `talk_tests`/`voice_tests`; `Society::setProsperity` from production is Phase 9's.
  - **Journal:** `lostShips` and an event's `noticeOn` are on the Market Board, written whole with it as every Board
    change is; nothing new waits on a tick notice.
  - **Tests:** a clean Release build (`build-rev`, `--clean-first`; no new warning in the lines touched), then ctest once
    with the database: 88/88, none skipped (`pg_client` on a throwaway `ratw_test_s2stage_rev`, dropped after; no
    `s2stage` database left). While building: `events_tests`' new `phase7Repaired` and `theRepairOnTheBoards`,
    `shipsInTheEvents` 877 checks, `port_tests`' `eventsPostWithTheirCause` 554 and `aStormboundShip` 849. No perf
    timings and no `tools/` or client tests (the merge's verify runs them).
- **2026-11-11: Phase 8 repaired** (branch `r2-staged-rtalk`, cut from `r2-staged` at `aba9f4b`; the Phase 8 checker's
  findings and the builder's open items). No economy simulation, no paid model call, no server; DEV and PROD untouched.
  The as-built note is in section 11 ("Phase 8 repaired").
  - **Fixed (HIGH):** (1) talk quotes the listening player's own list (`talkFor`, `AmbientPick::hearer`; before, the
    holder was never set); (2) a wagon's news is no longer overwritten by the topic's first fact; (3) a ship's price lines
    and ship topics name their own ship (`topic.ship`); (4) a piece bought never takes over a wagon's, ship's or event's
    cause; (5) food lines only of food (`topic.sort`; 12 older entries gated or reworded, and
    `tr_bake_flour_dear`'s "another penny a sack" out of price talk); (6) keeper lines only from a keeper who sells the
    good (`deals`; 5 older entries gated).
  - **Fixed (MEDIUM, LOW, missing):** the topic scores (above); the cause rules lose their priority and the harvest's,
    news's and ships' openers share their niches; event lines claim no price the list doesn't show, and say the past
    when over (11 kinds and the generic); a failing keeper never boasts ("can't grumble" and two more in salience, the
    keeper's boast gated on business); `talkPrices` once a look; plurals by `topic.is`; the catalogue's stiff words;
    pups and the little ones only from a parent, "Mother says"/"Father says" only with one; the destitute short
    trouble's "no earner"; the salt line's winter by season; the soonest posted wagon; the Means at the speaker's own
    shop and the player's list, cut at a sentence; the helped greeting only while lifted; `world.wagons`; the Mind's
    rules sentence says the Means carries a price and its cause.
  - **Rejected, and why:** the keeper's trade answer keeps its figures ("14 pennies"): it reads off the list a player
    is buying from, which shows figures. `e8p_sc_upper_trade_up` still takes most of its narrow niche (a rich upper
    wolf to a rich trader about a rise): left for Phase 10's ear.
  - **Lines:** 648 spoken lines in 298 entries (was 894: prices 189, roads 144, street 116, times 199), within the
    plan's 450-650.
  - **Tests (through the game's path for the HIGH items):** `market_tests`' `talkThroughTheRealPath` (the director's
    picks for each listening player and the topic as the game writes it, `talkTopicQuery` with its hearer, then
    `converse`: Ash's and Bo's lists differ and each hears their own, never the other's; a wagon's news as brought; one
    bag of nails never takes the late wagon's cause, one traveller's cap doesn't, two do; nails never eaten and no shelf
    of them but the smith's; five days of hours share the talk) and the Heron in `talkSaysTheShownPrice`; `talk_tests`
    (the ship's name, nails neither food nor a non-keeper's shelf, every event kind's lines claiming no price without its
    fact and past when over). A clean Release build (`build-rtalk`); ctest 88/88 with the database, the six `*_pg`
    tests run again with `Database/.env` after they skipped (7/7; `pg_client` on a throwaway `ratw_test_s2stage_rtalk`,
    dropped after; no `s2stage` database left). Not run (the merge verify's): `tools/test_*.py`, the client's.
- **2026-11-11: Phases 7 and 8 repaired and merged; the new baseline** (on `r2-staged`: `18b1b70` merges
  `r2-staged-rev` at `262dbaf`, `ae7739b` merges `r2-staged-rtalk` at `24948ce`, `4aa5f20` the clashes between them). No
  economy simulation, no paid model call, no real or scratch server; DEV and PROD untouched (DEV only exported, read only).
  - **Conflicts (two):** `economy8_times.talk`'s `e8t_ev_salt`: the events repair had replaced "What's a winter without
    salt? Ask me in a month." with "Every cook in town is after salt, and the pans give none."; the talk repair had moved
    the winter line to its own rule gated on the season (`e8t_ev_salt_winter`). The talk repair's season-gated rule kept,
    and the events repair's line added to the ungated `e8t_ev_salt` as its alternative the rest of the year. This
    Progress section: both repairs' entries kept, in the order merged (Phase 7 repaired, Phase 8 repaired). Everything
    else (`RatwMarket.cpp`, `RatwGame.h`, `RatwWorld.h`) merged on its own.
  - **The two together, through the real path** (`events_tests`' new `theRepairsTogether`; `RATW_TALK_SAMPLES=1` prints
    every line said): a game on the harbour strip with the talk library, each event kind east can show on (eight; the
    festival's buying and the lame ox need a festival and a farm's cart, and are in their own tests), posted ahead (the
    kinds posted ahead) and over; the Times past ordinary with the event as their cause; its goods pushed against it; a
    rich cargo's ship in, a lost ship, a storm; sampled through the director's picks for a listening player
    (`ambientPicks`, `AmbientPick::hearer`), `Game::talkTopicQuery` (the pick as `exchangeQuery` writes it) and
    `converse`, and the Means. The new event words read right after "talk of", "word of", "blames", "since" and alone
    ("Everyone blames the broken mill wheel.", "The brewers' glut, and suddenly everyone's got coin.", "Times here: lean,
    since the flooded salt pans."); the lost and storm troubles' words reach talk and the Means ("It's all the talk of
    the storms keeping the ships out, they say."; "since the ship lost at sea."), never a wagon's. **Three clashes,
    fixed in `4aa5f20`:**
    (1) a price running against its event said "though" before a noun phrase ("cheap still, though the House wedding"
    on the list; "Planks cheap still, though the fire at the works." in talk and the Means): an event's still is now
    "despite <its words>" in both (`eventCause`, `talkCause`); a wagon's or a ship's stays "though" (a clause).
    (2) A rich cargo's goods are the kind's own where its market deals in them, then the ship's (Phase 7's repair), but
    the ship's talk named her everyday cargo ("The Wavecutter's in with chamomile.") and the event's lines said silk at
    a market that sells none ("I've never seen so much silk in one place."). The ship topic now takes the rich cargo's
    first good while it is on ("The Wavecutter's in with brandy."); `World::eventFacts` gives `world.event_goods` (the
    event's goods in cargo words, `facts.json`); the cargo lines say those goods ("Brandy, wine, chamomile and dangling
    charm, and all of it off the one ship.") and silk only in the new `e8t_ev_cargo_silk`, gated on
    `world.silk_price=down`; the rich and the past lines no longer name silk. The notice reads "is in" once she is
    (Phase 7's `noticeOn`) and the event's talk is "now" from the same moment.
    (3) `e8t_ev_lost`'s "They're saying it was the loss of the Morning Star." is "Everyone's talking of the loss of
    ...". The stale "a brewing too many" in `market_tests`' and `talk_tests`' fixtures is "the brewers' glut".
    Lines: 650 in 299 entries by the Phase 8 repair's count (the salt alternative and the silk line), the plan's
    450-650.
  - **Seen, not changed (Phase 10's ear and weights):** at the port a lost ship's topic (2.3) outscores the Times
    (1.45-2.05) for its four days, and an event on (1.7-2.1) outscores lean Times most hours, so lean Times are seldom
    talked of while either lasts; the generic "posted ahead" lines ("They're crying <words> in the square") serve only a
    host's own kinds now (every shipped kind posted ahead has its own); "dangling charm" in a list of cargo is the
    catalogue's singular (`cargoWords`).
  - **Tests (the one full run):** a fresh Release build (`build-m78`, from empty; 126 warnings, as before the merge,
    none in the lines touched); ctest 88/88 with the database, none skipped (`pg_client` on a throwaway
    `ratw_test_s2stage_m78`, dropped after; the `_pg` tests' own `ratw_test_s2stage_*`); `events_tests` 269,425 checks,
    `talk_tests` 128,447, `market_tests` 13,864, `port_tests` 9,665, `trade_show_tests` 919, `voice_tests` 75. All 34
    `tools/test_*.py` with the database and this build's server (`RATW_TEST_SERVER`, `RATW_SERVER`, keys tagged
    `s2stagem78`; each through `python3 -m unittest` in `tools/`): 668 tests, none failing, 24 skipped (the 23
    `RATW_SLOW` world builds, `test_game_tables`' save under `Saved/`); no `s2stage` or `pkg` database left;
    `artifacts/screenshots/dev-console-fight.png`, rewritten by `test_live_map`, restored. The client's 146 and its
    typecheck; the Editor's 81 + 67 and its typecheck (`node_modules` linked from the main checkout, removed after).
  - **The new baseline** (the user's decision, 2026-11-11: "find a new baseline. After all this work we're going to
    continue to fill the world out anyway"). **It replaces Phase 0's table from now on: the gate for Phases 9-11.**
    DEV build 70 exported read only, `world_check EXPORT --simulate A B --players 20`, Release, 8 threads, the merged
    HEAD against `5afe23c` (the code before Phase 7, built in its own `git worktree`, removed after; each run from its
    own tree, so with its own `Data`), alternating, two runs each; the machine quiet (load 0.25 before the first run,
    0.9-1.5 during them from the runs themselves; no other heavy work). ms, the mean of the two runs:

    | Window | mean | p99 | p99 after the first minute | worst after the first minute | worst | residents |
    |---|---|---|---|---|---|---|
    | 6-8 | 2.60 | 5.77 | 5.44 | 12.2 | 38.5 (tick 19) | 765 |
    | 17-19 | 2.14 | 5.21 | 4.99 | 21.9 | 39.3 (tick 19) | 765 |
    | 23-25 | 1.62 | 4.52 | 4.04 | 12.8 | 36.8 (tick 19) | 765 |
    | 6-8 `--ship-day` | 2.65 | 5.86 | 5.49 | 12.7 | 37.6 (tick 19) | 765 |

    Beside `5afe23c` (ranges over the two runs of each):

    | Window | build | mean | p99 | p99 after the first minute | worst after the first minute | worst |
    |---|---|---|---|---|---|---|
    | 6-8 | merged (the baseline) | 2.59-2.60 | 5.75-5.78 | 5.42-5.45 | 12.1-12.3 | 38.3-38.7 |
    | 6-8 | `5afe23c` | 2.60-2.61 | 5.77-5.82 | 5.54-5.55 | 11.3-11.9 | 36.6-36.7 |
    | 17-19 | merged | 2.13-2.16 | 5.15-5.28 | 4.94-5.04 | 21.8-22.0 | 39.0-39.5 |
    | 17-19 | `5afe23c` | 2.10-2.13 | 5.12-5.18 | 4.88-4.95 | 21.9-22.1 | 37.5-39.5 |
    | 23-25 | merged | 1.62-1.63 | 4.44-4.60 | 4.04-4.05 | 12.8-12.9 | 36.7-36.9 |
    | 23-25 | `5afe23c` | 1.68-1.71 | 4.79-4.79 | 4.29-4.30 | 12.7-12.7 | 37.9-38.3 |
    | 6-8 `--ship-day` | merged | 2.64-2.66 | 5.80-5.92 | 5.45-5.52 | 12.5-12.9 | 36.7-38.5 |
    | 6-8 `--ship-day` | `5afe23c` | 2.59-2.61 | 5.74-5.80 | 5.41-5.45 | 12.1-14.3 | 37.8-38.7 |

    **What the merged code costs over `5afe23c`:** with a ship in (`--ship-day`) about 0.05 ms of mean (2%), 0.09 of
    p99 and 0.06 of p99 after the first minute, both runs above both of the base's: the port's events, ships and their
    talk (Phases 7 and 8). 17-19 about 0.02-0.03 ms of mean and 0.06-0.08 of the p99s, at the edge of the runs' spread;
    6-8's worst after the first minute 0.6 ms more (12.2 against 11.6), its p99s the same or lower. 23-25 is faster on
    the merged code (0.07 ms of mean, 0.27 of p99, 0.26 after the first minute; not looked into). The worst tick of every
    run is tick 19 in the first minute (a fresh world's trade points and bands, as the Phase 5 repair's entry says).
    Against section 15's bar (p99 at most 5.3 ms, worst at most 25 ms): the worst after the first minute is under 25 ms
    in every window, as are 17-19's and 23-25's p99s; 6-8's p99 (5.77, 5.86 with a ship) and the first minute's tick 19
    are over it, as on `5afe23c`. From now on the gate is: no window slower than this table beyond noise (a run's spread,
    about 0.05 ms of mean and 0.1 of p99 here). `world_check` has no Game and no journal: those stay outside the gate.
- **2026-11-11: Phase 9, dm lane** (branch `r2-p9-dm`, on K0 `0752a58`; the lane note, for the merge step's entry). **The
  Prosperity tab** replaces the Money tab: `dungeon_master.prosperity()` and `GET /api/prosperity` (viewer) read the
  checkpoint's `society.report` (Core's, passed through opaque), the towns' `stores:` treasuries, the week's wagons (the
  Board's committed departures, the checkpoint's roads, 7 days of arrivals, losses and cancellations), ships (the
  timetable and `lostShips`) and events, player coin made against sunk (`staged.faucets`, 7 days, the top 20 holders),
  today's thefts, the word on the street (at most 20) and a 28-day log (`dm.audit` levers and acts, `game.events`
  causes and calls, Board ripples over 0.2), with `writtenAt` and `stale` (over 3 minutes). `money()`, `/api/money`
  and `MoneyTab.tsx` are gone. **The acts:** `POST /api/prosperity/act` and `POST /api/players/grant` (dm) queue
  `times.nudge`, `harvest.override`, `event.add`, `event.cancel`, `ship.storm`, `prices.reset`, `band.override` and
  `character.grant` (in `ACTIONS` and `LIVE_KINDS`; no `economy.` kind), each clamped by `LIVE_ACTS` as Core clamps
  (refused only for an unknown target or a malformed payload) and audited with the checkpoint's state before and the act
  after. **The Storykeeper:** the Director's `player_grant` (calls `grantPlayer`) and `npc_band` (`overrideBand` of
  the NPC's household, from `budgetDay`); `economy_transfer` refuses a staged end; the snapshot names each account's and
  character's `kind` and each resident's `band`; `dm_service` takes ids with `:@.` up to 160, any catalogue good, the
  two new shapes, a band-based migration need, and no minted or sunk; `DM/` composes both; `dm_smoke` grants 23 and
  fines 20 (Ash ends at 23 as before). **The Editor's DM side:** `ProsperityTab.tsx` (bands, the hungry and short,
  thieves, beggars, the dole, Times and causes, the price board, harvests, the week, players' coin, the word, the log,
  treasuries; buttons for every act); `EconomyTab.tsx` over the editor lane's `SteerPanel`, `RulesPanel`,
  `SeasonsPanel`, `ImportsPanel`, `StatusPanel` and `MarketPanel` (slim-only three behind `data.slim`); `api.ts`'s
  `economy.{get,steer,needs,rules,services,seasons,imports,status,market}`, `prosperity`, `liveAct` and `grant` (the
  plan's `dmApi.act` name is the character actions' already: `liveAct` instead); NpcTab's Band picker ("From rank" and
  the four), "A paid post (for succession)" and "Band for a while" (the household); ServerTab's live.economy line.
  **Tests (this lane's one run, on K0):** a fresh Release build (`build-p9dm`, 127 warnings, none in the files
  touched); ctest 87/88 with the database (`pg_client` on a throwaway `ratw_test_s2stage_p9dm`, dropped): the one
  failure is `server_parts_behaviors` at "a grant of 23 is a faucet: not built", K0's `grantPlayer` stub; with a
  stand-in `grantPlayer` (not committed) all 249 of its checks passed. `test_dungeon_master` 49, `test_dm_service` 39,
  `test_dm_live` 2 (the new slider-and-act test skips until the core lane's `world_store` is in), `DM/model.test.mjs`
  16, the Editor's 81 + 66 of 67 (`economy.test.ts`'s rule list fails on K0's two new rules: the editor lane's) and a
  typecheck clean in `src/dm` against the contract's panel props (the six panels are the editor lane's).

- **2026-11-11: Phase 9, core lane** (branch `r2-p9-core`, cut from `r2-staged` at `5fa92a4`; one of three lanes,
  merged by the integration step; the plan's sections are the judged Phase 9 plan's).
  - **K0, the contract** (`0752a58`, `4781f4c`): migration 0061 (schema only: `live.npcs.band_override`;
    `live.people_manifest` redefined with the slim economy lines, an `overrides` line and `band` lines;
    `live.business_settings` dropped, nothing else naming it); `tools/manifest_lines.py` (the exporter's economy and
    band lines, the same as the SQL's); `tools/economy_contract.py` (constants; `check_overrides` left to the editor
    lane); `Editor/src/lib/economyContract.ts`; the rules `prosperity` (-100..100, 0) and `harvestBias` (-20..20, 0)
    in Core and their mirrors; `caps.grantsADay` 500; `RATW_TEST_DB_TAG` in the hard-coded test databases' names.
  - **Core** (`f18a443`): Authoring lenient for every economy record (skipped with a warning, `AuthoredRoster::warnings`,
    64 kept and a count; economy.sync's outcome ends "(N records skipped)"; the game logs 20 at load): the economy
    line of 3 or 5 numbers, seasons with or without supply and normal (0060's empty prints too), rules clamped
    (`prosperity` to its step of 5), `rule "prosperityCause"` quoted or bare, the `overrides` line (65536 bytes; its
    cause wins), `band` records; an over-long economy line skipped; `withoutPeople` strips overrides, band, protected
    and wander. `Society::adoptOverrides` (`status::hostOverride`: a host's keys only, a share table whole only when
    each row sums to 100, monotone, every share 2% or more; caps held to 5x, grants to 2500; `market::layered`:
    events per week 0-3, the list by shipped id or a host's own object, causes, ships' names, never-imported known
    goods) at every start and economy.sync; the events', causes', ships' and never-imported readers use
    `society.market()`. The slider is the rule `prosperity` with the host's cause (absent: 0). `Game::applyEconomyCommand`
    (RatwGameEconomy.cpp) with the nine kinds, clamped not refused, each journalled (`record`) before anyone is told,
    the report worked out again; several economy.sync rows in one poll applied once. `World::clearHarvest` (its
    notices down at once), `Society::resetPrices` (the Board's `resets`, saved, journalled, eased each roads pass;
    a "prices reset" notice), `grantPlayer` (the door's group `grant`), authored bands (the DM's timed band, then the
    authored, then the guarantees; the highest in a household; npc.sync applies a changed one at once), the harvest
    bias, `World::prosperityReport` (in the checkpoint, opaque; at start, each morning, after each act).
  - **Tools** (`c680d1e`): world_store on the kept columns for every world (0061 slim, 0058-0060 pre-slim, older their
    subset), the overrides split from the rules and merged back, `save_economy`, `patch_economy_rules` (one statement),
    the band; live_edit's `economy` key through `save_economy`, `person:` with its band, economy.sync queued
    (`queue_sync`); world_build without live.economy in its fingerprint; world_sync tolerating a target without
    `band_override`, `economy_kept` on the kept columns; world_package shipping the kept columns with the overrides,
    their words scanned, `society.report` never packaged; market.json without `houseBusinesses` and `estates`.
  - **Deviations:** the DM acts' tests are in `events_tests` (`theDMsLevers`, and eight crash cases in its crash list,
    so `events_pg` runs them through Postgres) rather than a new `dm_economy_tests`/`dm_economy_pg`; a minimal
    map_editor change (overrides and band allowed, daily herbs and meals optional, 2-part seasons) for the entity.
  - **Tests:** a fresh Release build (`build-p9core`; no warning in the lines touched); ctest 89/89 with the database,
    none skipped (`pg_client` on a throwaway `ratw_test_s2stage_p9core`, dropped); `events_tests` 270,170 checks,
    `host_edit_tests` 657, `status_tests` 21,975, `wire_tests` 428; the tools' tests with the database and this build's
    server (keys and databases tagged s2stage): test_world_store 21, test_live_edit 19, test_world_build 2,
    test_world_package 46, test_world_sync 52 (none skipped), test_migration_0061 4, test_manifest_lines 7,
    test_map_editor 51, test_world_db 49; the Editor's 81 + 67 and its typecheck. No s2stage or ratw_test_ database
    left. Left for the other lanes (their own rewrites): test_studio's economy flow (11) and test_dungeon_master's
    store-room parity (1) fail on the slim economy until they merge.

## Review (2026-11-08)

Four reviews of the first draft, all read-only against `ridgemere2` at c8ac05c: one checking the code claims, one on
exploits, robustness and migration, one on believability (a week as a player in Ridgemere and Coldwater), and one on
scope, hours and the first slice. No code was run and no database was queried.

### Changed: money loops and faucets

- **Rounding.** Sold lots now round down, and the backstop is applied to rounded, floored lots. The draft's "the
  rounding moves a lot by under a penny" was wrong: selling one 1p good paid 1p at any factor over 0.91, and 13 small
  lots made 1p a trip across markets. `exploit_tests` sweeps lots of 1-5 pieces of every good from 1p to 20p at both
  clamp ends, plus q = 1 for 1p and 2p goods.
- **Sub-penny sales.** `transfer` refuses a price under 1p a piece and `trade` splits the total by quantity, so the
  draft's "99 ore pays 54p" could never execute. Sales now move goods at price 0 and coins with one `shift`. A 0p lot
  is refused in words and moves no pressure. The client's `price > 0` test changes, and each ware carries its
  smallest sell lot.
- **Faction discounts** (`RatwGameFactions.cpp:467-492`) reopened the hauling loop: a sworn Chapter's 25% back
  lowers the bound to about 1.23, well inside the clamp. The discount is now capped per Chapter per week and logged
  as a faucet (D28). The draft's "faction terms don't change" is gone.
- **The ripple ratchet.** Ripples now come from the day's net position, so churn moves nothing; they land the next
  morning; upstream ripples are value-weighted; ripple-caused |P| is capped at 0.15; each counts against the
  holder's cap. The fan-out is capped at 6 goods a step (uncapped, ore reached 35 goods and charcoal 116).
- **Every per-player cap is per account holder,** so a second wolf shares them.
- **New faucet caps:** pickpocketing staged NPCs (20p a day, take by band), staked games against NPCs (30p a day),
  raided cargo (about 40p a camp), bounty value and camp strength, the welcome (once per account per world per week,
  D23).
- **"Always pays" is made true:** staged payers are topped up on demand, and tills are topped up to the lot within the
  budget. The `min(cash)` sites are listed for change.
- **`classify` covers every prefix** (all 22 of `facilityAccount`, plus player ids and `treasury`), by prefix, never
  by function name. `bandits:` is escrow; `chapter:` is real (the second `treasuryOf` would have been wiped by a
  nightly set written by name); `stores:` stays as the staged town treasury (D24). Escrow carries its source and the
  faucet is counted at payout, which also settles festival pots.
- **Goods in staged wallets are trimmed,** and food is topped up to a target, not added daily. Goods leaving the
  staged side to a player count in the tally.
- **Times reads no player pressure,** so players can't push budgets up or manufacture hard times.
- **`conserved()` asserts become `realBalanced()`,** not stripped (260 calls in 37 files), so tests still catch
  duplication bugs and unregistered faucets.

### Changed: believability

- **T1 can be seen.** Bread is 1p and the list showed one piece, so its listed price never moved. Goods under 3p are
  listed by the dozen (D4), talk claims a move only when the shown price moved by 1p, and the client gets ▲/▼
  markers with causes.
- **Depth** is measured on a catchment with a floor of 20 and an exponent of 0.6, reworked against real community
  sizes (Coldwater 14, Saltwell 4; the draft's "40 households" doesn't exist), with worked Ridgemere and Coldwater
  examples. The per-holder cap is 0.15 and decays like P, with no reset at dawn.
- **Staged events** (section 8.4) and **a cause for every slider move** (section 8.5) give a small world motion and
  every change a reason. New T13-T17 and the standing-still rule.
- **Caravans:** the pull's sign was backwards and its source half landed unseen. Departures now take the source's
  glut at loading time; arrivals only lower cargo prices; player-sold surplus leads the next manifest; departures and
  ships are committed and saved when posted; drivers are real carters, away from their posts, never killed off-screen
  (D26); manifests match the destination's shops; notices use the calendar's weekday names; arriving wagons carry
  news; routes are the 3 nearest and frequency scales with both ends' size.
- **Regional restock** (D27): imported goods refill only on arrivals, so late wagons show.
- **Carts and ships pull toward a baseline** instead of stacking fixed steps that pinned prices at the clamp. Farm
  carts come every morning, heavier on Marketday; Marketday stalls are stocked from them.
- **Bands:** a home term; bounded fortune noise (the draft's weekly walk had no bound); destitute needs a reason; the
  minimum is realm-wide; a band stays within one of its home and class; guarantees have a precedence; helped and
  guaranteed households sit outside the ranked pool; bands are saved with their `since`; help gives a lifted state at
  once, so a helped beggar stops begging the next day; money players pay NPCs reaches their bands.
- **Hunger follows the shelf:** food is a staged sale from the nearest food keeper, and an emptied shelf gives the
  poor a short day. Buy-ahead before feasts is given a mechanism.
- **Staged sales obey the standing-still rule,** and errand-runners are capped per player-occupied cell.
- **Business** reads sales against the shelf, adds a score event after three failing days, and suppresses "can't
  complain". Talk attributes causes from what actually moved the clamped factor. The Mind's Means carries the shown
  price and the pay word; rich talk tags are gated by class.
- **Theft** is an expected count, so hamlets don't have a daily crime wave, and takes follow the victim's band.
  Bandit loot from staged caravans becomes recoverable goods.
- **District factor** inside Ridgemere; reset prices eases over a day; every good maps to a depth group.

### Changed: wrong code facts

- Farm staffing depends on the farm code the draft cut (`staffFarms`, `farms_`, `farmOfPost`). The farm list,
  `farmWorkplace_`, `countFarmPosts` and the area switch stay; only the account checks go. `country_farms.json` keeps
  its farm defs, which `realm2_places.py` and `test_realm2.py` also read.
- `items::producerFor` stays; only the store room's call goes.
- Restore refuses over the roster count, changed roles and ledger days too; all are reconciles now. The departed and
  newcomer paths are rewritten without `minted`. The unknown-resident line is :1030.
- `budgetDay` is the society's day clock and stays required; the required set is corrected.
- Town treasuries are `stores:` accounts; the draft both deleted and kept them.
- `city_loop.json`'s other sections (`shelves`, `wages`, `luxuries`, `food`, `houseBusinesses`, `patrons`,
  `estates`, `markets`) each have a fate. `houses.json` and `specialties.json` don't exist; doc 61's section points
  at real sources.
- `EconomySpec` keeps rules, services and seasons; authoring keeps `season` lines; 0061 keeps `treasury`,
  `store_herbs` and `store_meals`.
- `--fast`, `--unseen-step`, `--straight` and `setUnseenStep` stay: they are sim speed.
- NPC theft takes from the keeper's purse, not the till; a till-food path is added. The order and the per-NPC gate
  are stated correctly.
- Provision food no longer goes through `create`: it would have written a ledger entry, a journal record and a
  WorldEvent per household a day, marked promises kept, and been remembered by the Mind as gifts.
- `robbed` and `wealth_days` already exist. Errands come from "fetching goods". The homeless were in no household;
  they are households of one now. Unplaced NPCs are not residents. `means` goes into `OPTIONAL_LIMITS`.
  `KEEPER'S PURSE` is at `dialogs.ts:1320`. "BUYING TODAY" needs a new merchant field, so the headline no longer
  says the client's fields don't change. `test_world_sync.py:1340` tests seasons, rules and services. The new
  struct is `TradePoint`, since `Markets` exists. Fruit's season range (×1.75) is over the clamp, stated as a fact.
- Counts corrected: about 480 lines test `wealth`, about 108 test `business`, 16 mention `price_cause` (a plain grep
  of `Data/Talk`); about 165 `shift(` matches in 31 Core files; 260 `conserved()` calls in 37 files; about 8,200
  lines of tests.
- D5 corrected: the general store buys stone, clay and sand, not timber; the ironworks and foundry already buy bars.
- Forgotten dependents added: `RatwFarmPeople.cpp:527`, `world_build.py:42-43`, the three Python tests, the
  `CMakeLists.txt` registrations, `item_catalog.py`'s hours requirement, the troubles' food checks, the Editor files,
  `sync.ts`, `UpdateFromLive.tsx`, `ServerTab.tsx`, the Inspector hint, the `paid` toggle (kept, relabelled), and
  `applyExternalNpcStates`' `restore()`.
- `tendRoads` returning early also stops NPC contracts and courier letters today; the road layer's `towns_` uses move
  to markets while law stays.

### Changed: robustness and saves

- Hosts can't edit `Data/` (doc 78). Defaults stay there; host changes live in `live.economy.rules`.
- Budgets, holds, lean days, bands and committed departures are saved; load settles only what is missing or out of
  range, so a restart changes nothing visible.
- Lone merchants map to a market; routes are capped at 3 nearest; wagons in transit are capped.
- Dropped caravans' letters reopen or refund; orphaned contracts refund.
- Unknown items in players' hands go to a quarantine list.
- Purse targets are clamped to 500p.
- `save_compat_tests` gains a PROD-shaped fixture from `dev-old`, a pre-79 package, a replayed pre-79 journal tail
  and a doc 78 "Reset economy rules" copy.

### Changed: scope and hours

- **Phase 0, "loop off",** comes first and its failure list is the first check-in (D29). Phase 1 then replaces
  `shopFactor` outright, and Phases 1 and 2 can run in parallel.
- The cut and street life use 0.45; reviews count at face value; a boxed look-and-feel phase is added; staged events
  get their own phase; Phases 4-8 run in sequence by default (D30).
- Doc 78's build order is stated (D22).
- Notes and memory pointers are marked history in Phase 3, with the user's say-so.
- Totals: about 158-221 plan, 73-101 expected (the draft said 124-172 and 47-64, and its headline and table
  disagreed on slice 1).

### Rejected, and why

- **Rounding bought lots up** (exploits review). The floored sale and the backstop on actual lots already make every
  round trip lose at least a penny. Rounding up would turn a 1% push on a 1p loaf into a doubled price, which is the
  opposite of T1's look.
- **Value-weighting downstream ripples** (exploits review). Ore is 1p and bars 9p, so weighting would cut the bar
  ripple to a ninth and erase the user's own iron-ore example. Netting and the caps stop the ratchet; only upstream
  ripples are weighted.
- **Making buys and sells ripple to the same set** (exploits review). With ripples from the net position, churn nets
  to zero whatever the sets are, so the sets stay as the link table gives them.
- **Upstream ripples landing instantly** (believability review). All ripples land the next morning from the net
  position: that closes the ratchet and gives every ripple a visible cause.
- **Locking welcome goods against giving or selling for N days** (exploits review). It needs coin provenance on
  purses. Counting the welcome per account per week bounds the faucet more simply.
- **Refunding unknown items at their last catalogue price** (exploits review). It would be a faucet and needs a saved
  price. Quarantine loses nothing and makes nothing.
- **Paying the faction discount as standing** (scope review). It would change a feature players already know; a
  weekly cap bounds the same loop.
- **A separate shorter half-life on wagon routes** (believability review). The wagon's departure carrying the glut
  off does the same job, visibly.
- **Goods in `realBalanced()`** (scope review asked for coins and goods). Coins only in v1; goods would need prices in
  the balance and add noise. Goods are in the tally and the report.
- **Copying a fixture from PROD** (exploits review asked for a PROD-shaped save). `dev-old` has PROD's map and shape,
  and nothing reads or writes PROD.
