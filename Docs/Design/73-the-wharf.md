# 73. The Wharf (2026-11-04)

The user, 2026-11-04 (verbatim):

> The Country should turn into the Wharf, which is going to turn into where, for the game, we can inject and take money,
> excess items, and import shortages. So change the name from "Country" to "Wharf", and make it so that all the changes
> that the country makes and items it offers flows through the wharf. The wharf "sells" and "buys" items from NPC's at
> whatever is the local, stable price. Even far-away towns/communities can order from the wharf and things will be
> delivered via wagon like any other order.

And, in the same message: "Import: aqua vitae, offal." and "Leave cheese as it is."

This doc is the design only (no code, no run, nothing written to DEV). It builds on doc 72 (the Country pulled into the
world; Phase 6's estates, Grayrock's being the Country's own purse; iron6), doc 68 (the city's loop: the store room's
draws, the gate toll, carriage, the Country's basket) and doc 67 (the store room, R35; the wharf, R5, R6, R28). The same
message's other fixes (the ironworks' and foundry's profits to their estate, timber, linen, new producers for brandy and
apple brandy, eggs, sheep and sheepskins on the farms, copper and tin at the mines) are other lanes; this design names
where they meet it.

**How it was measured.** iron6 (3 years, build 62's export), year 3 a day unless marked:
`artifacts/economy/r2/iron6.full.jsonl.gz`, `iron6.jsonl`, `iron6-report.md`; the session's scratchpad `country_sales.py`
and `good_sales.py`. The wharf's rooms and posts from `artifacts/economy/r2/export62/world.ratw` (read only).

## Headline

- **The Country is renamed the Wharf**, everywhere it is the outside market: the purse `outside:country` becomes
  `outside:wharf` (House Grayrock's estate, as since Phase 6), the market `country` becomes `wharf`, its rules, report
  keys, journal reasons and UI words with it. "Country" stays where it means the countryside (country farms, country
  workshops, `country_farms.json`, `cn_` ids, the country folk of the unplaced pool). **A save carries over at load**: an
  `outside:country` account is moved into `outside:wharf` and closed, as `fund:store` was moved into the Country's in
  doc 68. No data migration, no schema migration for Phases 0-3.
- **The Wharf is a place**: the Grayrock Wharf Office on the Wharf Road (cell `w_ridgemere_0_1`), its warehouse the
  Grayrock New Store, the Crown's Customs House beside them. Its 58 posts are the ones it already pays. **The store room
  stays what it is** (not a place, doc 67 R35; "at the gate" is the gate toll): the depot through which the realm's own
  farms and works sell. The Country's branch of it, the goods made from nothing and paid to the Country, becomes **the
  Wharf's lot** in a draw.
- **The market valve.** The Wharf **imports** what the realm's makers can't supply (aqua vitae, offal and butter by
  default; the goods no area grows, as the Country did) and **takes excess** (hides, bones, tallow, honey... piling on
  the farms), both at **the stable price**: the store room's price at its season's factor, never a shop's drifted price.
  Per good: on or off, a daily cap; takes only over **28 days of the farm's yield** in its store. **Cheese is on neither
  list, and refused if added** (the user).
- **Money.** What the realm pays for imports leaves it (sunk, "paid beyond the realm"); what the beyond pays for the
  realm's goods comes in (minted, "from beyond the realm"), booked in `SocietyState.wharf` and in the existing
  `minted`/`sunk`, so `conserved()` (money == minted - sunk) is unchanged. **The trade balances by default**: the Wharf
  buys from the realm only as far as the realm has paid the beyond for imports (**its trade credit**). The DM moves the
  credit to inject or take money, logged, capped, zero by default. **The Wharf keeps a 10% fee** (wharfage), its
  takings toward the wharf's posts.
- **Orders by wagon**: every till that draws from the store room (city shops, the communities' stores, alehouses and
  workshops) gets the Wharf's goods the same way, carriage by weight to the carters; the farms' baskets may fall back to
  it; one entry point (`orderFromWharf`) for the towns to come.
- **Build order**: (0) the rename, behaviour identical; (1) the beyond ledger and imports; (2) takes, the export basket and
  the credit; (3) the DM's and Atlas's controls; (4, later) per-world per-good settings (migration 0061), wagons seen on
  the road, distance in carriage. Each smoke asked first; the 3-year run asked first.

## 1. What the Country is now (iron6, year 3)

| Part | A day | Notes |
|---|---|---|
| Its own sales through the store room | **8p** | planks 6.1 pieces (8.1p), linen 0.1 (0.3p); nothing else |
| Its purse, the wharf's (Grayrock's estate) | posts 476p of 58 posts; in: 21p fish land rents, 8p sales, ~423p of Crown grants | 55 nights short over the run, 52 in year 3 |
| The estates' basket, patrons, the Houses' tables | **0, 0.1, 0p** from year 2 | no estate has spare; the city's exporters (paper, steel, maps, handbells, beads...) lost their only buyer |
| Goods short with no realm maker | butter ~76 pieces a day short at the stalls and provisioners after the dairies' 35.5 a day | cheese short by design (the user's luxury) |
| Area goods no farm grows | aqua vitae, offal, wheat, eggs, sheep, sheepskin, brandy, apple brandy | no draw of any refused in year 3 (`farms_out_goods`): their makers buy round them; the Wharf's caps start small |
| Piling (stock growth a year) | hide 8,342, bones 9,669, flax 5,916, charcoal 3,933 (hearths 5,588 held), iron ore ~2,400, limestone ~550, tallow ~170 a dairy, honey ~500 a hive | flax, charcoal, ore and limestone are yields sized on year 2 (the patch trims them) |

So the Country's import role is nearly gone, its export role (the basket) stopped for want of money, and the realm has
no way to land a shortage or ship a glut. That is the valve this doc adds.

## 2. The rename

### 2.1 Renamed (code, data, reports, UI)

| Now | Becomes | Where |
|---|---|---|
| `Society::CountryFund` = `"outside:country"` | `Society::WharfFund` = `"outside:wharf"` | `RatwSociety.h:653` and every use (38) |
| market id `country`, name "the Country" | `wharf`, "the Wharf" | `markets[0]` in the data file; the default in `RatwItems.cpp:725` |
| `Data/Items/country.json` | **`Data/Items/city_loop.json`** (open question 1) | the loader `RatwItems.cpp:580`; `tools/map_editor.py` `country_file()`; `Tests/season_tests.cpp`; tools/city_loop.py; comments (sed) |
| `items::Country`, `items::country()` | `items::CityLoop`, `items::cityLoop()` | 47 uses in Core and Tests |
| `EconomySpec::country`, `countryReserve`, `wageCountry`, `countryOff` | `wharf`, `wharfReserve`, `wageWharf`, `wharfOff` | `RatwSociety.h:344-349` and users |
| `countryBuys`, `countrySales`, `countrySpent`, `countryUnspent`, `patronsCountry` | `wharfBuys`, `wharfSales`, `basketSpent`, `basketUnspent`, `patronsEstates` | `RatwCityLoop.cpp`, `loopToday_` |
| `countryLeft(item)` (the farms' hold or the season's cap) | `supplyLeft(item)` (farms + the Wharf's lot, 4.3) | `RatwCityLoop.cpp:648` and 22 uses |
| `countryBasket`, `countryBasketShift` | `wharfBasket`, `wharfBasketShift` | `country_farms.json` key and its reader |
| wage rule payer `"country"`, `Pay::kind` `"country"` | payer `"estates"`, kind `"estate"` | the data file's `wages.rules`, `payOf` (`RatwCityLoop.cpp:112,145`) |
| `countryPostsOwed`, `countryKeep`, `countrySpare`, `countryPosts*` | `estatePostsOwed`, `estateKeep`, `estateSpare`, `estatePosts*` | these are every estate's posts, the Wharf's among them |
| rule `countryPostsDays` | **`estatePostsDays`** | `items::Rules`, the data file, `map_editor.RULE_DEFAULTS`, Editor rules |
| estate `fund: "country"` | `fund: "wharf"` | the data file's `estates` (Grayrock's) |
| journal reasons "to the Country", "carried to the Country", "fund:store is the Country's now", "restocked ... from the Country's stores" | "to the Wharf", "shipped from the Wharf", "outside:country is the Wharf's now", "... from the Wharf's stores" | `world_check.cpp:kindOf` reads both |
| world_check's economy keys `country`, `country_spent`, `country_unspent`, `country_sales`, `country_rents*`, `country_land_rents*`, `country_posts_*`, `patrons_country`, `country_basket_shift` | `wharf`, `basket_spent`, `basket_unspent`, `wharf_sales`, `estate_rents*`, `estate_land_rents*`, `estate_posts_*`, `patrons_estates`, `wharf_basket_shift` | `Tests/world_check.cpp`; payer group `country` becomes `wharf` (`outside:`) and `estate:<id>` as now |
| `econ_report`'s section "The Country: the Houses' estates and the sea trade" | "The Wharf and the Houses' estates" | `tools/econ_report.py:575-600`; reads the old keys too (old runs) |
| text: wage rule "Carriage by sea and the Country's warehouses", the `about`s, `RatwSociety.h` comments, DM Money labels (`MoneyTab.tsx:17,26`), `api.ts`, `economy.ts`, `EconomyTab.tsx`, `studio/CountryPanel.tsx` | "the Wharf's warehouses"...; `CountryPanel.tsx` becomes `WharfPanel.tsx` | the Editor; `tools/studio.py` help; `dungeon_master.py:979` comment |
| Tests named for the Country | `country_tests.cpp` becomes `wharf_tests.cpp` (`country_behaviors`: `wharf_behaviors`) | `Tests/CMakeLists` |

### 2.2 Kept (the countryside, and storage names)

- **The countryside**: `country_farms.json`, the country workshops (`country_soapworks`... business ids), `cn_` ids,
  "country folk" and the unplaced pool, `tools/country_industry.py`, the `country` look of the communities, arrivals'
  words "from the country" (the pool's people come from the realm's countryside).
- **Storage names, read as before** (renaming them needs a migration for nothing): live.economy's columns `country`,
  `country_reserve`, `wage_country`, `country_off` (mapped in `tools/world_store.py` `LOOP_COLUMNS` to the new keys);
  the world file's records (`loop` with its `country` word, `countryoff`), which `live.people_manifest` writes.
- Doc titles and old progress entries (history).

### 2.3 Carried over at load (no data migration)

- **Accounts**: in `Society::restore` (`RatwSociety.cpp`, beside the `fund:store` move at line 1095): if
  `outside:country` exists, `outside:wharf` is opened if missing, the old account's cash shifted ("outside:country is the
  Wharf's now") and its stock shifted good by good, then the old account closed. A world with the estates split keeps
  every `estate:<id>` as it is. Done before `settleEstates` ever runs, so the estates' first-night split (keyed on the
  purses being open) is not repeated.
- **Rules**: `buildSeasons` reads a world's `countryPostsDays` as `estatePostsDays` when the new name is absent;
  `world_store.py` and the Editor do the same, and write only the new name (the next save of Atlas's or the DM's rules
  drops the old key).
- **Entity keys** in the studio's JSON (`country`, `countryReserve`, `wageCountry`, `countryOff`): accepted as aliases
  on POST for one release, answered under the new names.
- **Data file keys**: the reader takes `fund: "country"`, payer `"country"` and `countryBasket` as aliases.
- **Journals and old runs**: history keeps `outside:country`; `world_check`, `econ_report`, `econ_detail`,
  `people_summary`, `econ_weeks` and the DM's Money tab map it to the Wharf when they read it.
- **Check**: Phase 0's 30-day smoke with `--deterministic` gives the same money in every account as the same run on
  `ridgemere2` before it, `outside:wharf` holding what `outside:country` held (a compare script in the branch).

## 3. Where it is

| Place | Id (as built, build 62) | House | Role in this design |
|---|---|---|---|
| **The Grayrock Wharf Office** | `rm_the_harbourmaster_s_office`, door on the Wharf Road (195.5, 94.5) of `w_ridgemere_0_1` | Grayrock | the Wharf's seat: its purse (`outside:wharf`), its clerks ("clerking at the Wharf Office"), the harbourmaster |
| **The Grayrock New Store** | `r2_the_grayrock_new_store` (187.5, 85.5) | Grayrock | **the Wharf's warehouse**: imports are landed and carted out from here, takes carted in and loaded; "tallying in the New Store" |
| The Grayrock Bonded Warehouse, the Long Store | `rm_the_greatly_bonded_warehouse`, `rm_the_fell_timber_store` (ids from doc 61's names) | Grayrock | its other stores' tallies (no role in the code) |
| The Customs House, the Customs Pier | `r2_the_customs_house` (192.5, 132.5) | the Crown | the toll on what lands: the gate toll's rate and account (the treasury), now also "customs" in the text; "collecting the Crown's customs" is a treasury post already |
| The piers, the cranes, Sump Stairs | `w_ridgemere_0_0`, `0_1` | Grayrock | its posts: "unloading ships at the piers", "working the cranes", "carrying from Sump Stairs" |
| The Iron Store | `rm_the_grayrock_iron_store` | Timberland | Timberland's (doc 67's renames), not the Wharf's |

- **No map change, no post added, no NPC made.** The 58 Grayrock posts (the sea trade's 52, Grayrock Hall's 4, the
  harbourmaster, the overseer) stay paid by the Wharf's purse, as Phase 6 has them.
- **The store room stays the farms' and works' depot** (the realm's own goods, farm till to shop till, toll and
  carriage, `drawFromStore`). It is not physical (doc 67 R35) and stays so. What changes is its Country branch (line
  1451 on: `create` in the till, cost less toll to `outside:country`): that branch **is the Wharf's lot** (4.3). The New
  Store holds no stock in this design (imports are landed to order, takes shipped the same day): a buffer stock held there
  is open question 6.
- **Arrivals** (about 4 a year, up to 30p each from the Wharf's purse) come off the ships at the Wharf: unchanged in
  money, renamed in text.

## 4. The market valve

### 4.1 The stable price

`Society::stablePrice(item, n)` = the store room's price at markup 100: `ceil(catalogPrice(item) × n × f)`, where `f`
is the area's year price (`farmPriceFactor`, an area with `price: year`) if it has one, else the season's factor
(`priceFactor`: the season group's `price`, blended), else 1. Never the shop's stock factor (`stockFactor`), never a
luxury's band. Imports to a till are charged as every store draw is (`storeCost`: the world's `storeMarkup` for making,
`resaleMarkup` for resale, at the same factor), so a shop pays the Wharf exactly what it pays a farm for the same good.

### 4.2 The lists (in the data file's new `wharf` section)

```json
"wharf": {
  "about": "...",
  "imports": {"aqua_vitae": {"most": 10}, "offal": {"most": 20}, "butter": {"most": 90},
              "wheat": {"most": 40, "on": false}, "eggs": {"most": 30, "on": false}, "sheep": {"most": 4, "on": false},
              "sheepskin": {"most": 10, "on": false}, "brandy": {"most": 2, "on": false}, "apple_brandy": {"most": 4, "on": false}},
  "takes": {"hide": {"most": 30}, "bones": {"most": 35}, "tallow": {"most": 6}, "honey": {"most": 6},
            "small_pelt": {"most": 2}, "straw": {"most": 2}, "berries": {"most": 2}, "resin": {"most": 1},
            "flax": {"most": 20, "on": false}, "iron_ore": {"most": 10, "on": false},
            "charcoal": {"most": 15, "on": false}, "limestone": {"most": 5, "on": false}},
  "never": ["cheese"]
}
```

- `most`: pieces a day, realm-wide, times the rules `wharfImportScale` / `wharfTakeScale` (percent). Sized on iron6's
  year 3 (butter ~76 short; hides ~23, bones ~26.5, tallow ~1.4, honey ~4 a day piling) with room; aqua vitae and offal
  had no refused draws, so small caps until a smoke shows their draws.
- **Disjoint**: a good on both lists, or on `never`, is refused at load (an error in the catalogue's report), so the
  Wharf never imports what it takes and never routes a good both ways; `never` beats everything, including the per-world
  switch.
- Off by default: the area goods the other lanes give producers (eggs, sheep, sheepskins on the farms; brandy and apple
  brandy), so their runs show their own balance first; and the piles the yields' patch trims (flax, ore, charcoal,
  limestone). The DM switches any on.
- **Every good no area names is the Wharf's import, as it was the Country's** (planks, linen, copper and tin ores and
  bars until the mines offer them...), under the season caps that applied (the season groups' `normal × supply`), and
  the per-world `wharfOff` list (the old `countryOff`, now "goods the Wharf does not land").

### 4.3 Imports (the Wharf's lot in a draw)

- `supplyLeft(item)`: for an area good, the farms' hold **plus** the Wharf's left today (on the imports list, on, not in
  `wharfOff`, `most × scale` less what it landed today); for any other good, the season's cap as `countryLeft` was (and
  0 if in `wharfOff`). `restockFromStore` uses it where it used `countryLeft`, so a shop asks for what can come.
- `drawFromStore`: for an area good, the farms' lots first as now, by their stock; **the Wharf is the last lot** for
  what they lack, its pieces created in the till ("landed at the wharf"). The draw's cost is shared over the lots by
  their pieces exactly as the farms' now (the same rounding), the toll and the carriage on the whole draw as now. A
  non-area good is all the Wharf's lot (today's branch). `farmsShort` and `farms_out_goods` still count a farm good the
  farms couldn't fill, **even when the Wharf filled it** (so a real shortage stays visible; 7).
- **Money of the Wharf's lot** (`wharfValve` on): its share of the cost less the toll goes to `outside:wharf`; then
  `(100 - wharfFee)%` of it is **paid beyond the realm** (sunk, ledger `outside:wharf -> beyond`, "paid beyond the
  realm: <good>"), the rest the Wharf's fee. `state.wharf.out += it`, `state.wharf.credit += it`. With `wharfValve` off,
  nothing is sunk: the Wharf keeps the whole of it, as the Country did (Phase 0's behaviour).
- Refunds (a failed create) reverse the sink as well (`minted` is not touched; `sunk` is lowered by what was sunk).

### 4.4 Takes (the Wharf buys the realm's excess)

Once a day at **16:00** (before the 18:00 wages, so the takings count for tonight's crews, rents and land rents):

1. For each good on `takes`, on: each farm or works till holding it (`farmSells`, as a draw finds them) keeps
   `max(wharfKeepLeast, wharfKeepDays × its yield a day of the good today)` (the blended season yield times the farm's
   and area's yield scale; a made good, `makes`, its make's `aDay`). What it holds over that is its excess.
2. Today's room for the good: `most × wharfTakeScale%`, less what was taken today; and the day's export room (4.6).
   The excess is taken from the holders by their excess (largest first, then by till), whole pieces.
3. Legs, for `n` pieces at `P = stablePrice(good, n)`, carriage `C = carriageOn(WharfFund, good, n)` (no treasury
   subsidy: the beyond pays it), toll `T = gateToll(<farm till>, P)` (the farms' goods pass the gate as any draw's;
   open question 3), fee `W = P × wharfFee%`:
   - minted `P + C + W` into `outside:wharf` ("from beyond the realm: <good>"); `state.wharf.in += P + C + W`;
     `state.wharf.credit -= P + C + W`;
   - `outside:wharf -> farm till` `P - T` ("bought for beyond the realm"), `-> treasury` `T` ("gate toll: to the
     wharf"), `-> carters` `C` ("carriage to the wharf"); the goods shifted farm till -> `outside:wharf` and consumed
     there ("shipped beyond the realm"). The Wharf keeps `W`.
   - The farm's day as a store sale: `sold`, `goods[good].sold`, `takings += P - T`, `rentTakings += P - T` (its land
     rent or works' rent tonight counts it), `loopToday_.goodsSold`.
4. Only farms and works (outside the walls). City makers and shops are left out (their shelves follow their sales and
   their prices fall on a glut: `stockPrices`); a rule `wharfTakesShops` is not built (open question 7).

### 4.5 The export basket

What credit is left after the takes buys the city's exports, so imports' money comes back to the makers who lost their
buyer: **the Wharf's basket** (the data file's `markets[0].basket`, blended toward `wharfBasket` as now), hourly from
08:00 to 17:00, `budget = exportRoom / hours left`, through the existing `basketBuys` with `fund = WharfFund`: mint the
hour's budget first, then buy (`keep` = the Wharf's cash before the mint, so it spends only the minted), at the
catalogue's price from the makers' open shops, goods consumed ("shipped beyond the realm"), carriage out by the beyond.
What the hour did not spend is sunk back at once ("unspent, back beyond the realm"), so nothing minted lingers in the
purse. The fee `W` on what was spent is minted to the Wharf. The estates keep buying **the same basket from their own
spare** as now (the Houses' spending, inside money); the two never share a budget.

### 4.6 The trade credit (the balance guard) and the DM's lever

- `state.wharf.credit` (pennies, saved): up by what the realm pays beyond (4.3), down by what the beyond pays in (4.4,
  4.5). The day's export room = `min(credit, wharfExportMost - exported today)`, never below 0. Credit is capped at
  `wharfCreditMost`; any over it is simply not credited (the realm has overpaid: deflation shows in the report, 7).
- **So by default the realm's money is level**: over time the beyond pays in what it was paid, plus the fees. The fee
  is the only standing injection: 10% of the flows, about 15-30p a day at iron6's sizes.
- **Inject or take through the Wharf** (default off/zero):
  - rules `wharfInjectADay` and `wharfTakeADay` (pennies a day, 0-1,000, 0): at the 18:00 settle the credit is moved by
    `inject - take` (it may go below 0 by a take: the beyond buys nothing until the realm's imports have paid it back);
  - **a DM action `wharf.credit`** {amount: -N..+N, note}, at most `wharfActionMost` (2,000) either way, refused when the
    rule is 0, queued and audited like the DM's other live actions (`tools/dungeon_master.py` `LIVE_KINDS`), applied by
    the game server (`Core/RatwGame.cpp`, where `deed.award` is) through `Society::moveWharfCredit(amount, who, note)`,
    journalled as a world event "wharf credit" (no coin moves at that moment: it's a limit, not money).
  - **Money comes in only as goods bought** (at most `wharfExportMost` a day: the injection's daily cap) and leaves only
    as goods landed, never as a grant to a purse (doc 46's rule: channels, never handouts). A take therefore drains at
    the pace of the realm's imports (~110p a day at the defaults, below).
- **Sizes at iron6's year 3** (estimates for the first smoke): butter's ~76 a day at the resale price (~2.25p) ~171p,
  the toll 51p, the Wharf's 120p, its fee 12p, **~108p a day credited**; hides, bones, tallow and honey at the stable
  price ~165p a day wanted, so the credit binds the takes at first and the export basket gets little until the DM
  injects or more imports come (aqua vitae, offal). The toll gains ~30% of the takes (~35-50p a day).

### 4.7 The fee, and who keeps it

**Recommended: 10% (`wharfFee`), kept in the Wharf's purse** (House Grayrock's estate, which runs the wharf and pays its
58 posts): on imports, 10% of its takings after the toll (the rest sunk); on exports, 10% over the price, paid by the
beyond (wharfage: the ships pay it, so the realm's sellers get exactly the stable price, the user's words). It is
income toward the wharf's posts (~476p a day), not a cure for its gap (~15-30p of it): that gap is the other lane's (the
ironworks' and foundry's profits to their estate) and the Crown's grants, unchanged. The fee shows in the estates' table
(7) as "the Wharf's fees".

### 4.8 What happens to the rest of the Country's doings

| The Country did | Now |
|---|---|
| supplied every good no area grows (made from nothing, paid in full to its purse) | the Wharf's lot: landed, 90% paid beyond with `wharfValve` on |
| bought its basket of the city's exports with its spare | split: the estates from their spare (as now); the Wharf from its credit (4.5) |
| paid its posts | the Wharf's purse pays Grayrock's 58 (Phase 6, unchanged) |
| the season caps on its supply | the sea's seasons on the Wharf's landing of non-area goods |
| `countryOff`, goods it no longer supplies | `wharfOff`, goods it no longer lands (the DM's per-good switch, per world) |
| arrivals' purses | the Wharf's purse (unchanged) |
| the Houses' tables, patrons' work | the estates' (unchanged) |
| "the Country's table" (`estatePeople`, `otherWorkers`) | "the estates' table" (text only) |

## 5. Orders by wagon

- **One entry point**: `EconomyResult Society::orderFromWharf(buyerAccount, item, n, why)`: the Wharf's lot as a draw
  of its own (4.3's money), carriage by weight to the carters (`carriageSplit`, the treasury's subsidy share as for any
  draw), whole or nothing. `drawFromStore` calls it for its Wharf lot; nothing else needs a second path.
- **Who orders**: every till the store room serves already does through `restockFromStore`: the city's shops, the
  communities' stores and alehouses (`cn_<community>_store`, the Lamp and Pick...), the country workshops' inputs. The
  farms' baskets (`basketBuys` from `RatwFarms.cpp:498`) fall back to `orderFromWharf` for a good on the imports list that
  no open maker holds (rare; a test proves the path). The towns to come (doc 42's `stores:<town>` and town buyers, when
  the communities become towns) call `orderFromWharf` the same way.
- **Carriage** is today's: weight × rate / a wagon load, paid to the carters (the Carters' Yard by the East Gate), the
  same for a shop on Market Square and the Frosthollow stores. Far places paying more by the walk is open question 4
  (recommended later: no order uses distance yet, and it would raise every community's prices at once).
- **A wagon seen on the road** from the New Store to a community's store is not built (doc 68, 6 left it too):
  `World::sendCaravan` needs towns at both ends, and the communities stay regions until the user makes them towns (doc
  72, Phase 0). Open question 5.

## 6. Controls (the DM and Atlas)

**New rules** (`items::Rules`, the data file, `map_editor.RULE_DEFAULTS` and ranges, the Editor's rules, no migration:
`live.economy.rules` is by name):

| Rule | Range | Default | What it does |
|---|---|---|---|
| `wharfValve` | 0-1 | 1 (0 through Phase 0) | 0: the Wharf is the Country renamed (no beyond, no takes, no export basket) |
| `wharfFee` | 0-50 | 10 | the Wharf's percent (4.7) |
| `wharfKeepDays` | 1-365 | 28 | days of a farm's yield it keeps before the Wharf takes the rest |
| `wharfKeepLeast` | 0-10,000 | 20 | pieces a farm always keeps |
| `wharfImportScale`, `wharfTakeScale` | 0-1,000 | 100 | percent on the lists' `most` (0 turns a side off) |
| `wharfExportMost` | 0-100,000 | 300 | pennies a day the beyond pays in at most (takes, the export basket, carriage, fee) |
| `wharfCreditMost` | 0-1,000,000 | 10,000 | the most credit held |
| `wharfInjectADay`, `wharfTakeADay` | 0-1,000 | 0 | the standing levers (4.6) |
| `wharfActionMost` | 0-1,000,000 | 2,000 | the most one DM action moves the credit (0: refused) |
| `estatePostsDays` | as `countryPostsDays` | 2 | renamed (2.1) |

- **Atlas (DEV)**: the Economy page's `CountryPanel` becomes **`WharfPanel`**: the switch and reserve, the per-good
  "the Wharf lands" switches (`wharfOff`), the lists read from the file (per good: imports or takes, on, most), the rules
  above in the Rules panel. Atlas has no live action.
- **The DM (live worlds)**: the Economy tab the same; the Money tab lists `outside:wharf` as "The Wharf's purse (House
  Grayrock's estate)" and a new line **"Beyond the realm"**: in, out and net since the world began, the credit now and
  its last moves (from the checkpoint's `society.wharf`); a form for `wharf.credit` (amount, a note, the DM's password as
  the other money actions), audited "wharf.credit".
- Per-world per-good edits (a good's `on` and `most` per world) need a column: Phase 4's migration 0061
  (`live.economy.wharf jsonb`, with a `wharf` line in `live.people_manifest` as 0060 did for the farms). Until then the
  lists are the file's and the DM switches a good's imports with `wharfOff`.

## 7. Reports

- **`world_check`** economy line: `wharf` {`imported`: {good: [pieces, pennies]}, `taken`: {good: [pieces, pennies]},
  `basket`: pennies, `in`, `out`, `fee`, `credit`, `moved` (the DM's and the rules' moves today)} and the run's totals;
  its end line: "money conserved: true; in from beyond N, out M, net K" with the check `in - out == the money's change
  less the founding mints` (the farms' and works' floats and purses stay counted as minted, doc 71).
- **`econ_report`**: a section **"The Wharf"**: a table by good (landed a day, pieces and pennies, by buyer trade;
  taken a day, by farm), the export basket by good, the money in and out of the realm a day and by season, the credit at
  the start, the end and its least, the fee; the imbalance tables gain a column "the Wharf filled" (short goods) and "the
  Wharf took" (piling goods), so a filled shortage is still listed; the estates' table gains "the Wharf's fees". The
  section reads old runs' `country*` keys.
- `good_sales.py`-style trade rows name the Wharf `wharf` and the beyond `beyond`.

## 8. Tests

- `wharf_tests.cpp` (from `country_tests.cpp`), new cases:
  - **the alias**: a save with `outside:country` (cash and stock) and a rule `countryPostsDays` 3 loads as
    `outside:wharf` with both and `estatePostsDays` 3, the old account gone, conserved, the estates not split again;
    `fund: "country"`, payer `"country"`, `countryBasket` and the world record `countryoff` read as before;
  - **imports**: a draw of butter with the dairies holding 3 of 10 takes 3 from them and 7 from the Wharf; the cost
    shared by pieces; the toll to the treasury; 90% of the Wharf's share sunk, 10% kept; `minted - sunk` matches the
    money; `farms_out_goods` counts butter; the cap (`most × scale`) holds across shops in a day; `wharfOff` and
    `never` refuse (cheese never, even listed or switched on); a refused create refunds the sink; `wharfValve` 0: the
    Country's behaviour exactly (the whole share kept);
  - **takes**: a farm holding 40 days of its hides gives the excess over 28 days at 16:00, within `most` and the credit;
    the farm's takings less the toll, the treasury's toll, the carters' carriage, the goods gone, its land rent tonight
    counting them; a farm under its keep gives none; the credit falls by price, carriage and fee;
  - **the export basket**: credit left after the takes buys the basket from open makers, the hour's unspent sunk back,
    nothing minted left in the purse; the estates' basket untouched;
  - **the credit**: imports raise it, exports lower it, never below 0 by trade; `wharfInjectADay` lets exports run up to
    `wharfExportMost` a day; a take below 0 stops exports until imports repay; `moveWharfCredit` over `wharfActionMost`
    refused; saved and restored;
  - **orders**: a community store's draw and a farm basket's fallback both through `orderFromWharf`, carriage by weight.
- `farms_behaviors`, `workshops_behaviors`, `trades_tests`, `season_tests`, `treasury_tests`, `econ_fix_tests`: the
  renames (`supplyLeft`, the data file's name); otherwise unchanged.
- `tools/test_econ_report.py` (the Wharf section, old keys), `test_world_store.py` (the entity aliases, the rule alias),
  `test_dungeon_master.py` (`wharf.credit`: range, the rule 0 refusal, the audit), the Editor's `economy.test.ts` (the
  new rules, `WharfPanel`'s labels), `test_item_catalog` (the lists disjoint, unknown goods refused).
- `ctest` in `build-core` all pass; the catalogue loads with no new errors.

## 9. Build order

| Phase | What | Checks | Runs (each asked first) |
|---|---|---|---|
| **0. The rename** | 2.1-2.3; `wharfValve` 0; no behaviour change | the tests; the deterministic 30-day compare against `ridgemere2` (every account equal, `outside:wharf` = old `outside:country`) | 30-day smoke |
| **1. The beyond and imports** | `SocietyState.wharf` (in, out, credit, saved in `RatwWire.cpp` and `RatwGameJournal.cpp`'s society paths), `stablePrice`, the lists' reader, `supplyLeft`, the Wharf's lot, `orderFromWharf`, the sink, the fee; `wharfValve` 1 with takes and the basket not yet built (credit only accrues) | imports within caps; butter's shortage filled; conserved; the treasury's toll on imports as before | 30-day smoke |
| **2. Takes, the basket, the credit** | 4.4-4.6, the rules, the 18:00 settle (before `settleEstates`), world_check's and econ_report's sections | piles level off over 28 days' yield; the credit near level; money's net change ~the fees | 60-day smoke |
| **3. Controls** | `WharfPanel`, the rules in both suites, the Money tab's "Beyond the realm", the `wharf.credit` action and its handler | the DM's action applied on a scratch server of DEV (no real server), audited | none |
| **4. Later** | migration 0061 (per-world lists); distance carriage; wagons seen; a buffer stock at the New Store | | |
| **The run** | 3 years on build 62's export (or the next), beside iron6 | the marks below | **3-year run** |

**Marks** for the 3-year run: money conserved every day; the realm's money within ±5% of iron6's at the end (the valve
neither floods nor drains); no good both landed and taken; butter's draws filled ≥ 95% of days; hides and bones stock
flat in year 3 (growth under 10% of a year's sales); the credit never at its cap and never below 0 without a DM move;
the estates' posts as iron6 or better; the treasury over its float every night; hunger no worse.

## 10. Risks and the guards

| Risk | Guard |
|---|---|
| **Inflation from injected money** (hides alone ~115p a day at the stable price; the realm holds ~64,400p) | the credit: the beyond pays in only what it was paid; the fee (~15-30p a day) the only standing injection; the DM's moves capped per action and as goods at most `wharfExportMost` a day; default 0 |
| Deflation (imports with nothing to take) | the export basket spends the credit back on the city's exports; the credit's cap shows in the report when the realm overpays; the DM injects |
| **Hiding real shortages** (imports fill what the farms should) | per-good lists, small caps, the new producers' goods off; `farms_out_goods` and the imbalance table still count a filled shortage, with "the Wharf filled" beside it |
| Hiding mis-sized yields (takes of piles) | the yields-patch goods (flax, ore, charcoal, limestone) off; 28 days' yield kept, so the pile is visible in the farm's store |
| **Double-routing with the store room** | one path: the Wharf is a lot inside `drawFromStore`, after the farms; the lists disjoint and `never` checked at load; takes only from farms and works, at 16:00, of goods not on imports |
| Cheese | on `never`: refused on both sides, whatever the DM sets (the user) |
| The toll changing | the rate is the user's and unchanged; applied to the Wharf's lot as the Country's was and to the takes as any farm draw (open question 3) |
| The rename breaking saves or old reports | the load alias (2.3), the readers' aliases, Phase 0's deterministic compare |

## Open questions for the user

1. **The data file's name.** Recommended: **`Data/Items/city_loop.json`** (it holds the whole loop: the Wharf, the
   estates, wages, food, comforts, rules, services; `tools/city_loop.py` already checks it). Or `wharf.json`, or keep
   `country.json` (no churn, but "country" stays on the Wharf's file).
2. **`countryPostsDays`'s new name.** Recommended: **`estatePostsDays`** (it is every estate's nights, the Wharf's
   among them); or `wharfPostsDays`.
3. **The toll on the Wharf's takes.** Recommended: **yes**, the farms' goods pass the gate as any draw's (the farm gets
   the stable price less the toll, the treasury its toll: ~35-50p a day more to it). Or no toll on exports (the farms
   get the whole price: more money to the Crown's and Fell's land rents, none to the treasury).
4. **Distance in carriage** for the far communities. Recommended: **not now** (no order uses distance; it would raise
   every community's prices together). Or a rule `carriageFar`, percent more a thousand tiles of walk from the wharf.
5. **Wagons seen on the road.** Recommended: **with the towns** (when the communities become towns, `sendCaravan`
   carries the Wharf's orders to them). Or now, as road folk with no load (looks only).
6. **A buffer stock at the New Store** (the Wharf holds what it takes for some days and sells it back when the realm runs
   short). Recommended: **not now**: the farms' own stores already hold 28 days; it adds stock to save and a second
   path. Later if a seasonal good both piles and runs short.
7. **Takes from city makers and shops.** Recommended: **no** (their shelves follow their sales and their prices fall
   on a glut). Or a rule `wharfTakesShops`.
8. **Butter imported.** Recommended: **on** (short ~76 a day after the dairies: "import shortages"); it is also what
   gives the credit for the takes at first. Or off (the dairies' lane fixes butter; then the takes wait for credit or a
   DM injection).
9. **The fee.** Recommended: **10%, the Wharf's (Grayrock's)**, paid by the beyond on exports, out of the imports'
   takings on imports. Or 0 (the Wharf earns nothing of its trade), or more (a higher standing injection).
10. **The new producers' goods** (eggs, sheep, sheepskins, brandy, apple brandy, copper and tin). Recommended: **listed,
    off** (eggs...) or the Wharf's as now (copper, tin) until their lanes land; then the DM decides each. Or on now as a
    fallback after the farms.

## Progress

### The build (branch `r2-wharf`, 2026-11-04)

Built from ridgemere2 at 7e45f4d (the producers merged), this doc merged in first; the open choices as recommended
(`city_loop.json`, `estatePostsDays`, the toll on the takes, no distance in carriage, no wagons seen, no buffer stock, no
takes from shops, butter imported, a 10% fee the Wharf's, the new producers' goods listed off). No DEV or PROD write, no
map change, no NPC made, no post added; the treasury's levers untouched; cheese as it is (on `never`).

**The rename (Phase 0).**

- Core: `Society::WharfFund` = `outside:wharf` (and `OldCountryFund`, read only); `items::CityLoop`/`items::cityLoop()`;
  `EconomySpec::wharf`, `wharfReserve`, `wageWharf`, `wharfOff`; `supplyLeft`, `estatePostsOwed`, `estateKeep`,
  `estateSpare`, `wharfBasketShift`; `LoopDay::basketSpent`, `basketUnspent`, `wharfSales`, `patronsEstates`,
  `estatePosts*`; a post's `Pay::kind` "estate". One departure from 2.1: `countryBuys` is **`estatesBuy`** (it is the
  estates' basket; the Wharf's own export basket is `wharfExports`). The market `wharf` ("the Wharf"); the data file
  `Data/Items/city_loop.json` (format `ratw-city-loop`), its wage rules' and patron groups' payer **`estates`**, Grayrock's
  estate `fund: "wharf"`, the rule `estatePostsDays`; country_farms.json's `wharfBasket`. Journal reasons "to the Wharf",
  "shipped from the Wharf", "... from the Wharf's stores", "an arrival's purse, from the Wharf", "the Wharf's purse split",
  and the store room's refusals in the Wharf's words. world_check's economy keys `wharf`, `wharf_sales`, `basket_spent`,
  `basket_unspent`, `estate_rents*`, `estate_land_rents*`, `estate_posts_*`, `patrons_estates`, `wharf_basket_shift`; its
  trade group and payer kind `wharf`, and `beyond` for the world beyond. `country_tests` is `wharf_tests`
  (`wharf_behaviors`). Atlas's `CountryPanel` is `WharfPanel`; the Editor's, Atlas's and the DM's economy keys `wharf`,
  `wharfReserve`, `wageWharf`, `wharfOff` (map_editor, world_store, studio, model.mjs, economy.ts).
- Kept "country": the countryside (country_farms.json, the country workshops' ids, `cn_` ids, the unplaced country folk,
  arrivals "came in from the Country", "eaten in the Country"), tools/country_industry.py, the world file's records (`loop
  ... country on ...`, `countryoff`), live.economy's columns (`country`, `country_reserve`, `wage_country`, `country_off`,
  mapped in world_store's LOOP_COLUMNS), migrations and old doc entries. Many code comments still tell the history in the
  Country's words where they describe doc 68-72's steps.
- Aliases: `Society::restore` moves an old `outside:country` (coins and goods) into `outside:wharf` and closes it, beside
  doc 68's `fund:store` move, before the estates' night; `items::ruleName` reads a `rule "countryPostsDays"` record (and the
  data file's key) as `estatePostsDays`; the data file's payer `"country"`, `fund: "country"` and `countryBasket` read as
  before; an older data folder's `country.json` is read when `city_loop.json` is missing; map_editor's `legacy_economy`
  (and model.mjs's `legacyEconomy`) reads an older project's or POST's `country`, `countryReserve`, `wageCountry`,
  `countryOff` and rule `countryPostsDays`, the new names written only; econ_report (`renamed`), econ_weeks, econ_detail,
  people_summary, country_industry and world_check's reason reader map the old keys and "to the Country"; the DM's Money
  tab shows an old `outside:country` as the Wharf's purse.
- **The check**: `tools/wharf_rename_compare.py` on 14 days (`--simulate 6 342 --farms 0 --farms-every 1 --deterministic`,
  build 63's export) of ridgemere2 (7e45f4d) against r2-wharf with `rule "wharfValve" 0` added to the export's records:
  **0 differences** in every number of every economy line once the keys are mapped (the accounts, the money, the estates,
  the farms, the trades). (The roll that rounds a basket's odd piece keys the Wharf's purse by the Country's old name, so
  the rename moves no roll.)

**The valve (Phases 1-3).**

- `Core/RatwWharf.cpp`: `stablePrice` (the store room's at markup 100, the area's year price or the season's, never a
  shop's), `wharfImports`/`wharfRoom` (on the list, on, not `never`, not the world's `wharfOff`, `most × wharfImportScale`
  less today's landing), `wharfTakes`/`wharfKeep` (the larger of `wharfKeepLeast` and `wharfKeepDays` of the farm's yield
  a day today, its makes' own yield too), `wharfExportRoom` (the credit, at most `wharfExportMost` a day, never under 0),
  `orderFromWharf` (any buyer's order: the stable price, the gate toll, carriage by weight to the carters with the
  treasury's share, whole or nothing), `bookWharfLanding` (the one booking of a landing, a draw's lot or an order),
  `moveWharfCredit` (the DM's lever), `settleWharf` (the standing levers at 18:00, before the estates), `wharfTakesRun`
  (16:00) and `wharfExports` (each shop hour, after the estates' basket).
- Store draws: an area's good the farms hold too little of is filled by **the Wharf's lot last** when the good is on its
  imports, within its room; the cost less the toll shared by pieces (the farms' lots as before over their pieces); the
  draw still counted in `goodsOut`/`areasOut` (a filled shortage stays seen). `supplyLeft` is the farms' hold plus the
  Wharf's room. A good no area grows is the Wharf's lot whole, as the Country's was.
- Money: of what a landing paid the Wharf (after the toll), **its fee kept** (`wharfFee`, 10%) and the rest **sunk**
  ("paid beyond the realm: <good>", `outside:wharf -> beyond`), the credit raised by it (to `wharfCreditMost`). Takes and
  the export basket **mint** ("from beyond the realm: <good>", `beyond -> outside:wharf`) the price, the carriage and the
  fee, the credit lowered by it; the export basket's unspent budget un-minted at once; `conserved()` unchanged. The book
  (`LoopState::Wharf`: in, out, fee, moved, credit) saved in the save's `loop.wharf` (the checkpoint; not the journal,
  whose `minted`/`sunk` are). With `wharfValve` 0 none of it runs (the Country's behaviour, the Phase 0 check).
- Takes at 16:00 from the farms and works over their keep, the largest excess first, within the good's most and the
  export room; the farm paid the stable price less the gate toll (its takings, and its rent's base tonight), the toll to
  the treasury ("gate toll: to the wharf"), carriage to the carters ("carriage to the wharf"), the goods shipped.
- The export basket (`wharfBasketNow`, the estates' basket's blend, shared) from the credit left after the day's takes
  (their want kept back before 16:00), at most `wharfExportMost` a day; its carriage out as any basket's (the treasury's
  share too: a departure from 4.4's "the beyond pays it", which holds for the takes).
- The farms' baskets fall back on `orderFromWharf` for a good on the imports that no open maker holds.
- The lists (city_loop.json `wharf`): imports aqua vitae 10, offal 20, butter 90 (on); wheat 40, eggs 30, sheep 4,
  sheepskin 10, brandy 2, apple brandy 4, copper ore 4, tin ore 2 (off: the mines and farms offer them now); takes hide 30,
  bones 35, tallow 6, honey 6, small pelt 2, straw 2, berries 2, resin 1 (on); flax 20, iron ore 10, charcoal 15, limestone
  5 (off); never cheese. A good on both lists or on `never` is dropped at load (`WharfLists::refused`).
- Rules (Core's `Rules`, the data file, map_editor's RULE_LIMITS, model.mjs, economy.ts's rules panel; no migration):
  `wharfValve` 1, `wharfFee` 10, `wharfKeepDays` 28, `wharfKeepLeast` 20, `wharfImportScale` 100, `wharfTakeScale` 100,
  `wharfExportMost` 300, `wharfCreditMost` 10,000, `wharfInjectADay` 0, `wharfTakeADay` 0, `wharfActionMost` 2,000.
- The DM: the live action **`wharf.credit`** (`tools/dungeon_master.py` LIVE_KINDS and its permission; payload amount and
  note, checked against the world's `wharfActionMost`, refused at 0, audited "wharf.credit: ±Np: note"), applied in
  `Game::applyDmActions` by `moveWharfCredit`; the Money tab's "Beyond the realm" (in, out, net, fees, credit, moved, from
  the checkpoint's `society.loop.wharf`) and its form (`Editor/src/dm/WharfCredit.tsx`). Atlas: `WharfPanel` with the
  Wharf's lists read from the file (`wharfLists`), the per-good "lands" switches (`wharfOff`), the rules on the Rules
  panel.
- Reports: world_check's economy line `wharf_day` {valve, imported {good: [pieces, pennies]}, taken, basket, in, out,
  fee, moved, credit, book_in, book_out, book_fee, book_moved, sunk}; its end line "the Wharf beyond the realm: in, out,
  net; its fees; credit; money conserved (money = minted - sunk)". econ_report's "The Wharf and the Houses' estates" (the
  old Country section, reading old runs) and **"The Wharf: beyond the realm"** (a day: in, out, net, fees, the export
  basket, moves; the credit's course; by season; by good landed and taken).
- Tests: `wharf_tests` (an old save's `outside:country` and the book through the wire; the Wharf's landing sunk, its fee,
  the credit, conserved, the valve off as the Country; the stable price; cheese never, the lists disjoint; an order from
  the Wharf by wagon to a far community's store with carriage; the credit moved within `wharfActionMost`, refused past it
  or at 0, below 0 stops exports, the standing inject at 18:00 under its cap; an old rule name); `farms_tests`'
  `theWharf` (aqua vitae landed after the farms, the fee and the sink, the cap across draws, the world's off switch and
  the valve off; butter from the dairies then the Wharf, the cost shared; hides over the keep taken at 16:00, 30 of them,
  at the stable price, the farm paid less the toll, the minting booked; a farm under its keep gives none; no credit, no
  takes); the doc 68-72 suites run with `wharfValve` 0 (their flows as they were), game_tests' `/store` with the fee.
  test_econ_report (the Wharf's section; old keys), test_map_editor (the aliases), test_dungeon_master (`wharf.credit`),
  content.test.mjs (an older project's keys). ctest 79 of 79; the Python suites as before but the known failures
  (test_map_editor's Greyfen 11/10, test_studio's 10 farm areas, test_ridgemere2's two live-DEV checks); the Editor's
  typecheck clean, its tests 80 of 81 (the same Greyfen 11/10, failing at ridgemere2 too).

**The 60-day smoke** (build 63's export, `--simulate 6 1446 --farms 0 --farms-every 1 --deterministic --threads 4
--economy-level full`; `artifacts/economy/r2/wharf-smoke-report.md`, `wharf-smoke.out`; ridgemere2's same run beside it,
`wharf-smoke-base.jsonl`, not committed):

- **Money conserved** every day; the realm's money at the end **64,557p against ridgemere2's 64,595p**: the difference,
  −38p, is the Wharf's net over the run (in 15,197p, out 15,235p, its fees 1,403p). Nobody hungry.
- **The credit's course**: the first ten days, before the farms' areas are on, the Wharf lands nearly everything (as the
  Country did: 2,600p a day out on day 0), so the credit climbs to **8,360p** by day 5; the export basket spends it back at
  up to 300p a day (232p a day over the run), to **38p** on day 59. Days 10-59 a day: in 248p, out 94p, net +154p, fees
  20p, the export basket 227p, no moves.
- **Landed a day** (days 10-59): butter **36.0** (61p; ridgemere2's butter draws refused 114 a day, now 27 counted short,
  the Wharf's filled ones among them; its most, 90, never reached), linen 2.6, iron hoops 3.1, copper bars 0.8, tin bars
  0.2, clay pots 1.9, the gathered herbs and the rest under a piece each. **Aqua vitae and offal: none drawn** (no maker
  drew either, in ridgemere2's run too).
- **Taken: none.** In 60 days from their founding no farm holds more than 28 days of a good's yield (the hides', bones'
  piles of iron6 grew over years); the path is the tests'.
- **The estates**: 0 nights short over days 10-59 (as ridgemere2), but **7-8 nights short in days 0-9** for every estate
  (2 the Crown's; ridgemere2 none): the Wharf's purse keeps only its fee of those first days' landings, the Country's
  whole takings before, and the estates' first-night split and Grayrock's grants are poorer for it. Grayrock's purse ends
  at 23p (least 2p) as in ridgemere2; the Crown's 1,358p (ridgemere2 1,385p). The estates' basket 163p a day (228p).
- The treasury 850 to 2,093p (least 850p; ridgemere2's least 1,128p), the toll 1,549p a day (1,546p).

**Open.**

1. **The estates' first nights** with the valve on (above): in a world whose farms are on (DEV now) the Wharf's lot is
   ~25p a day and Grayrock keeps ~2.5p of it, ~22p a day less than the Country kept: covered by the Crown's grants in the
   smoke; the 3-year run (asked first) should watch the estates' nights and the Crown's purse beside producers7. The fee
   (`wharfFee`) is the lever if it bites.
2. **The takes** wait for piles: the 3-year run shows whether hides, bones, tallow and honey level off over 28 days'
   yield, and whether the credit then binds them.
3. **The credit at a fresh start** runs up to ~8,000p while the farms switch on; a live world starts at 0 (no saved
   book) and accrues ~85p a day from its landings.
4. **The book isn't journalled** (the checkpoint carries it, every 30 s); `minted` and `sunk` are, so money stays
   conserved after a crash; only the book's last half-minute may be lost.
5. **The DM's action on a scratch server**: `Game::applyDmActions` doesn't run on a scratch server (`options_.scratch`), so
   Phase 3's check there can't be done; the action is tested in Core (`moveWharfCredit`) and in the DM's queue.
6. Not built (Phase 4): per-world per-good lists (migration 0061), distance in carriage, wagons seen, a buffer stock.

### The review (2026-11-04)

Two defects fixed, both in the export basket (4.5); everything else checked held (the money's legs booked once each,
the refunds, the caps, the stable price, the orders, the rename and its aliases, the DM action, the records):

- **A good both ways in a day.** The export basket bought from the makers' shelves goods the Wharf had landed that day
  (the smoke landed copper and tin bars and iron hoops, all in the basket; aqua vitae is on the imports list and in the
  basket too), so the beyond bought back what it had sold the realm. Now the basket skips a good on the imports list
  (on) and any good the Wharf landed today. `wharf_tests` `theExportBasket` (buckles, without the fix, bought back).
- **The treasury paid the beyond's carriage.** `basketBuys` charged the export basket's carriage out through
  `carriageSplit`, so the treasury paid its 25% subsidy on the beyond's own purchases (4.5: "carriage out by the
  beyond"). `basketBuys` takes `subsidised` (false for the export basket only). And its carriage now stays inside
  `keep` for every basket (a cut-down lot was checked against its price only, so the carriage could take a purse under
  its keep: for the export basket, into Grayrock's own coins, the credit then lowered by more than was minted).

Left as they are (noted): the farms' baskets' Wharf fallback never fires with the shipped data (no good of the farms'
basket is on the imports list, and a good no area grows isn't offered by the fallback); for an area good it would land
without looking at the farms' stores. The takes stop the day's run at the first good the credit can't fit, not trying
cheaper goods after it. A good can't be both imported and taken (dropped from both at load): sheepskin, copper and tin
ore are on the imports list (off), so to take their piles they must move to `takes`. Not re-run: the smoke's numbers
are before these fixes (the basket a little smaller on the days the Wharf lands one of its goods, the treasury a little
fuller).

### wharf7's 3-year run (2026-11-04): stopped on day 44, the realm's money drained

The Wharf merged into ridgemere2 (01862eb; the Editor's economy tests brought up to `city_loop.json` and the wharf
rules, e18a6a9: three of them failed after the rename, unseen behind `content.test.mjs`'s known Greyfen failure). The
3-year run on build 63's export (`export63`, `--farms 0`: the first area on day 0, one every 28 days, as producers7)
was stopped by hand on day 44: the days had slowed from 2-9 s to 40-50 s of wall time (the run would have taken some
ten hours), and the verdict was already plain. Report, patch and log: `artifacts/economy/r2/wharf7-report.md` (44 days;
its full log's last, cut line dropped).

Days 0-43, against producers7's same days:

| | wharf7 | producers7 |
|---|---|---|
| Money conserved every day | yes | yes |
| The realm's money, day 0 -> 43 | 48,914 -> 8,366p | 51,220 -> 55,463p |
| The beyond paid in / was paid, a day | 272p / 1,342p (net -1,070p) | - |
| The treasury's least | 200p (from day 10) | 1,021p |
| Hunger | 425 hours at most, ~650 households short a day (1,277 on day 43) | none (0.4 short) |
| Estates' posts | short every night (day 43: 27p of 824p paid) | never short |
| Wage support, a day | 9p | 228p |
| Areas on, day 43 | 2 | 2 |

**Why.** Every landing pays 90% of its takings beyond the realm (`bookWharfLanding`), not only the listed imports.
With the farms switching on one area every 28 days, the Wharf is still the realm's farmer for most goods (as the
Country was), and now each of those sales leaves the realm: apples 114 a day landed (114p), raw meat 112p, pig 104p,
firewood 88p, fish 85p, shellfish 74p, eggs 74p (listed off, landed anyway as an area good not yet on), honey 68p,
milk 64p, meal 62p, butter 61p... about 1,340p a day out. What comes back is capped at `wharfExportMost` (300p a
day) and the credit sat at its cap (10,000p) from day 5, so what the realm paid over it was simply lost ("the realm
overpaid"). The Country kept those takings in Grayrock's purse, so the estates' posts were paid; now Grayrock keeps
10% and its posts go unpaid, the treasury runs to its floor, wages and food money run out, and the hungry slow the
ticks. The 60-day smoke didn't show it because its farms came on one area a day.

Not measured (the run stopped): the takes, aqua vitae and offal, far communities' orders, the steady state with every
area on, prices.

What it needs (not done here, for the user): a landing should sink only where the realm truly imports, i.e. the
listed imports that are on (and perhaps the goods no area grows), with an area good not yet switched on landed as the
Country did, its takings kept in the realm; or the sink bounded by what the beyond pays in (the credit, so the valve
nets to about 0 a day), not left to run while the credit sits at its cap.

### After wharf7: the valve bounded (branch `r2-wharf-b`, 2026-11-04)

Both of wharf7's fixes, together (code first, data second; no treasury lever, no cheese):

- **Only true imports are paid beyond the realm** (`Society::wharfPaysBeyond`): a good on the imports list (on) the
  farms couldn't fill, or a good no area names (planks, linen...). An area's good whose farms aren't on yet (apples,
  meat, firewood, fish, eggs... while the areas come on one by one) is landed as the Country did: its takings kept whole
  in the Wharf's purse (Grayrock's), nothing sunk, the credit unmoved.
- **The sink never runs past the credit's room**: a landing pays the beyond at most `wharfCreditMost - credit` (the
  most the realm may have paid beyond and not had back), the rest kept in the Wharf's purse. Before, what went over the
  cap was simply lost to the realm. `wharfCreditMost` is now **2,000p** (was 10,000p: ~4% of the realm's money at most
  ahead of the beyond's pay-back, about a week of the export basket).
- **Yields** (data, `tools/econ_report.py --apply` of producers7's patch, `--goods timber,flax,eggs,cider`): the two
  logging camps' timber and the grain farms' flax half the way down to what sold (timber 19.9 to ~18.0 a day, flax 29.4
  to ~28.0); the dairies' eggs (18/20/16/12 to 24/25/21.7/18.5) and the orchards' cider (about +70%) the whole way up,
  both supply-bound every season.
- Tests: `wharf_tests` `theWharfLands` (3p of room left: 3p paid beyond, the rest kept), `farms_tests` `theWharf` (a
  good of an area not on: kept whole), `game_tests`' `/store` draw (iron bars, the iron area's, its works off in that
  world: the Wharf keeps all of it). ctest 79 of 79; the Editor's lib tests 68 of 68.

**The 60-day smoke** (build 63's export, wharf7's flags: `--farms 0`, an area every 28 days; `wharf7b-smoke.out`, not
committed): money conserved every day; the realm's money 51,194p to 59,355p (the steps on days 28 and 56 are the farms
switching on, as producers7's); the beyond paid in 10,120p and was paid 10,155p over the 60 days (~170p a day each way,
the credit 4-83p); the estates' posts paid every night (824p of 824p); nobody hungry (1 hour at most); the treasury's
least 1,209p; wage support ~248p a day; the days 1.3-2.0 s of wall time (wharf7: 40-50 s by day 40).

### wharf7b's 3-year run (2026-11-04): money level, the estates still short

After the valve's bounds (above), build 63's export, wharf7's flags (`--farms 0`, an area every 28 days), 1,095 days,
36 minutes, the days 1.3-2.2 s of wall time throughout. Report, patch and log: `artifacts/economy/r2/wharf7b-report.md`,
`wharf7b-report.md.patch.json`, `wharf7b.out`. Year 3 a day unless marked, against producers7 (the Country, no valve):

| | wharf7b | producers7 |
|---|---|---|
| Money conserved every day | yes | yes |
| The realm's money at the end | 64,588p | 64,595p |
| The beyond paid in / was paid (years 1, 2, 3) | 112.7 / 112.8, 56.3 / 56.3, 52.7 / 52.6p | - |
| The Wharf's fees; the export basket | 3.2p; 25.4p (93p in year 1) | -; - |
| Its imports | butter ~56 a day (the dairies' 37.9 first); aqua vitae and offal no draws | butter short |
| Its takes | bones, honey, tallow, straw, small pelts within the credit; **no hides** (8,655 a year piling) | - |
| The treasury's least, its 4-week means | 1,970p; 4,778 -> 5,777p | 1,903p; 4,108 -> 4,633p |
| Wage support | 296p | 295p |
| Hunger | 6 days, 9 hours at most, 0.31 households short | 3 days, 8 hours, 0.28 |
| **Estates' posts** | **788p of 824p, 107 nights short** (Grayrock's and Greatly's) | 803p of 824p, 40 nights |
| The Crown's purse (year ends) | 1,002, 976, 625p (least 178p) | 946, 1,005, 714p |
| The Crown's business rents | **60.9p** | 103.1p |
| The foundry's wages paid | 3.4% (it still stops selling in year 2) | 0.2% |
| Oakhaven's press, Southmeadow's pottery | 69.1%, 76.6% of wages | 87.9%, 85.7% |
| Eggs sold / farm draws short | 66.9 / 97.7 | 49.5 / 114.1 |
| Cider sold / short | 6.4 / 18.2 | 3.5 / 9.7 |
| Timber, flax sold (not short) | 29.6, 39.6 | 29.0, 41.3 |

**Verdict: not stable.** The drain is gone: the realm's money ends where producers7's did, the valve nets to nothing
each year (the credit 7-83p), the treasury rises, nobody goes hungry, and the run keeps its pace. But the estates' posts
went short 107 nights in year 3 (~36p a day), worse than producers7's 40, because the Crown's purse, which grants
Grayrock and Greatly their gap, falls by year 3: its business rents are ~42p a day lower than producers7's (not traced
here; its two country workshops account for ~3p of it). The Wharf earns Grayrock almost nothing (3p of fees a day):
with only butter imported, the credit is small, so the export basket buys ~25p a day and does not revive the foundry,
and the takes never reach hides. Eggs and cider sell more but are still short.

### Round 1 of stabilizing (branch `r2-stable8`, 2026-11-04): the estates paid, the foundry working

The user: "Do what you need to in order to stabilize the economy. If we need more logging make more logging." No
treasury lever (toll, wage support, float), no move of the Wharf's credit, cheese left as it is.

**The trace** (wharf7b's logs; per business from the full log, by till and good):

- **The Crown's business rents (60.9p a day in year 3 against producers7's 103.1p)** are its five country workshops'
  takings over their keep: the soapworks 16.9p (36.3p), Coldwater's weavers 5.8p (16.9p), Larkfield's weavers 7.0p
  (18.7p); the press and the smithy as before. Their takings fell 60p a day (the soapworks' 154 against 182p, the wool
  weavers' 131 against 150p, the linen weavers' 118 against 132p) because the households' comforts fell 38p a day (398
  against 436p): more of their food money went on eggs (66.9 a day against 49.5) and the Wharf's butter (~56 a day), and
  Fell's still sold ~32p more apple brandy to them out of the same comforts budget. A workshop's wages are fixed, so its
  House takes all its marginal takings; a farm's takings go first to its basket. Nothing was lost: the money moved from
  the estates' workshops to the farms' baskets.
- **The estates' gap was structural, not the Crown's alone.** Year 3's rents in, all five estates, were ~784p a day
  against 824p of posts (year 2's ~855p): the estates' purses fell to about one night's posts (~850p together), and a
  Restday's thin land rents (day 811: the farms' rents 130p, the day before 527p) left the next night short. Meanwhile
  the dairies' baskets spent 100.8p a day each against their design's 53.5p (`basketADay`), the grain farms' 69.2 against
  48.3p, orchards 1 and 3 62 against 41p, the eel marsh 33.7 against 22.1p: ~237p a day of the farms' takings over their
  design spent on carts, stone and hoops while their Houses' people went unpaid.
- **The Houses' city businesses** (the user's candidates: the Anchor and Nailers' forges, the Rivet and Rain, the
  charcoal store and the sawmill; Greatly's plasterworks, colour works, masons' yard, yard and bank; Grayrock's
  ropewalk) made their keepers 0-2.5p a day each in year 3: listed in `houseBusinesses`, they would send their Houses
  nearly nothing. Not added.
- **The foundry** sold iron bars it drew from the store room in year 1 (46.6p a day to the smiths); once the iron area
  went on the store sent none, and the ironworks, first in every iron buyer's `suppliers` line, sold them all. No city
  trade buys its own goods (bronze, copper and tin bars, handbells): its takings 1.9p a day in year 3, its wages 3.4%
  paid. The Frosthollow mine digs 0.8 copper ore and 0.4 tin ore a day.
- **The hides** piled at the butchers (~5,000 at each of the big ones by year 3, ~24 a day; their bones the same), the
  byproduct of their carcasses: the tannery draws ~3.5 a day from the farms, and the takes only looked at the farms. And
  the takes' run stopped at the first good the room couldn't fit, and its proportional shrink floored to 0 when the room
  was small against the lot (room for one bone of 60 wanted: none taken).

**More logging** (the economy file, normal level; Tests/world_check.cpp): each estate's rents in by source (`land`, of
it `surplus`, `works`, `shops`: the workshops and the Houses' city businesses; `fees`: the Wharf's takings kept, its fee
and what the credit had no room for); `wharf_day` the takes from the city's shops (`taken_shops`), what each landing
paid beyond by good (`landed_beyond`) and what the takes wanted that the room could not fit (`unfit`).
`tools/econ_report.py` adds the estates by year and by source (rents in, land and its surplus, works, shops, fees,
grants and levy net, posts, nights short, the purse at the year's end), the takes from shops beside the farms', and the
takes' unfit pennies.

**The fixes** (commit 4a96d72):

- **The farms' surplus is their House's** (the rule `farmSurplus`, 1; Core/RatwFarms.cpp `farmBasketMost`, `payRents`,
  `farmsBuy`): a food farm's basket spends at most its design's a day (`basketADay`, its carriage within it), and once it
  has paid its crew, what its till holds each night over its float and that is its House's estate's, as a works' surplus
  is (doc 72: "each works pays its own crew and sends its surplus to the noble House whose land it is on"). The farms
  under their design (the pigs, the sheep) are untouched; the estates spend what is over their keep on the Houses'
  basket, tables and patrons, so the money goes back to the city's makers. The Editor's and Atlas's rule lists have it.
- **The takes** (Core/RatwWharf.cpp `wharfTakesRun`): in the list's order (city_loop.json; it was the alphabet's), a good
  the room can't fit left for the next, the shrink never past 1 at a step; a take's `shops` (businesses.json ids) are
  the city's trades it is taken from too, at the stable price and inside the walls (no toll), each shop keeping `keep`
  (or wharfKeepLeast, wharfKeepDays of its sales, or its shelf). The list now: the foundry's handbells first (keep 1, 4 a
  day: its posts' wages), then tallow (12) and honey (10) from the farms, then hides (40) and bones (60) from the farms
  and the butchers.
- **Imports**: eggs (40 a day) and cider (20) when the farms can't fill a draw; copper and tin ore (4, 2), the bells'
  metal.
- **Yields**: wharf7b's patch, `--apply --goods eggs,cider,tallow,honey` (eggs and cider supply-bound, the whole way:
  the dairies' eggs 24/25/21.7/18.5 to 30.3/30/27.6/25.6, the orchards' cider ~+50%; tallow and honey half way, ~-5%).
- Tests: `farms_tests` `theFarmsSurplus` (the salt works given 3,000p: its basket within its design's, the rest the
  Crown's that night, booked as its surplus, conserved; the rule off, no bound), `theWharf` (forty hides, the new most;
  room for no hide but a bone takes the bones, the unfit booked; the butcher's hides over its keep taken whole, no toll);
  `wharf_tests` (eggs and cider imported); the land rent's and the farms' basket tests run with the rule off. ctest 79
  of 79; the Editor's lib tests 68 of 68 (`content.test.mjs`' Greyfen failure and `test_map_editor`'s Greyfen test fail
  as on ridgemere2).

**The 60-day smoke** (build 63's export, wharf7b's flags, the final build; `stable8-smoke.out`, not committed): money
conserved every day (51,198 to 59,341p, wharf7b's smoke 59,355p); the estates' posts paid every night (824p of 824p);
nobody hungry (1 hour at most); the treasury's least 1,021p; wage support 235p a day (wharf7b's smoke 248p); the beyond
paid in 189p and was paid 190p a day; the butchers' hides and bones taken (8.4 and 7.7 a day), the foundry's bells 0.2.

**The 3-year run, stable8** (build 63's export, wharf7b's flags: `--farms 0`, an area every 28 days; 1,095 days, ~40
minutes beside another run, 1.5-2.6 s a day). Report, patch and log: `artifacts/economy/r2/stable8-report.md`,
`stable8-report.md.patch.json`, `stable8.out`. Year 3 a day unless marked:

| | stable8 | wharf7b |
|---|---|---|
| Money conserved every day | yes | yes |
| The realm's money at the end | 64,584p | 64,588p |
| **Estates' posts** | **824p of 824p, never short in three years** | 788p of 824p, 107 nights short |
| Estates' rents in (all five; posts 824p) | ~1,021p (the farms' surplus 293p: the Crown 212, Fell 64, Grayrock 18) | ~784p |
| What the estates bought (basket, tables, patrons) | 196p (luxuries for the Houses 57p, patrons 36p) | 0p |
| The Crown's purse (year ends; least) | 1,629, 1,557, 1,400p (909p) | 1,002, 976, 625p (178p) |
| All five estates (year ends) | 2,419, 2,189, 2,054p | 1,610, 1,469, 1,094p |
| The farms' baskets | 555p | 873p |
| The foundry's wages paid; to its House | 100%; 11.9p (bells 2.4 a day to the Wharf) | 3.4%; 0p |
| The ironworks to its House | 3.2p | 23.8p |
| Eggs to households / short | 106.9 (85.1 the dairies') / 16.4 | 66.9 / 95.0 |
| Cider from the orchards / short | 10.1 / 16.2 | 6.4 / 21.6 |
| Apple brandy short | 9.3 | 4.6 |
| Hides, bones piling (a year) | 6,860, 10,234 | 8,655, 9,095 |
| The beyond paid in / was paid | 112.7 / 112.6p (the credit 0-50p) | 52.7 / 52.6p |
| The takes (a day); what the room could not fit | tallow 10.3, honey 3.8, bells 2.4, hides 2.4, bones 0.9; 424p | honey, tallow 6, small pelts |
| The treasury's least; its 4-week means | 1,573p; 3,266 -> 4,126p | 1,970p; 4,778 -> 5,777p |
| Wage support (years 1, 2, 3) | 126, 221, 293p | 143, 242, 296p |
| Hunger | 6 days, 9 hours at most, 0.31 households short | 6 days, 9 hours, 0.31 |

Beside it, the same code before the bells went first and the basket's carriage went within its bound (`stable8-pre`):
posts never short, the Crown 1,383, 1,457, 1,358p, wage support 115, 199, 277p, the foundry 24% paid. And the takes,
imports and yields alone, without the farms' surplus (`stable8-takes`, stopped on day 824): 14 nights short in year 2
and 20 in year 3's first 94 days, the Crown's purse 591p and falling, wage support 245p in year 2.

**Verdict: better, not yet stable.** The estates are mended: every post paid every night for three years, their rents
~200p a day over their posts, the surplus spent back into the city; the estates' purses fall slowly from year 1's
2,419p as they spend down to their keep (2 nights of posts and the reserve), the Crown's with them (1,400p at the end,
least 909p). The foundry pays its crew from its bells. The valve nets (its book 149,469p in, 149,480p out). What is
left:

- **Wage support still climbs year on year** (126, 221, 293p; wharf7b's 143, 242, 296p): the smiths' (38p a day), the
  inns' (41p), the butchers' (27p), the glassworks' (18p). The farms' baskets, 318p a day smaller, bought the smiths' and
  the ironworks' goods: the ironworks now sends Timberland 3p a day, not 24p, and the mine's ore piles (4,917 a year).
- **The piles the credit can't reach**: the takes want ~424p a day more than the beyond's room (which is what the realm
  imports: butter, cider, eggs, ~113p a day), so the hides (6,860 a year) and bones (10,234) still pile at the butchers.
  The valve can only take as much as it lands; the real fix for these is a use or a smaller yield in the realm, not more
  exports.
- **Cider and apple brandy short** (the households' top shortages now; eggs 16 a day short, from 95).
- In year 1 the Wharf lands the country workshops' own goods (linen cloth, scarves, soap, pamphlets: ~70p a day paid
  beyond in the first 60 days) while their makers can't keep up; gone by year 2.

**Review of round 1** (before the merge into ridgemere2): `unfit` counted a holder's shortfall again at the next holder
of the same good (the cap a shrunk take left passed on to it, which the room then refused whole), so stable8's ~424p a
day of unfit overstates what the takes wanted past the room. It is now booked once a good: what its holders held over
their keep within its cap, less what was taken. Logging only; the takes themselves are as they were.

### stable8's 3-year run, on the merged build (ridgemere2 2510e81, 2026-11-04)

Run again in the main checkout after the merge, build-core rebuilt, the same flags (build 63's export, `--farms 0`, an
area every 28 days; 1,095 days, 2,376 s, 2.17 s a day). Report, patch and log: `artifacts/economy/r2/stable8-report.md`,
`stable8-report.md.patch.json`, `stable8.out` (the first run's logs kept beside it, not committed, as `stable8-r1.*`).
**Every day's economy line is the same as the first run's but `unfit`**, and the end digests match (people
d41e6b0216f355c0, society c962e2b0cb53c31d): the merge and the review's fix changed nothing in the run. `unfit`, now
booked once a good: 182, 308, 320p a day by year (the first run's year 3: ~424p). So the round-1 table above stands.

Year 3 against the stable definition:

| | year 3 | stable? |
|---|---|---|
| Money conserved every day | yes (64,584p at the end; minted 214,188 less sunk 149,604) | yes |
| Estates' posts | 824p of 824p, no night short in three years | yes |
| Estates' purses (year ends) | Crown 1,629, 1,557, 1,400p; Grayrock 45, 24, 30p; Timberland 449, 393, 396p; Greatly 67, 52, 39p; Fell 229, 163, 189p; all five 2,419, 2,189, 2,054p | **no**: the Crown and Greatly fall every year |
| The Crown's rents in | 682, 747, 689p a day (land 617p, of it surplus 211p; shops 72p, down from 100p) | falling in year 3 |
| Treasury | over its float all three years (least 1,573p in year 3; 4-week means 3,266 -> 4,126p) | yes |
| Wage support | 126, 221, 293p a day | **no**: still climbing |
| Hunger | 6 days, 9 hours at most, 0.31 households short | as wharf7b |
| Piles | hides +6,860 a year, bones +10,234, timber +4,857, iron ore +4,917 | **no** |
| Standing shortages | cider (16 a day short), apple brandy (9.3), eggs 16 | **no** for cider and brandy |
| The valve | 112.7p in, 112.6p out a day; the credit 0-50p | yes |
| The foundry | wages 100% paid from its bells (2.4 a day taken), 11.9p to Grayrock | yes |

**Verdict: not stable.** Four of the definition's tests fail: wage support climbs year on year, the Crown's and
Greatly's purses fall, hides, bones, timber and iron ore pile, and cider and apple brandy stay short.

Where the climbing wage support goes in year 3 (a day; year 1 in brackets): inns 40.7 (30.6), smiths 38.2 (14.7),
butchers 26.7 (5.7), glassworks 17.6 (3.1), preservers 16.8 (14.7), brickworks 11.8, papermill 11.2, sawmill 10.4,
charcoal store 10.1, cartographer 9.7. The report's till table gives the reasons: input costs for the inns, smiths,
butchers and preservers (takings 63, 31, 78, 80p against costs 46, 24, 62, 64p), low sales or too many hands for the
glassworks, sawmill, brickworks and bakeries. The smiths' share grew most since the farms' baskets were bounded (the
baskets bought from the smiths and the ironworks).

### Round 2 of stabilizing (branch `r2-stable8b`, 2026-11-04): the butchers' by-products sold, the stills' cider

The user: "Do what you need to in order to stabilize the economy. If we need more logging make more logging." No
treasury lever (toll, wage support, float), no move of the Wharf's credit or its injection, cheese left as it is, no DEV
write.

**The trace** (stable8's logs, 3 years; a day unless marked):

- **Wage support does not climb; what the treasury can pay of it does.** What the short tills could not pay their posts
  (`support_wanted`, logged since doc 71) was 312, 309 and 326p by year. The treasury pays it only from what it holds over
  its cap (treasuryCap 2,000p): it was over it on 107, 105 and 300 nights, so it paid 126, 221 and 293p and left 186, 87 and
  32p unpaid. The paid figure climbs toward the wanted one, which is about level (+4% in year 3).
- **Where the shortfall sits.** In year 3, 16 tills hold half of it (164p of 320p). They are: the two small inns of three
  (Ashgate's and Gullmore's: 23p and 28p of takings against 21p owed); Halford's glassworks (three hands, 9.5p of
  takings); the papermill, the cartographer, the charcoal store and the paintworks (two each, 5-10p of takings); the
  sawmill and the Gilbey brickworks (three each); Beechshaw's press (four); all five smithies (7.8-8.7p each: two posts at
  the 7p floor, 23-26p of takings, 17-20p of costs); and Bilgemarsh's preserver (no sales in year 3). These are crews too
  big for what their place sells. Every post is filled (614 of working age, 620 paying posts), so a hand stood down
  would have no post to go to. His wage would only move into the alms (10p a head).
- **The smiths' share grew because year 1's demand was a spend-down.** In year 1 the estates spent their split purses
  down (973p a day bought, then 321p and 196p) and the export basket bought 93p a day (16p by year 3). That year the
  smithies sold ~65p a day to the Wharf and ~41p to the estates (steel bars, rivets, mining picks). By year 3 they sold
  ~15p and ~10p, against the farms' ~95p.
- **The Crown's and Greatly's purses spend down to their keep; this is not a deficit.** The estates spend what they hold
  over their keep (estateKeep: the 200p reserve and two nights of posts, 1,848p for all five). All five averaged 2,191,
  2,123 and 1,937p in years 1-3, so by year 3 they sat at their keep plus a day's spare. In year 3 their rents were
  1,009p a day against 824p of posts and 196p bought: level. Greatly lives on the Crown's grants by design (28p of works
  rent against its 126p of posts).
- **The butchers' hides and bones had no buyer in the city.** A butcher buys a pig (~7.8p; 23.8 a day) and cuts it into
  four meat, a bone, two tallow and a hide. Its tallow went to the chandlers (a `suppliers` line). But the tannery drew its
  hides (2.6 a day) and the inns, the general stores, the jeweller and the tinker their bones (33.8 a day) from the farms.
  So the butchers' hides (+21 a day) and bones (+23 a day) piled: 18,835 and 24,304 at the end. Their carcasses' cost paid
  for by-products they never sold, and their shortfall rose from 13p to 30p.
- **Cider and apple brandy are one shortage.** The cider was short at the distillery (Saltley) and at Marshholm's still,
  12.5 and 11.5 a day by year 1's end. Both run cider into apple brandy, and the vintner buys its apple brandy from them,
  so it was short too.
- **The foundry's bars.** The smiths (swords), the jeweller (bracelets) and the glassworks (mirrors) use bronze and tin
  bars, but they drew them from the Wharf, so the foundry sold none of them in the city.
- **The works' piles** (timber, iron ore, stone, sand, oak bark) are yields over what was drawn.

**More logging** (the economy file, normal level; Tests/world_check.cpp): `short_by_trade` (what each trade's short tills
could not pay tonight, before the support), `rotted` (by good) and each estate's `keep` (estateKeep tonight, beside its
purse). `tools/econ_report.py` adds three things. The first is wage support by year: wanted, paid, unpaid and the wanted
by trade. The second is each estate's mean keep and its purse over it, by year. The third is what rotted.

**The fixes** (data and one rule):

- **The butchers' by-products go to the trades that use them** (city_loop.json `suppliers`). The tannery buys its hides,
  and the inns (their stew), the general stores, the jeweller and the tinker (their charms) buy their bones, from the
  butchers first. The foundry's bronze bars go to the smiths and the jeweller, and its tin bars to the glassworks.
- **What still piles of a perishable by-product rots** (the rule `shopRot`, 1; `Society::rotStock`, at the wages' hour).
  It covers a good a maker's crafts make and none of them uses, that is not food and has a `keeps` (the butchers' hides,
  6 days: "Rots if not tanned"). Each night the maker's stock of it loses a keeps-th of what is over its keep, the
  Wharf's keep for a shop (wharfKeepLeast, 28 days of its sales, its shelf). The tannery's own hides (its input) and the
  bones (no `keeps`) never rot. Goods only: no money moves. The Editor's, Atlas's and map_editor's rule lists have it.
- **Cider for the stills**: the Wharf's cider import's most from 20 to 40 a day, and the orchards' cider yields the
  patch's whole step (supply-bound in every season; about +30%).
- **Yields** (`--apply` of stable8's patch, half way): timber, oak bark, iron ore, stone and sand; then the farms' bones
  (s8b-y1b's patch, half way: the butchers now supply 24 of the 34 a day the city draws).
- Tests: `farms_tests` `aMakersByProducts` checks four things: the new supplier lines; a butcher's hides over its keep
  losing a sixth a night, booked, the money unmoved; the tannery's hides not rotting; the rule off. The quarry's and the
  pits' yields follow the patch. ctest 79 of 79; the Editor's lib tests 68 of 68 (`content.test.mjs`' Greyfen failure as
  on ridgemere2).

**A first try, not kept** (`s8b-y1`, 1 year): the Wharf's apple brandy and brandy imports on (10 and 3 a day). The
vintner's apple brandy shortage fell from 10 to 0.7 a day, and the beyond's room grew enough that 18 hides a day were
taken from the butchers. But the households' comforts then went beyond the realm for it. The Crown's workshops' takings
fell from 89p to 68p a day in year 1 (pamphlets, soap, scarves), its rents from 682p to 644p, and the wanted support in
the year's last quarter was 27p a day higher. So cider for the realm's own stills replaced it.

**The 60-day smoke** (the first 60 days of the year's run below; deterministic, so the same as a 60-day run). Money was
conserved every day (51,201p to 59,353p; stable8's 59,341p). The estates' posts were paid every night. Nobody was hungry
(1 hour at most). The treasury's least was 1,172p. Wage support was 227p a day (wanted 243p). The beyond paid in 189p a
day and was paid 189p.

**The year's run, stable8b** (build 63's export, wharf7b's flags: `--farms 0`, an area every 28 days; 365 days, 907 s;
`stable8b-y1-report.md`, its patch and `stable8b-y1.out`, renamed when the 3-year run below took the name; the logs
are kept, not committed). Against stable8's year 1:

| | stable8b, year 1 | stable8, year 1 |
|---|---|---|
| Money conserved every day; the realm's money at the end | yes; 64,585p | yes; 64,570p |
| Estates' posts | 824p of 824p, no night short | the same |
| Estates' purses at the year's end (the Crown, Timberland, Greatly, Fell, Grayrock) | 1,542, 441, 64, 258, 50p | 1,629, 449, 67, 229, 45p |
| The Crown's rents in | 663p (land 579p, shops 85p) | 682p (593p, 89p) |
| Wage support wanted / paid / unpaid | 309 / 116 / 193p | 312 / 126 / 186p |
| The butchers' support | 4.4p | 5.7p |
| Hides, bones piling (a year) | none; hides held 265 at the end, 6.1 a day rotted | +3,380, +9,692 |
| Cider short (Marshholm's still, the distillery) | 2.5, 1.5 a day | 8.9, 9.1 |
| Apple brandy, brandy short (the vintner) | 11.3, 3.7 | 9.5, 3.5 |
| The beyond paid in / was paid | 188.9 / 188.9p | 176.2 / 176.2p |
| The export basket; the takes' unfit | 44p; 134p | 32p; 182p |
| The foundry's wages paid; to its House | 100%; 37p | 100%; 37p |
| The treasury's mean; least | 1,610p; 210p (day 230, as stable8's least) | 1,738p; 385p |
| Hunger (the most hungry hours in a day; households short) | 3; 0.66 | 10; 0.57 |

**Verdict: the piles and the stills' cider are mended, and wage support is not climbing.** The butchers' hides and bones
now sell or rot, so no pile grows. The stills' cider shortage fell from ~18 to ~4 a day. What the short tills want is
level, and what the treasury pays of it rises only toward that, as its room grows. Wage support itself is unchanged
(~310p a day wanted). The treasury ran ~130p lower: the till-to-till by-products pass no gate toll. Not measured: years 2
and 3, which a 3-year run would show.

**Left, for the user:**

- **The structural shortfall (~310p a day) is the over-staffed tills listed above.** With every post filled, no rule of
  the loop can move their hands to work that sells. Two ways would end it: fewer posts at those tills (world data: the
  two small inns, Halford's glassworks, the papermill, the cartographer, the charcoal store, the paintworks, one hand at
  each smithy), or more of the realm's money reaching them (the estates' basket or the works' tools, crafts.json
  `tools`, which no works buys yet). Either is a design choice; neither was made here.
- **Imports pay for exports.** The beyond buys the realm's hides, bones and export goods only as far as the realm buys
  from it (the credit). Each import moves comfort money out of the realm and its room back to the takes, as the first
  try showed.
- **Apple brandy is still short at the vintner** (~11 a day). The stills now have their cider, so it is their crews' hours
  or the vintner's draw; not traced.

### stable8b's 3-year run (ridgemere2 28df182, 2026-11-05)

Round 2's fixes merged (the butchers' by-products, `shopRot`, the stills' cider, the yields), build-core rebuilt, build
63's export, wharf7b's flags (`--farms 0`, an area every 28 days); 1,095 days, 2,266 s, 1.9-2.2 s a day. Report, patch
and log: `artifacts/economy/r2/stable8b-report.md`, `stable8b-report.md.patch.json`, `stable8b.out` (the logs kept, not
committed). Its first 365 days are the year's run above. A day unless marked; year 3, years 1-3 where given:

| | stable8b | stable8 | wharf7b | producers7 |
|---|---|---|---|---|
| Money conserved every day; at the end | yes; 64,585p (minted 227,163 less sunk 162,602) | yes; 64,584p | yes; 64,588p | yes; 64,595p |
| Estates' posts; nights short | 824p of 824p; none in three years | the same | 788p; 107 | 803p; 40 |
| The Crown's purse (year ends; year 3's least) | 1,542, 1,599, 1,340p (867p) | 1,629, 1,557, 1,400p (909p) | 1,002, 976, 625p | 946, 1,005, 714p |
| The Crown's purse by quarter, year 3; its keep | 1,416, 1,326, 1,386, 1,367p; 1,345p | 1,452, 1,379, 1,434, 1,417p | 566, 550, 572, 751p | 670, 595, 786, 952p |
| Grayrock, Timberland, Greatly, Fell (year ends) | 50/27/29, 441/409/396, 64/60/46, 258/215/197p | 45/24/30, 449/393/396, 67/52/39, 229/163/189p | -, -, -, - | -, -, -, - |
| All five estates (year ends) | 2,355, 2,310, 2,008p | 2,419, 2,189, 2,054p | 1,610, 1,469, 1,094p | 1,563, 1,451, 1,144p |
| The Crown's rents in (years 1-3) | 663, 727, 667p (year 3: land 601p, of it surplus 201p; shops 66p) | 682, 747, 690p | 464p | 503p |
| The Crown's grants out; bought | 517p; 76p | 516p; 96p | 502p; 0p | 529p; 0p |
| The foundry's wages paid; to its House | 99%; 9.9p (bells 2.3 a day taken) | 100%; 11.9p | 3.4%; 0p | 0.2%; 0p |
| The takes (pieces a day); unfit | tallow 10.3, honey 4.7, hides 3.2, bells 2.3, bones 1.1; 314p | tallow 10.3, honey 3.8, bells 2.4, hides 2.4, bones 0.9; 320p | - | - |
| Eggs to households (the dairies'); short | 107.3 (85.1); 15.6 | 106.9 (85.1); 16.4 | 66.9 (66.9); 95.0 | 49.5; 114.1 |
| Cider from the orchards; short | 13.4; **1.2** | 10.1; 16.2 | 6.4; 21.6 | 3.5; 9.7 |
| Apple brandy to households; short | 11.0; 9.0 (the vintner) | 9.5; 9.3 | 5.5; 4.6 | - |
| Hides rotted (years 1-3) | 6.1, 16.8, 18.0 a day | none (piled) | none (piled) | - |
| The treasury's least; 4-week means | 1,290p; 2,372 -> 3,375p | 1,573p; 3,266 -> 4,126p | 1,970p; 4,778 -> 5,777p | 1,903p; 4,108 -> 4,633p |
| Wage support wanted / paid / unpaid (years 1-3) | 309/116/193, 282/176/106, 309/270/39p | 312/126/186, 309/221/87, 326/293/32p | 303/296/7p (year 3) | 295p paid |
| Hunger: days, the most hours, households short | 6, 9, **0.45** | 6, 9, 0.31 | 6, 9, 0.31 | 3, 8, 0.28 |
| The beyond paid in / was paid (years 1-3) | 188.9/188.9, 136.3/136.3, 119.8/119.8p | 176.2, 120.7, 112.7p | 112.7, 56.3, 52.7p | - |
| The Wharf's fees; export basket; the credit | 12.5p; 8.8p; 0-441p, 10p at the end | 11.5p; 7.8p | 3.2p; 25.4p | - |

The beyond's year 3 by good (paid it a day): butter 55.7p, cider 33.4p (17.4 a day to the distillery and the country
still), eggs 22.6p, tin ore 3.8p, copper ore 2.5p; it paid for the bells 58p a day over the run, hides 31p, honey 13p.

**Piles in year 3** (day 730 to 1,094, all at the farms and works; the butchers' hides held level at ~300): bones +810
at each of the eight meat farms (~6,500 in all), hemp +5,780 at the grain farms, iron ore +5,190 at the mine, wool +4,210
at the sheep runs, timber +3,720 at the logging camps, limestone +2,735 at the lime quarry, honey +1,840 at the hives.
The report's patch lowers each (bones at the dairies 4.4-5.3 to 2.0-2.6 a day, hemp to ~28, iron ore to ~46, wool to
~21, timber to ~12, limestone to ~19, honey to ~23) and raises the mine's tin ore (0.4 to ~1.0: the foundry 4 a day
short, its 2 a day from the beyond).

**Against the stable definition, year 3:**

| test | result | stable? |
|---|---|---|
| Money conserved every day | yes | yes |
| Estates' posts paid every night | 824p of 824p, no night short in three years | yes |
| No estate purse trending down | the Crown's fell 1,599 -> 1,340p at year end and ~1,500 -> ~1,370p by quarter, to its keep (1,345p, 29p over it); level within year 3. Grayrock's and Greatly's (grant-fed, ~30-45p) drift down 5-10p a year; Timberland's and Fell's level | **no, narrowly**: the Crown is at its keep with no margin, its rents down 60p from year 2 |
| The treasury over its float | least 1,290p, rising 2,372 -> 3,375p | yes |
| Wage support not climbing | wanted 309, 282, 309p (level); paid 116, 176, 270p (rising toward it as the treasury's room grows) | **no** for what is paid; the wanted is level |
| Hunger as low as wharf7b | the same days and hours (day 777's 9 hours); households short 0.45 against 0.31 (45 days with any, against 27) | **no, narrowly** |
| No runaway pile, no standing shortage a producer should cover | the farm piles above; apple brandy 9 a day short at the vintner, the foundry's tin ore 4 | **no** |
| The valve nets | 119.8p in, 119.8p out a day; the credit back to 10p | yes |

**Verdict: not stable.** Round 2 did what it aimed at: the butchers' hides and bones no longer pile (they sell, are taken
or rot), the stills' cider shortage is gone (1.2 a day from 16), money is conserved, every estate post was paid every
night for three years, the treasury rises and the valve nets. What fails:

1. **Wage support**: what the short tills want is level at ~300p, but the treasury pays more of it each year (270p in
   year 3, its last four weeks 337p). The inns (44.8p), smithies (41.4p), glassworks (19.1p), preservers (17.0p),
   butchers (14.8p), brickworks (13.6p), papermill, sawmill, charcoal store and cartographer (11-12p each) want it.
   These are crews too big for their sales, or input costs too near their takings.
2. **The Crown's purse is at its keep**, with no margin: its rents fell from 727p to 667p (land 632p to 601p, its
   workshops 95p to 66p as the households' comforts fell from 426p to 408p a day). The same happened in stable8.
3. **The farms' and works' piles** above: the yields are over what the realm uses.
4. **Apple brandy** is 9 a day short at the vintner. It buys ~8 a day from Marshholm's still and the distillery, which
   are bound by their cider (17 a day of it from the beyond) and their hours. The foundry is 4 a day short of tin ore.
5. **More households short** (0.45 against 0.31) while the treasury leaves 39p a day of support unpaid (wharf7b's 7p,
   stable8's 32p). The treasury's year-3 mean is 2,451p (stable8 2,714p, wharf7b 3,999p) and it pays support only over
   its cap; the toll is 20p a day lower than stable8's (the by-products pass no gate). Which households are short was
   not traced.
6. **Hides rot** at 18 a day by year 3 (the tannery draws ~3 and the takes ~3): the butchers buy carcasses whose
   hides waste. Cheese stays short at the draws, as decided.

### Round 3 of stabilizing (branch `r2-stable8c`, 2026-11-05): the crews' hours, the stills, the farms' piles

The user: "Do what you need to in order to stabilize the economy. If we need more logging make more logging." No
treasury lever (toll, wage support, float), no move of the Wharf's credit or its injection, cheese left as it is, no DEV
write, no new NPC (the still's two new posts are staffed from the Country's unplaced people, as every workshop's are).

**More logging** (the full level only, so it stays off unless asked: `--economy-level full`; Tests/world_check.cpp):

- `work`: each maker's crew-hours a day, in tenths: what its crew put in (restockFromStore's work bank, a post an open
  hour), what its crafts used, and what the bank could not hold (its cap, workBankMost: hours no craft wanted). A till that
  uses all its hours is bound by its crew; one that loses most of them, by its sales. (`Society::LoopDay::work`.)
- `hh_short`: each household short of food at the shops' last hour: its first member, class, members, purse, earners,
  and up to three of its earners' payers.
- `tools/econ_report.py` adds, under the short tills, their trades' crew-hours (put in, used, lost) and the makers that
  used 95% or more of theirs; and under welfare, the short households (how many have no earner, how many live alone,
  their purse, their earners' payers).
- The workshops' takings by buyer class were already in the full log (`trade` rows: the seller's till and the buyer's
  group), so no field was added for them.

The logging changes nothing in the run: a 60-day smoke with it (`s8c-base60`, the old data) gave every economy line the
same as stable8b's first 60 days.

**The trace** (stable8b's logs, the smoke, and the year's run below; a day):

- **The short tills are idle, not busy.** The trades that want the wage support use part of their crews' hours (the
  year's run): inns 61%, smithies 38%, preservers 30%, general stores 34%, fishmongers 19%, the sawmill 35%, the
  glassworks 35%. In the year's second half the two small inns (Ashgate's, Gullmore's) used 18-22% of their three posts'
  hours, the sawmill 28%, Halford's glassworks 21%, the papermill 23%, the cartographer 23%, the smithies 25-27%: about
  1.5-2.5 posts idle at each. Their shortfall is posts with no work. The butchers are the exception (88% used): theirs is
  margin. Every post is filled (620 of 620 paying posts held), so a hand stood down has no post to go to.
- **Eight makers are bound by their crews**: Larkfield's press, the Coldwater still, both chandleries, the distillery, the
  pottery, the weaver and the tailor use 95-100% of their hours. **The stills are one of them: apple brandy is short for
  hands, not cider.** The 60-day smoke on the old data: the distillery used 26.4 of its 27.2 hours a day, the Coldwater
  still 20.7 of 21.4. Each apple brandy is 2 hours, a brandy 9.
- **The apples piled while the realm imported cider.** The orchards' apples grew ~9 a day an orchard over what sold
  (stable8b, years 2-3), while the Wharf landed ~22-25 cider a day (~45-51p) for the stills and the inns. The orchards pressed
  too little cider.
- **The short households are lone wolves with no earner**, not the short tills' crews. In the year's run 75% of the
  short households' rows had no earner and 74% were one wolf, with a mean purse of 15p. For example, the smoke's
  rmx_orla_wetridge is 15, "at lessons", and lives alone on the alms (10p a head). This is the alms' line and who lives
  alone, which is world data. The tills are not the cause.
- **The comforts are one pot.** Filling the vintner's apple brandy and brandy draws comfort money from the Crown's
  workshops (the wool scarves, the soap, the pamphlets), as `s8b-y1`'s imports did. More drink means House Fell's
  still and the vintner take more, and the Crown's shops take less.
- **Honey: stable8b's patch changes almost nothing.** Its "balanced" honey (~23 a day a hive, before the area's 88% scale) is
  the yield now. Honey still piles ~1.7 a day a hive. Salt and clay pile 5-10% over what sells, too: the patch's +5%
  slack, kept forever by goods that never rot.
- **Hides**: the leather the realm uses is ~5 a day (the farms' baskets), so the tannery draws ~3 hides a day of the
  butchers' ~21. A pig gives a hide, so the rest rot.

**The fixes** (data):

- **The Coldwater still has three still hands** (workshops.json; was one): +2 posts, staffed from the Country's pool.
- **The orchards press their spare apples into cider, and the vineyards their spare grapes into wine.** Each orchard's
  cider yield rises by 0.45 of its stable8b apple surplus by season (2 apples a cider, 90% of the surplus): +3.4 to +4.8
  a day each, ~20 a day in all. Each vineyard's wine rises the same way (+0.7 to +1.8). Their apple yields stay as they were: a first try
  that also cut the apples (`s8c-try1`) left the breweries and inns ~40 apples a day short in spring and summer, because
  they now press the cider the Wharf used to land.
- **Yields** (stable8b's patch): the whole step for bones, wool and hemp (hemp then set to `s8c-try3`'s whole step: the
  full cut left the ropewalk and the chandleries ~7 a day short); half the step for timber, iron ore, limestone,
  honey (about 1% lower) and the dairies' and pig farms' own hides (halved; they spoiled at the farms, ~1 a day each);
  the mine's tin ore the whole step (0.4 to ~1.0). A try at the whole step for timber, iron ore and limestone (`s8c-try2`) left the
  ironworks without charcoal and ore in winter (iron bars ~20,000 draws short), so it went back to half.
- Tests: `econ_fix_tests` checks the work bank's logged hours against the maps made. `sim_fast_tests` checks the full
  tables `work` and `hh_short`, and `test_econ_report.py` the report's new lines. The workshops' and the pool's post
  counts go from 37 to 39 (workshops_tests, farm_people_tests). `farms_tests`' ore check is now two ore a bar for the ~20
  bars the city buys, not the Country's old ~27. ctest 79 of 79. test_econ_report, test_country_industry,
  test_farms_summary and test_realm2 pass. test_ridgemere2's two failures (R35, the food trades' staffing) fail the
  same way on ridgemere2.

**The year's run, stable8c-y1** (build 63's export, wharf7b's flags: `--farms 0`, an area every 28 days; 365 days, 692 s;
`stable8c-y1-report.md`, its patch, `stable8c-y1.out`; its first 60 days are the 60-day smoke, deterministic). Against
stable8b's year 1:

| | stable8c, year 1 | stable8b, year 1 |
|---|---|---|
| Money conserved every day; the realm's money at the end | yes; 64,561p | yes; 64,585p |
| Estates' posts | 824p of 824p, no night short | the same |
| Purses at the year's end (the Crown, Timberland, Greatly, Fell, Grayrock) | 1,450, 429, 61, 393, 46p | 1,542, 441, 64, 258, 50p |
| The Crown's rents in (land, shops) | 647p (575p, 72p) | 663p (579p, 85p) |
| House Fell's rents in (land, shops) | 178p (164p, 14p) | 122p (116p, 5p) |
| Wage support wanted / paid / unpaid | 323 / 120 / 203p | 309 / 116 / 193p |
| Apple brandy, brandy short (the vintner) | 6.0, 2.2 a day | 11.3, 3.7 |
| The stills' cider short; cider landed from beyond | 0.2 a day; 5.5 a day (11p) | 4.0; 24.6 (51p) |
| The foundry's tin ore short | 1.8 a day | 2.1 |
| Piles over the year's second half (day 182 to 364): bones, hemp, wool, iron ore, timber, limestone | +187, +476, +649, +85, -35, +23 | +2,103, +1,909, +1,647, +789, +763, +285 |
| ... apples, honey, salt, clay | +5,149, +1,032, +2,112, +1,606 | +6,327, +849, +2,130, +1,786 |
| The beyond paid in / was paid; the takes' unfit | 147 / 147p; 189p | 189 / 189p; 134p |
| Hides rotted; taken beyond | 9.7; 7.6 a day | 6.1; 11.4 |
| The treasury's mean; at the end | 1,621p; 1,733p | 1,610p; 1,811p |
| Hunger: days, the most hours; households short | 6, 10 (day 322); 0.9 | 8, 3; 0.7 |

Day 322 is the day after a festival's buying-ahead in both runs (stable8b had 3 hungry hours on it, this run 10), spread
over ~70 wolves' hours. Its short households were two lone wolves on the alms and a smith's household.

**Verdict: the piles and the drink are mended; the wage support and the Crown's margin are not, and these changes do
not help them.**

- **Mended:** the farms' and works' piles that stable8b's year 3 named (bones, hemp, wool, iron ore, timber, limestone)
  stopped growing. The apple surplus is now pressed into cider in the realm: the stills' cider shortage is gone and the
  realm pays the beyond 40p a day less for cider. The vintner's apple brandy and brandy shortage fell from 15 a day
  to 8.
- **What it moved:** the drink is comfort money. House Fell's rents rose by 56p a day (its orchards' cider, its still)
  and the Crown's fell by 16p (its workshops' wool scarves and soap 13p). The Crown's purse ended the year 92p lower.
  It still ran 219p a night over its keep on average, but that is margin taken from a purse that stable8b's year 3 left
  29p over its keep. With less cider imported, the beyond has less room to take the realm's exports: hides taken fell
  from 11.4 to 7.6 a day, and 9.7 a day rot.
- **Not mended:** wage support wanted is ~310-320p a day, as before. The crew-hours show why: idle posts at tills whose
  sales can't use them. No loop rule can move a hand to work that sells while every post is filled. That is world data
  (DEV's posts), and the user's choice.

**Left, for the user:**

1. **Over-staffed posts** (the main cause of wage support). The tills below use a fifth to a third of their crews' hours:
   Ashgate's and Gullmore's inns (3 posts each, ~2.4 idle), the sawmill (3, ~2.2), Halford's glassworks (3, ~2.4),
   Saltstack's preserver (3, ~2.2), the papermill and the cartographer (2, ~1.5 each), the five smithies (2, ~1.5 each),
   Gilbey's brickworks (3, ~2.0), Dripworth's fishmonger (2, ~1.7). Standing down a hand moves his wage from the support
   to the alms (10p a head) unless a post with work takes him. The crew-bound makers above (the presses, the
   chandleries, the stills, the weaver, the tailor, the pottery) are where hands would earn. Moving posts from the first
   list to the second is a DEV world edit. Not done here.
2. **The Crown's margin.** Its workshops lose comfort money whenever another comfort is supplied better (the still now,
   the imports in `s8b-y1`). If the Crown is to keep a margin, its estate's income needs a source that is not comfort
   money, or the still's two hands go back (one line in workshops.json, which brings the vintner's shortage back).
3. **Slack piles of goods that never rot:** apples (~28 a day in all), salt, clay, honey. Each is 5-10% over what sells,
   and the patch's +5% keeps it there for good. The Wharf can only take what its credit allows, and less cider imported
   means less credit. A patch at 0% slack for these goods, or a `keeps` (spoil) for apples, would end it.
4. **The lone wolves on the alms** are most of the short households: 10p a head buys about a day's food, but not
   `shortDays` of it. Who lives alone, and the alms' line, are the user's.
5. **Not run:** a 3-year run on this branch (the year above is the measure).

### stable8c's 3-year run (ridgemere2 01c3369, 2026-11-05)

Round 3's fixes merged (the work and hh_short tables, the Coldwater still's three hands, the orchards' and vineyards'
spare fruit pressed, the yields), build-core rebuilt after the merge, build 63's export, wharf7b's flags (`--farms 0`, an
area every 28 days); 1,095 days, 2,246 s, 1.3-2.2 s a day. Report, patch and log: `artifacts/economy/r2/stable8c-report.md`,
`stable8c-report.md.patch.json`, `stable8c.out` (the logs kept, not committed). Its first 365 days are `stable8c-y1`
again, number for number (deterministic). A day unless marked; year 3, years 1-3 where given:

| | stable8c | stable8b | wharf7b | producers7 |
|---|---|---|---|---|
| Money conserved every day; at the end | yes; 64,577p | yes; 64,585p | yes; 64,588p | yes; 64,595p |
| Estates' posts; nights short | 824p of 824p; none in three years | the same | 788p; 107 | 803p; 0 |
| The Crown's purse (year ends; year 3's least) | 1,450, 1,443, 1,281p (901p) | 1,542, 1,599, 1,340p (867p) | 1,002, 976, 625p | 946, 1,005, 714p |
| The Crown's purse by quarter, year 3; its keep | 1,405, 1,328, 1,371, 1,358p; 1,342p | 1,416, 1,326, 1,386, 1,367p; 1,345p | 566, 550, 572, 751p | 670, 595, 786, 952p |
| Grayrock, Timberland, Greatly, Fell (year ends) | 46/21/24, 429/431/297, 61/37/45, 393/378/392p | 50/27/29, 441/409/396, 64/60/46, 258/215/197p | -, -, -, - | -, -, -, - |
| All five estates (year ends) | 2,379, 2,310, 2,039p | 2,355, 2,310, 2,008p | 1,610, 1,469, 1,094p | 1,563, 1,451, 1,144p |
| The Crown's rents in (years 1-3); its grants out | 647, 704, 658p (year 3: land 601p, of it surplus 201p; shops 57p); 293, 507, 521p | 663, 727, 667p; 517p (year 3) | 464p | 503p |
| House Fell's rents in (years 1-3) | 178, 177, 154p | 122, 110, 99p | 47p | 34p |
| The foundry's takings (years 1-3); to its House | 91.6, 25.8, 14.7p; 34.3, 9.7, 3.6p (wages 95% paid, 1.3p support) | year 3: 99%; 9.9p | 3.4%; 0p | 0.2%; 0p |
| The takes (pieces a day); unfit | tallow 9.4, honey 2.4, bells 1.8, hides 1.3, bones 0.7; 349p | tallow 10.3, honey 4.7, hides 3.2, bells 2.3, bones 1.1; 314p | - | - |
| Eggs to households; short | 107.8; 15.2 | 107.3; 15.6 | 66.9; 95.0 | 49.5; 114.1 |
| Cider from the orchards; landed from beyond; the stills short | 26.9; 0; 0 | 13.4; 18.3; 1.2 | 6.4; -; 21.6 | 3.5; -; 9.7 |
| Apple brandy to households; short | 16.7; **0.1** | 11.0; 9.0 | 5.5; 4.6 | - |
| Hides rotted (years 1-3) | 9.7, 19.1, 19.8 a day | 6.1, 16.8, 18.0 | none (piled) | - |
| The treasury's least; 4-week means | 1,364p; 2,828 -> 3,981p (mean 2,528p) | 1,290p; 2,405 -> 3,407p | 1,970p; 4,811 -> 5,810p | 1,903p; 4,142 -> 4,669p |
| Wage support wanted / paid / unpaid (years 1-3) | 323/120/203, 278/191/87, 305/268/37p (last 4 weeks paid 339p) | 309/116/193, 282/176/106, 309/270/39p | 303/296/7p (year 3) | 311/295/16p |
| Hunger: days, the most hours, households short | 6, 7, **0.34** (35 days with any) | 6, 9, 0.45 (45) | 6, 9, 0.31 (27) | 3, 8, 0.28 (34) |
| The beyond paid in / was paid (years 1-3) | 147/147, 92.5/92.5, 83.0/83.0p | 188.9, 136.3, 119.8p | 112.7, 56.3, 52.7p | - |
| The Wharf's fees; export basket; the credit | 7.0p; 7.2p; 0-38p, 18p at the end | 12.5p; 8.8p; 10p | 3.2p; 25.4p | - |

The beyond's year 3 by good (paid it a day): butter 55.4p, eggs 23.1p, tin ore 2.9p, copper ore 2.2p; no cider, no
firewood or charcoal after year 1. It paid for the bells 45p a day, tallow 9p, honey 8p, hides 7p.

**Piles in year 3** (day 730 to 1,094, at the farms and works): stable8b's are mostly mended. Apples (+16,231 in
stable8b), bones (+6,460) and limestone (+2,735) no longer pile; hemp (+5,783 -> +3,166), iron ore (+5,187 -> +3,154),
wool (+4,208 -> +1,829) and timber (+3,720 -> +2,115) pile less. Still piling: firewood +7,095, clay +5,958, charcoal
+4,055, salt +3,955, flour +3,708, peat +3,281, flax +2,833, honey +2,547 (each hive ~2,250 at the end, up from 142),
stone +2,248, tallow +1,636 (the dairies'). Firewood, clay, charcoal, salt, flour and peat piled the same in stable8b.

**Shortages in year 3** (pieces a day, cheese aside: 46.4, as decided): milk 16.5 and eggs 15.2 (the dairies, supply-
bound), meal 10.0 (new: stable8b's 2.0; the towns' food shops, the dairies and pig farms sell all they grow), pig 2.3,
resin 2.1, butter 1.7, tin ore 1.4 (the foundry, from 4.0). The vintner's apple brandy and brandy shortage is gone (0.1).

**Against the stable definition, year 3:**

| test | result | stable? |
|---|---|---|
| Money conserved every day | yes | yes |
| Estates' posts paid every night | 824p of 824p, no night short in three years | yes |
| No estate purse trending down | the Crown's 1,450 -> 1,443 -> 1,281p at year ends; ~1,457 -> ~1,366p by quarter, at its keep (1,342p, 23p over it); its rents 704 -> 658p and its grants out 507 -> 521p. Timberland's 431 -> 297p at year end, ~306 -> ~256p by quarter (its works' rents 168 -> 139p); Fell's level at ~390p; Grayrock's and Greatly's grant-fed and level (~25, ~40p) | **no**: the Crown is at its keep with no margin, falling as in stable8b; Timberland falls a year |
| The treasury over its float | least 1,364p, rising 2,828 -> 3,981p | yes |
| Wage support not climbing | wanted 323, 278, 305p (level); paid 120, 191, 268p, the last four weeks 339p | **no** for what is paid |
| Hunger as low as wharf7b | the same days, fewer hours (7 against 9); households short 0.34 against 0.31 (35 days with any against 27) | **no, narrowly** (better than stable8b's 0.45) |
| No runaway pile, no standing shortage a producer should cover | the slack piles above (honey, clay, salt, the fuels, flour); meal 10 a day short; hides rot 20 a day | **no** |
| The valve nets | 83.0p in, 83.0p out a day; the credit 18p at the end | yes |

**Verdict: not stable.** Round 3 mended what it aimed at: the drink (apple brandy short 0.1 a day from 9; the stills'
cider short none; the realm landed no cider from beyond in year 3, 36p a day it no longer pays), House Fell (its purse
level at ~390p, its rents 154p from 99p), and the farm piles stable8b named (apples, bones, limestone gone; hemp, iron
ore, wool, timber about halved). Money is conserved, every estate post was paid every night for three years, the
treasury rises and the valve nets. What fails:

1. **Wage support** paid rises each year (120, 191, 268p; the last four weeks 339p) while the wanted is level at
   ~305p: the same idle posts as stable8b. The trades (year 3): inns 42.7p, smithies 40.6p, glassworks 18.4p,
   preservers 17.4p, butchers 14.5p, the country press 13.9p, brickworks 12.1p, papermill 11.6p, sawmill 11.4p. Their
   crew-hours over the run: glassworks 23% used, preservers, smithies and general stores 29%, the sawmill 30%,
   brickworks 38%, inns 63%; the butchers 93% (margin, not idle hands). Eight makers are bound by their crews
   (Larkfield's press, Marshholm's still, both chandleries, the pottery, the weaver, the tailor, Kelpstack's brewery).
2. **The Crown's purse** is at its keep and falls (1,281p at the end of year 3, 49p under its keep that night). Its
   grants out (521p a day) are what Grayrock (476p of posts on 35p of rents) and Greatly (126p on 39p) lack; its own
   rents fell 46p from year 2 (land 629 -> 601p, shops 75 -> 57p as the comforts move to the drink).
3. **Timberland's** purse falls a year (its works' rents 168 -> 139p as the fuels pile at its works).
4. **The foundry is fading**: its takings 91.6 -> 25.8 -> 14.7p a day, to its House 34.3 -> 9.7 -> 3.6p; its bars and
   bells went to the Wharf in year 1 (~30p a day) and not after (the beyond's credit). It now pays its wages from its
   float (203 -> 104p) and 1.3p of support.
5. **Hides rot** at 20 a day; the takes take 1.3 a day because the room is short (349p a day of takes unfit, from
   314p): the beyond only buys what it is paid for imports, and the imports fell from 147p to 83p a day.
6. **Slack piles** (goods that never rot, kept 5% over by the patch): honey, clay, salt, charcoal, firewood, peat, flour,
   flax, stone.
7. **Meal** is 10 a day short at the towns' food shops (from 2.0), the dairies and pig farms out each day.
8. **Households short** 0.34 a day against wharf7b's 0.31: 46 households over the run, 68% with no earner, 63% alone,
   a mean purse of 17p (lone wolves on the alms, as round 3 found).

**Next, without the levers** (none tried here):

- Move posts from the idle tills (item 1's first list) to the crew-bound makers: a DEV world edit of the posts, the
  user's call (round 3, left item 1).
- Size the sea trade's and Greatly's posts to what their estates earn, or give Grayrock income from the takes it
  ships, so the Crown's grants shrink; that is the Crown's margin.
- The foundry: one hand fewer, or a city buyer for its bars (the jewelers and smithies), now the beyond takes none.
- The hides: a craft that uses them (parchment for the papermill or the cartographer, glue), or fewer hides a carcass
  in the butchers' by-products, since the realm's leather wants ~5 a day.
- The patch at 0% slack, the whole step, for the goods that never rot (honey, clay, salt, charcoal, peat, stone, flour,
  flax), and meal's (and eggs', milk's) whole step up at the dairies and pig farms.

### Round 4 of stabilizing (branch `r2-stable9`, 2026-11-05): posts moved, alms for those with no earner, the slack piles

The user, on stable8c: alms for the households with no earner and those living alone, and the posts moved ("you can go
ahead and move those posts"); a few rounds, real fixes, not levers. No treasury lever (toll, wage support, float), no
move of the Wharf's credit or its injection, cheese left as it is. **DEV written** (the posts, below; backed up first); no
NPC made (the villages' new posts and the still's are filled from the pool, as every workshop's are).

**The trace** (stable8c's logs; a day, year 3 unless marked):

- **The wage support is idle posts, as round 3 found, but there is little work to move them to.** Of the ~267p of
  support paid in year 3, the largest are the two small inns (Gullmore's and Ashgate's, 3 posts each, 18% and 21% of
  their hours used, ~14.5p each), Oakhaven's press (4 posts, 21%, 11.9p), Halford's glassworks (3, 12%, 11.6p), the Rag
  Mill (2, 5%, 10.7p), the sawmill, the charcoal store, the cartographer, the tileworks and the five smithies (2 posts,
  20-28%, 7-8p each). A post there is ~7p a day (the wage floor). The makers bound by their crews (95% of their hours or
  more) are few, and most of them have no unmet demand: the chandleries' candles (stock never out, price 1.00), the
  pottery's pots and Kelpstack's cider likewise. The goods at the price band's top in year 3, the sign of demand unmet:
  the tailor's and the weaver's linen neckerchiefs (1.25), the weaver's scarves and straw hats (1.24), the still's
  brandy and cider (1.23-1.24), and the villages' food: Coldwater's and Larkfield's bakehouses' bread and honey cakes
  (1.24-1.27) and Southmeadow's and Fenshore's alehouses' porridge (1.26-1.27), each a one-post shop using 90-100% of
  its hours, its keeper's profit 25-36p a day at the bakehouses.
- **The comforts are one pot.** A first smoke with the city's hands moved to the makers (below) raised the country
  workshops' support wanted (the Crown's linen and wool looms, its soapworks: 4 -> 10-12p each in the first spring) as
  the city's neckerchiefs, scarves and soap took their sales: more of one comfort is less of another.
- **The Crown's own workshops have idle posts too.** Coldwater's soapworks used 57% of its 8 posts' hours (45 of 79 a
  day), Coldwater's wool looms 61% of 7 (46 of 76), Larkfield's linen looms 68% of 6 (44 of 65). Their wages are the
  Crown's margin: what they hold over their float and four days' wages goes to its estate.
- **The foundry has no buyer.** The smaller iron trades buy of whichever seller holds most of a good (`buyFromCity`),
  so the ironworks' shelves took every order (27 bars a day) and the foundry sold ~0.2 a day once the Wharf stopped
  taking its bars; its takings 92, 26, 15p a day over the three years, its inputs (21p) over them.
- **The short households hold a day's alms and nothing over.** 154 rows over the run, 63% with a purse of exactly 10p
  a head (the alms' line), no earner, most alone (68%, 63%); nobody went hungry for long (6 days in three years, 7 hours
  at most), but they end each day's market under a day's food.
- **Slack piles** (stable8c's patch at no slack, the goods that never spoil): flour ~10% over what is taken, hemp ~10%,
  salt ~14%, clay ~18%, charcoal ~21% (the Upper hearths) and ~20% (the Greenholt's), honey ~11%, stone ~16%, peat ~7%,
  firewood ~5%, iron ore ~10%, flax ~4%.
- **Eggs, milk and meal**: the dairies sell every egg they lay (zero stock on 78% of days), 15 a day short at the shops;
  milk 16 a day short at the inns while the dairies' cheese rooms take 30 a day of their own; meal 10 a day short at
  the villages' food shops while the dairies and pig farms hold none on 27-30% of days. Their patch's step is small
  (the shops' wants never reach the farms' draws), so they are raised by what the shops lacked.

**The fixes:**

- **Twelve posts moved in DEV** (the user's go-ahead; a person keeps home, evening and everything else; the work spot
  moves to a free floor tile of the new workplace, the label to its hand's, the greeting and the description's
  workplace clause with it). From the idle tills to the makers short of hours or the shops whose keepers' profit can
  pay a hand: Gullmore's inn's two hands to the tailor (Needle & Drip) and the weaver (Greywool); Ashgate's two to the
  chandleries (The Lamp & Wick, Wickham's); Halford's glassworks' two to the pottery (The Rain Pot) and the printing
  house (The Council Press); the Rag Mill's hand to the perfumer (The Cedar Phial); a foundry hand to Rainford's
  Store; Eastgate Ironmongers' hand to Sump Sundries; a tannery hand to Bridge Stores. And House Greatly's two
  servants at Greatly House to Harbour Provisions and the Eastgate Bakehouse (Greatly's posts 126 -> 110p a day, the
  Crown's grants to it with them). The sea trade's 54 posts are not moved: there is no work for 54 more hands that sells.
  **DEV**: backed up first to `artifacts/backups/ratw_dev_20261105-061236.pgdump` (and its Atlas json, revision 83);
  saved as **revision 84**, the same map (its identity kept); `world_build.py dev`: **build 64**, exported to
  `artifacts/economy/r2/export64` (not committed). Against export63 only these twelve residents differ.
- **Data posts** (the Country's, filled from the unplaced pool as every workshop's are, nobody made): a shop hand at
  Coldwater's and Larkfield's bakehouses and a pot hand at Southmeadow's and Fenshore's alehouses (communities.json, and
  realm2_places' `COMMUNITIES` so the generator agrees); Oakhaven's press 4 -> 2 posts; the Crown's soapworks 8 -> 6,
  Coldwater's wool looms 7 -> 5, Larkfield's linen looms 6 -> 5 (workshops.json; their idle hours above). A fourth still
  hand was tried and taken back (below).
- **The smaller iron trades buy of the foundry** (city_loop.json `suppliers`): the cooperage, the cartwright, the tinker,
  the builders' works, the armoury, the saddlery and the fletcher (~4 bars a day) of the foundry alone; the smithies and
  Larkfield's smithy of either, as before. Both are House Timberland's.
- **Alms for households with no earner** (`almsNoEarner`, a rule in percent, 150 in city_loop.json; Core 100, as any
  household): a household with no member in a paid post is made up to half as much again as the day's alms a head (15p
  at 10p). That part over the day's is paid only of what the treasury holds over its cap, after the short tills' wages,
  so it never takes the wage support's money that night. Logged (`alms_no_earner`, `alms_no_earner_homes` on the economy
  line; the journal "alms: a household with no earner"). The DM's and Atlas's rule limits know it (map_editor's
  RULE_LIMITS).
- **Yields** (country_farms.json): stable8c's patch at no slack, the whole step, for **salt, clay and honey** (never
  spoil, piled 5-18% over what sold). Eggs +5 a day a dairy, milk +5.5, meal +2 a dairy and +1 a pig farm (the shops'
  shortfalls, as the patch's step was ~0).
- **Tests**: `econ_fix_tests` (the no-earner's alms 15p a head and booked apart, an earner's household the day's alone);
  `treasury_tests` and `selfcorrect_tests` keep the alms as any household's (100); `workshops_tests` 32 posts (39),
  `farm_people_tests` the markets' 25 posts (21) and the workshops' 32; `farms_tests` the clay, and the smaller iron
  trades of the foundry alone. ctest **79 of 79**; `test_econ_report`, `test_farms_summary`, `test_country_industry`,
  `test_realm2` pass; `test_map_editor` fails one Greyfen resident count, the same on ridgemere2.

**What was tried and taken back** (each a one-year run on export64, wharf7b's flags, `--farms 0`: an area every 28 days):

- `stable9-y1` (every change above, and the no-slack patch for all eleven slack goods, a fourth still hand, and the
  no-earner's alms at 150 of the treasury's float): the still pressed ~10 apples a day more and the inns, breweries and
  preservers ran out of apples on days 165-185 (18-48 households short a day, 22 days with someone hungry); the
  no-earner's alms cost 163p a day, the treasury fell under its cap and the wage support paid 52p a day in the second half
  against stable8c's 118p.
- `stable9b-y1` (the still back to three hands, hemp back, the no-earner's alms of what is over the cap): the winter's
  fuel, stone and iron ran short (peat 4,020 pieces at the Market Hall, firewood, the masons' stone ~22 a day, the
  smithies' iron bars 12,303 draws short as the hearths' charcoal ran out): the patch sizes each season on its own takings,
  but the woods, the peat cut, the hearths and the quarry carry their summer's surplus into the winter, and a run that
  starts with empty stores has none. So firewood, peat, charcoal, stone, flour, iron ore and flax went back to stable8c's.

**The year's run, `stable9c-y1`** (the branch as committed, export64, 365 days, 687 s; `stable9c-y1-report.md`, its
patch, `.out`; its first 60 days are the 60-day smoke, deterministic). Against stable8c's year 1 (a day):

| | stable9c, year 1 | stable8c, year 1 |
|---|---|---|
| Money conserved every day; the realm's money at the end | yes; 64,572p | yes; 64,561p |
| Estates' posts | 808p of 808p, no night short | 824p of 824p, none short |
| Wage support wanted / paid / unpaid | **291** / 77 / 213p | 323 / 120 / 203p |
| ... inns, smithies, preservers wanted | 44.5, 32.4, 20.0p | 64.7, 35.3, 24.1p |
| The treasury: mean, at the end; least in summer, autumn | 1,322p, 1,739p; 200, 200p (its float) | 1,621p, 1,733p; 474, 243p |
| Alms; of it the no-earner's (households a night) | 465p; 38p (7.4) | 451p; none |
| Households short of food (economy line; the shops' last hour) | **0.46**; **0.13** (29 households, 39% with no earner) | 0.90; 0.22 (32, 75%) |
| Hunger: days with any, the most hours | 8, 8 | 6, 10 |
| The second half (days 182-364): the Crown's purse; its rents; grants out | 1,627 -> 1,533p (mean 1,606p); 791p; 453p | 1,698 -> 1,450p (1,578p); 756p; 444p |
| ... Timberland, Greatly, Fell, Grayrock purses | 361 -> 451, 4 -> 53, 194 -> 318, 26 -> 27p | 315 -> 429, 29 -> 61, 237 -> 393, 53 -> 46p |
| ... the foundry: takings, crew-hours used; to its House | 71.6p, 92%; 30.7p | 40.4p, 57%; 25.2p |
| ... the ironworks' takings | 177.4p | 223.6p |
| ... piles at the farms: salt, clay, honey | +475, 0, +264 | +2,112, +1,602, +1,032 |
| ... eggs, milk short (the shops) | 5.2, 2.7 | 19.0, 12.8 |
| ... **apples short** (the shops) | **67.7** | none (the orchards held ~810 each) |
| The keepers' profit; the toll | 697p; 1,511p (second half) | 801p; 1,562p |
| Hides rotted | 11.2 | 9.7 |

The moved posts did what they were for at the tills: the two small inns' support fell from ~7.7p a day each to ~1.5p,
Halford's glassworks' from 3.4p to 0.2p, the Rag Mill's 3.5p to 1.0p, Eastgate's smithy's 3.2p to 0.2p, Oakhaven's
press 5.8p to 0.6p; the tills that took the hands paid them (none on the support) and sold more (the tailor 82 -> 104p
a day, the weaver 79 -> 103p, Wickham's chandlery 118 -> 140p, the pottery 56 -> 76p, the printing house 54 -> 72p,
the perfumer 56 -> 74p).

**Verdict: better on the wage support, the foundry, the slack piles, the dairies and the short households; not
stable.** What it costs and what is open:

1. **The comforts are one pot, and so is the toll.** Each hand moved onto a selling till lowered the wanted ~3p a day,
   not the ~7p of a post: the makers that took them sold the comforts other makers had sold (the Lamp & Wick's takings
   171 -> 146p as Wickham's rose; the country workshops' share). Wanted fell 323 -> 291p, but the toll fell with the
   households' spending (~50p a day in the second half; in part the Crown's workshops' trimmed wages, its estate's now
   rather than the households'), so the treasury paid less of it (77p against 120p) and touched its float in summer and autumn.
   Year 1 is the lean year in every run; the three-year run is the measure.
2. **Apples** are short ~68 a day in the second half (the inns, the breweries, the preservers), where stable8c's
   orchards held ~810 each: the orchards sell 53.6 a day each against 48, the Coldwater still taking 41 a day against
   20 for the same apple brandy (with the same three hands). Not traced: its keeper, drawn from the pool, may be
   another wolf than stable8c's. Next: the orchards' apples back up to what was drawn, or the still's cider from the
   orchards first.
3. **The slack fuels, stone, flour, iron ore and flax still pile** (as in stable8c): the patch cannot cut them while it
   sizes each season on its own takings. A patch for goods that carry over (the year's takings, or the least stock of
   the year at zero) would.
4. **The Crown** still sits near its keep and its purse fell over the second half (1,627 -> 1,533p), less than
   stable8c's (1,698 -> 1,450p); its rents rose 35p a day (its workshops), and its grants to Grayrock (390p) and Greatly
   (64p) are as they were but for Greatly's two servants. Grayrock's posts (the sea trade's 54 and its people) are 476p a night on ~40p of rents and fees.
5. **Hides** rot at ~11 a day; not changed this round (a pig's hide is a by-product; the realm's leather uses ~5).
6. Not run: a 3-year run on this branch.

### stable9's 3-year run (ridgemere2 47d9213, 2026-11-05)

Round 4's fixes merged (the moved posts, the no-earner's alms, the foundry's buyers, the yields), build-core rebuilt after
the merge, **build 64's export (`export64`)**, wharf7b's flags (`--farms 0`, an area every 28 days); 1,095 days,
2,236 s, 1.1-2.2 s a day. Report, patch and log: `artifacts/economy/r2/stable9-report.md`, `stable9-report.md.patch.json`,
`stable9.out` (the logs kept, not committed). Its first 365 days are `stable9c-y1` again, number for number
(deterministic). **world_check ended with exit 134** ("free(): invalid next size (fast)") after the last day, while
writing the end summary: every day's line and the full log are whole (1,095 days, money conserved on each), but the
`.out` stops in the Wharf's book line. stable8c's and stable9c-y1's runs exited 0. A heap fault somewhere in the run,
found only at a free; not traced (an ASan run of three years is hours; `build-core-clang-sanitize`, a 60-day and a
1-year run first).

A day unless marked; year 3, years 1-3 where given:

| | stable9 | stable8c | wharf7b | producers7 |
|---|---|---|---|---|
| Money conserved every day; at the end | yes; 64,590p | yes; 64,577p | yes; 64,588p | yes; 64,595p |
| Estates' posts; nights short | 808p of 808p; none in three years | 824p of 824p; none | 788p; 107 | 803p; 0 |
| The Crown's purse (year ends; year 3's least) | 1,533, 1,637, 1,306p (851p) | 1,450, 1,443, 1,281p (901p) | 1,002, 976, 625p | 946, 1,005, 714p |
| The Crown's purse by quarter, year 3; its keep | 1,511, 1,440, 1,416, 1,440p; 1,322p | 1,405, 1,328, 1,371, 1,358p; 1,342p | 566, 550, 572, 751p | 670, 595, 786, 952p |
| Grayrock, Timberland, Greatly, Fell (year ends) | 27/20/13, 451/495/353, 53/35/34, 318/349/379p | 46/21/24, 429/431/297, 61/37/45, 393/378/392p | - | - |
| The Crown's rents in (years 1-3); its grants out | 673, 771, 724p (year 3: land 656p, of it surplus 247p; shops 68p); 298, 499, 519p | 647, 704, 658p; 293, 507, 521p | 464p | 503p |
| House Fell's rents in (years 1-3) | 157, 156, 131p | 178, 177, 154p | 47p | 34p |
| The foundry's takings (years 1-3); to its House | **95.7, 68.4, 53.9p**; 29.4, 22.1, 14.2p (2 hands; wages 99% paid, 0.2p support) | 91.6, 25.8, 14.7p; 34.3, 9.7, 3.6p | 3.4%; 0p | 0.2%; 0p |
| The ironworks' takings (years 1-3) | 222.7, 170.8, 147.0p | year 3: 188.3p | - | - |
| The takes (pieces a day); unfit | tallow 9.5, bells 1.2, hides 1.7, bones 1.0; 334p | tallow 9.4, honey 2.4, bells 1.8, hides 1.3, bones 0.7; 349p | - | - |
| Eggs to households; short | 108.6; **5.7** | 107.8; 15.2 | 66.9; 95.0 | 49.5; 114.1 |
| Milk, meal short | **3.3, 0** | 16.5, 10.0 | - | - |
| Apple brandy to households; short | 15.8; 0 | 16.7; 0.1 | 5.5; 4.6 | - |
| **Apples short (the inns, preservers, breweries, the still)** | **47.6** (years 1-3: 69.6, 15.4, 47.6) | none (the orchards held ~2,200 each) | - | - |
| Hides rotted (years 1-3) | 11.2, 19.2, 19.6 a day | 9.7, 19.1, 19.8 | none (piled) | - |
| The treasury's least; 4-week means; year ends | 1,028p; 1,637 -> 3,177p (mean 2,050p); 1,739, 1,864, 3,278p | 1,364p; 2,828 -> 3,981p (2,528p) | 1,970p; 4,811 -> 5,810p | 1,903p; 4,142 -> 4,669p |
| Wage support wanted / paid / unpaid (years 1-3) | 291/77/213, 233/94/139, **252/195/57p** (last 4 weeks paid 265p) | 323/120/203, 278/191/87, 305/268/37p (339p) | 303/296/7p (year 3) | 311/295/16p |
| Alms; of it the no-earner's (households a night), years 1-3 | 465p; 38 (7.4), 475; 45 (8.8), 448; 78 (15.2) | 451, 439, 416p; none | - | - |
| The keepers' profit (years 1-3); the toll | 697, 483, 434p; 1,499, 1,534, 1,612p | 801, 521, 446p; 1,540, 1,602, 1,654p | - | - |
| Hunger: days, the most hours, households short | 3, 8, **0.35** (38 days with any) | 6, 7, 0.34 (35) | 6, 9, 0.31 (27) | 3, 8, 0.28 (34) |
| The hh_short table (the run): a day; households; no earner, alone; mean purse | 0.11; 54; 30%, 35%; 23.7p | 0.14; 46; 68%, 63%; 17.3p | - | - |
| The beyond paid in / was paid (years 1-3) | 106/106, 64.7/64.7, 58.8/58.8p | 147/147, 92.5/92.5, 83.0/83.0p | 112.7, 56.3, 52.7p | - |
| The Wharf's fees; export basket; the credit (year 3) | 4.7p; 6.1p; 0-38p, 5p at the end | 7.0p; 7.2p; 0-38p, 18p | 3.2p; 25.4p | - |

The beyond's year 3 by good (paid it a day): butter 46.0p, eggs 8.6p (from 23.1p), tin ore 2.2p, copper ore 2.0p. It
paid for the bells 29p a day, tallow 9.5p, hides 8.3p.

**Wage support by trade** (wanted, year 3): smithies 34.6p, inns 19.4p (from 42.7p), bakeries 15.1p, brickworks 14.7p,
the sawmill 12.7p, preservers 12.3p. The worst tills: the sawmill (3 posts, 24% of its hours used, 9.8p), Pennock's and
Gilbey's brickworks (8.3p, 3.4p), the charcoal store (8.0p), the cartographer and the paintworks (7.2p each), the four
smithies (6.2-7.0p each, 21-27% used), Southmeadow's pottery (6.7p), Saltstack's bakery (5.9p), the mason (5.4p),
Oakhaven's press (5.3p), Greyby's glassworks (4.7p). The two small inns are down to ~3.5p each.

**Piles in year 3** (day 730 to 1,094, at the farms and works): salt, flour, peat and honey no longer pile (honey -119,
each hive ~223 at the end, from ~2,250); clay +1,467 (from +5,958), firewood +3,433 (from +7,095). Still piling:
charcoal +4,312, iron ore +3,714, firewood +3,433, hemp +3,331, stone +2,694, timber +2,638, flax +1,871, limestone
+1,867, sand +1,804, grapes +1,740, clay +1,467, wool +1,366, herbs +1,246, wine +911. Oats fall (-1,089).

**Apples.** The orchards grow the same (283 a day in years 2-3) and sell all of it (273 a day, 10 eaten; 0-3 held each
at the year ends, against stable8c's ~2,200). What the city bought is nearly the same (273 against 271 a day: the inns
105, preservers 65, breweries 48, the Coldwater still 45, the presses 10); with no stock at the orchards the shelves run
dry and 47.6 a day of wants go unmet across 22 tills. The orchards were drained in year 1 (the still and the preservers
took 17 a day more than in stable8c, as stable9c-y1 found), and the realm's demand is now just over what they grow, so
they never refill. Bilgemarsh's preserver (idle in stable8c) now makes dried apples and preserves (~12 a day in year 3).

**Against the stable definition, year 3:**

| test | result | stable? |
|---|---|---|
| Money conserved every day | yes (the run's exit 134 is after the last day) | yes |
| Estates' posts paid every night | 808p of 808p, no night short in three years | yes |
| No estate purse trending down | the Crown's 1,533 -> 1,637 -> 1,306p at year ends, by quarter ~1,511 -> ~1,440p, 16p under its keep at the end; its rents 771 -> 724p, its grants out 499 -> 519p. Timberland's 495 -> 353p (its works' rents 182 -> 147p). Grayrock's 27 -> 20 -> 13p (grant-fed). Fell level (318, 349, 379p), Greatly level at ~35p | **no**: the Crown at its keep and falling in year 3, Timberland falling |
| The treasury over its float | least 1,028p, rising 1,637 -> 3,177p | yes |
| Wage support not climbing | wanted 291, 233, 252p (year 3's quarters 204, 250, 289, 266p); paid 77, 94, 195p, the last four weeks 265p | **no** for what is paid; wanted lower than stable8c's (252 against 305p) |
| Hunger as low as wharf7b | 3 days, 8 hours at most (wharf7b 6, 9); households short 0.35 against 0.31 | **no, narrowly** |
| No runaway pile, no standing shortage a producer should cover | apples 47.6 a day short; the fuels, iron ore, hemp, stone, timber pile; hides rot 20 a day | **no** |
| The valve nets | 58.8p in, 58.8p out a day; the credit 5p at the end | yes |

**Verdict: not stable.** Round 4 did what it aimed at: the wage support wanted is lower every year (252 against 305p in
year 3), the foundry keeps selling (54 against 15p a day in year 3), eggs, milk and meal are nearly met (5.7, 3.3, 0 short
from 15.2, 16.5, 10.0), salt, flour, peat and honey stopped piling, and the short households are fewer and not mostly the
lone and earnerless any more (30% with no earner, from 68%). What fails:

1. **The Crown's purse** falls in year 3 (1,637 -> 1,306p, under its keep of 1,322p at the end; its year-3 least 851p).
   Its grants out (519p a day) are Grayrock's 440p (476p of posts on 32p of rents and 5p of fees) and Greatly's 79p
   (110p on 31p); its rents fell 47p from year 2 (land 693 -> 656p as the dairies' and orchards' surplus fell, shops 78 ->
   68p). Fix: size the sea trade's 54 posts to what the boats and the Wharf earn (move the hands to tills that sell: the
   preservers' apples, the bakeries short of hours), or give Grayrock the fees of the takes it ships.
2. **Apples are short 47.6 a day** with the orchards selling every apple they grow: the realm's demand passed the
   orchards' yield once year 1 drained their stores. Fix: the orchards' apple yield up ~15-20% (the patch's whole step for
   autumn and winter, which it marks supply-bound), or a sixth orchard on Fell's land (Fell's purse has room: +105p a
   night over its keep).
3. **Wage support paid climbs** (77, 94, 195p; 265p in the last four weeks) as the treasury rises over its cap; wanted
   rose again in year 3's second half (289p in autumn). The idle posts left are small makers with no buyers (the sawmill,
   the brickworks, the charcoal store, the cartographer, the paintworks, the smithies at ~25% of their hours). Fix: move a
   hand from each of the sawmill, the brickworks, the charcoal store, the paintworks and the cartographer (the user's
   go-ahead covers DEV moves) to the tills bound by their crews (the country press, the weaver, Cornley's bakehouse, the
   butchers) and to the preservers short of apples once item 2 is fixed.
4. **Timberland's purse falls a year** (495 -> 353p) as its works' rents fall with the fuel piles (charcoal +4,312,
   firewood +3,433, timber +2,638): the works grow more than is bought. Fix: the patch at no slack for charcoal, firewood
   and timber taken by the year's takings rather than each season's (round 4's stable9b-y1 showed the seasonal cut runs
   the winter short), or a coppice hand fewer.
5. **The keepers' profit falls** (697, 483, 434p) while the toll rises (1,499 -> 1,612p): the city's tills keep less
   each year. Not traced this round; next: the till table by trade by year (takings less inputs, draws and toll).
6. **Hides rot** at ~20 a day (the takes' room is short: 334p a day unfit; the beyond's imports fell to 59p a day). Fix: a
   craft that uses hides (glue at the tannery or parchment for the papermill), or fewer hides a carcass at the dairies.
7. **Households short 0.35** against wharf7b's 0.31; hunger itself is low (3 days, 8 hours at most). The no-earner's alms
   reached 15 households a night in year 3 (78p a day) once the treasury was over its cap.
8. **world_check's exit 134** at the end of the run (a heap fault seen at a free, after day 1,095): the run's numbers are
   whole, but the fault may be anywhere. Fix: run the ASan build (`build-core-clang-sanitize`) for 60 days and a year on
   export64 to find it before the next 3-year run.

### Round 5 of stabilizing (branch `r2-stable9b`, 2026-11-05): fewer of the estates' idle posts, the orchards, the yields that carry over

The user's go-ahead as for round 4 (posts moved in DEV, alms for those with no earner, a few rounds, real fixes). No
treasury lever (toll, wage support, float), no move of the Wharf's credit or its injection, cheese left as it is. **DEV
written** (nine posts, below; backed up first); no NPC made.

**The trace** (stable9's logs, `stable9.full.jsonl.gz` by year; a day):

- **The demand that fell is the estates' basket.** The estates' and the Wharf's buying of the city's goods (`basket_spent`)
  was 820, 270 and 149p a day over the three years. In year 1 the Wharf still held its first purse (707p) and sold its
  opening stores (`wharf_sales` 835p a day, then 65 and 59p). After that, what the estates can spend is what they hold
  over their keep, and the Crown's spare went to Grayrock's posts (grants 298, 499, 519p). The makers that sell to that
  basket lost their buyers year by year: the masons' stone 28.6, 21.8, 18.3 a day; the ironworks' ore 46.0, 37.7, 32.5;
  the smithies' iron bars 15.6, 10.6, 9.4; the brickworks' limestone 9.5, 6.8, 5.7; the charcoal store's timber 10.2,
  5.0, 2.8. So the works grew more than was taken (charcoal 0.72 of its yield taken in year 3, stone 0.71, timber 0.73,
  sand 0.72, limestone 0.76, iron ore 0.80, hemp 0.84), Timberland's works' rents fell (182 -> 147p), and the idle makers
  (the sawmill, the brickworks, the smithies, the charcoal store) went onto the wage support.
- **The keepers' profit falls with the takings, and the toll climbs.** All the city's tills (years 1-3): takings 9,227,
  8,971, 8,781p a day; inputs and draws 5,755, 5,582, 5,468p; wages 3,004, 3,084, 3,087p; the toll 1,497, 1,530, 1,608p;
  the keepers' profit 697, 483, 434p. The toll rises while the takings fall because **the toll's rate drifts up**:
  `toll_now` 30 on day 0, 31.5 at day 273, 32.5 at 546, 35 from day 910 (`tollDriftMost` 5, its limit). It is the
  self-correcting rule treasuryDrift: the treasury's level (1,200-2,200p) sits far under `treasuryTarget` (5,500p), so
  every 28 days it firms the toll and tightens the alms. That is a treasury lever and is **not changed** here; it is
  the user's call (below).
- **Apples were not short of what was eaten.** With stock at the orchards (a first try at +17%, `s10a`) the city bought
  250-278 a day and the orchards piled ~18,700 in year 1. stable9's 47.6 a day short were wants repeated each day while
  the orchards were empty, after year 1's still and preservers drained them. What the city takes is ~266 a day with the
  orchards' 10 eaten; the old yield was 283.
- **The short households** (the hh_short table, at the shops' last hour) were 0.13, 0.11, 0.10 a day over the three years;
  the economy line's `households_short` counts each market hour a household stays short, so it moves with noise between
  runs (stable9's year 3 0.35, a repeat with other yields 0.47, this round's 0.36).
- **Hides**: the farms' hides (2.1 a day at the dairies and pig farms) never sold once in three years and all spoiled;
  the butchers' (~16 a day) rot past the Wharf's room (round 2's shopRot).

**The fixes:**

- **Nine posts moved in DEV** (round 4's script, `move_posts.py`, with new moves; each keeps home, evening and the rest,
  the work spot to a free floor tile beside the till's hands, the label, greeting and the description's workplace
  clause to match): seven of the sea trade's pier hands (rm_dorcas_rainford, rm_frida_kelpford, rmx_ansel_kelpford,
  rmx_ottilie_dripdale, rmx_una_brineridge, rmx_winifred_nailton, rmx_orm_nailhand) to The Lamp & Wick, The Rain Pot,
  Needle & Drip, Commons Sundries, Greywool, Wickham's Chandlery and Rainford's Store, and two of Greatly House's
  servants (rm_maren_tarhand, rm_oda_kelpridge) to The Council Press and Vane & Daughters: the city's tills whose
  keepers' profit in stable9's year 3 was 9-17p a day over their wages, one hand each. Grayrock's posts 476 -> 420p a
  night, Greatly's 110 -> 94p. The Market Hall's provisioner (27p of profit) was not given one: a hand in a hall of six
  shops has no single keeper to work for. No more were moved: the other tills' profit (under ~9p) would have gone onto
  the wage support. **DEV**: backed up first to `artifacts/backups/ratw_dev_20261105-082020.pgdump` (and its Atlas json,
  revision 85); saved as **revision 86**, the same map; `world_build.py dev`: **build 65**, exported to
  `artifacts/economy/r2/export65` (not committed). Against export64 only these nine residents differ.
- **A second shop hand at Coldwater's bakehouse** (communities.json, and realm2_places' `COMMUNITIES`): still bound by
  its two posts' hours (99% used), its bread and honey cakes at the band's top on 23% of days, its keeper 33p a day over
  its wages. Filled from the pool.
- **The yields that carry over** (country_farms.json; the goods that never spoil, so a whole year's takings are what
  matter, not a season's): half way to stable9's years 2-3 takings + 5%, every season by the same factor so the summer
  still stocks the winter: charcoal x0.93 (the Upper hearths and the Greenholt coppice), iron ore x0.95, stone x0.91,
  timber x0.915, limestone x0.94, sand x0.905, hemp x0.95, grapes x0.927, wool x0.954, herbs x0.79, wine x0.8. The whole
  way (`s10a`) left stone, limestone, sand, iron ore and timber 4-16 a day short in year 2, when the basket still bought
  more. **Apples** +3% in autumn and winter only (the patch's supply-bound seasons). **Hides** at the farms 0 (never sold).
- **Tests**: `farms_tests` the quarry's stone and the pits' sand; `farm_people_tests` the markets' 26 posts. ctest **79 of
  79**; `test_realm2`, `test_econ_report` pass.

**What was tried and taken back** (3-year runs on export65): `s10a`, apples +17% and the carry-over goods the whole way
(stopped on day 735: apples piled 34,000 by year 2's end and the builders' goods ran short); `s10b`, apples +4% all year
and wine x0.57 (year 3: apples piling 28 a day, the vintner's takings 43p a day lower, Fell's rents 115p against 131p).

**The 3-year run, `s10c`** (the branch as committed, export65, wharf7b's flags: `--farms 0`, an area every 28 days;
1,095 days, 2,583 s, exit 0; `s10c-report.md`, its patch, `s10c.out`; its first 60 days are the 60-day smoke,
deterministic). A day, year 3 unless marked:

| | s10c | stable9 |
|---|---|---|
| Money conserved every day; at the end | yes; 64,589p | yes; 64,590p |
| Estates' posts; nights short | 736p of 736p; none in three years | 808p of 808p; none |
| The estates' basket (years 1-3) | **877, 333, 230p** | 820, 270, 149p |
| The Crown's grants out (years 1-3) | 252, 423, **441p** (Grayrock 381p, Greatly 60p) | 298, 499, 519p |
| The Crown's rents in (years 1-3) | 660, 762, 723p | 673, 771, 724p |
| The Crown's purse: year ends; year 3's quarters; keep | 1,590, 1,602, 1,333p; 1,473, 1,415, 1,391, 1,417p; **1,174p** (least 804p) | 1,533, 1,637, 1,306p; 1,511, 1,440, 1,416, 1,440p; 1,322p |
| Timberland: year ends; its works' rents (years 1-3) | 513, 476, 335p; 116, 193, 164p | 451, 495, 353p; 116, 182, 147p |
| Fell, Greatly, Grayrock (year ends) | 300/270/373, 52/33/52, 45/19/15p | 318/349/379, 53/35/34, 27/20/13p |
| Wage support wanted / paid / unpaid (years 1-3) | 287/83/204, 218/96/122, **234/184/50p** | 291/77/213, 233/94/139, 252/195/57p |
| The keepers' profit (years 1-3); the toll; its rate at the end | 686, 491, 433p; 1,503, 1,526, 1,589p; 34.5 | 697, 483, 434p; 1,499, 1,534, 1,612p; 35 |
| The treasury: least, year ends | 1,086p; 1,715, 2,248, 2,940p | 1,028p; 1,739, 1,864, 3,278p |
| Alms; of it the no-earner's (households a night) | 435p; 81p (15.8) | 448p; 78p (15.2) |
| Hunger: days, the most hours; households short (economy line; hh_short) | 6, 8; 0.36; 0.09 | 3, 8; 0.35; 0.10 |
| Apples short; held at the orchards at the end | **0**; 9,584 (1,900 each) | 47.6; 13 |
| Year 3's shortages at the shops (pieces a day, cheese aside) | eggs 5.1, flax 4.5, resin 2.5, clay 2.4 | apples 47.6, eggs 5.7, milk 3.3 |
| The farms' and works' stock at the end | 44,600 | 67,600 |
| Piles, year 3 (growth): charcoal, stone, timber, iron ore, hemp | +2,190, +1,130, +1,130, +1,610, +2,230 | +4,312, +2,694, +2,638, +3,714, +3,331 |
| Hides rotted at the makers (the run) | 15.5 | 16.7 |
| The valve: the beyond in / out (the run); fees; basket | 86p / 86p; 7p; 15p | 77p / 77p; 6p; 14p |

**Against the stable definition, year 3:** money conserved (yes); the posts paid every night (yes); the treasury over
its float (yes, least 1,086p); the valve nets (yes); hunger about as low as stable9's (6 days, 8 hours at most; the
shops'-last-hour table 0.09 a day, stable9's 0.10; wharf7b's measure 0.36 against 0.31, narrowly no); no standing
shortage a producer should cover (yes: apples met, the builders' goods met in year 3); no runaway pile (better, not
done: the carry-over goods pile at half stable9's rate); wage support not climbing (wanted lower than stable9's every
year, 234 against 252p in year 3, but paid still rises with the treasury, 83, 96, 184p); **no estate purse trending down:
no** for Timberland (476 -> 335p at the year ends, its works' rents 193 -> 164p) and the Crown's year end (1,602 ->
1,333p), though the Crown's year-3 quarters stay over its keep (1,391-1,473p against 1,174p).

**Verdict: better, not stable.** The basket the estates buy is a half larger in year 3 (230 against 149p), the grants
are 78p a day less, the wage support wanted is lower every year, apples are met, and the piles are half. What is open:

1. **The toll drifts to its limit** (30 -> 34.5-35) because the treasury sits at a third of `treasuryTarget` (5,500p):
   ~110p a day taken from the tills by year 3, the keepers' profit falling with it in every run since stable8. That is a
   treasury lever and the user's: a target near where the treasury settles (2,000-3,000p) would stop the drift. Not
   changed.
2. **The basket still falls year on year** (877, 333, 230p): the Crown's spare is its rents less Grayrock's ~380p of
   grants. Grayrock's 47 sea-trade posts are paid on ~33p a day of its own; no more of them can be moved to tills that
   pay without putting those tills on the wage support. A real fix needs the sea trade to earn (the Wharf's trade is
   ~86p a day each way) or fewer posts there with somewhere to go (new work, not an idle till).
3. **Timberland's purse falls in year 3** with its works' rents, as the basket's iron, charcoal and timber fall.
4. **Hides** still rot at ~15 a day (no craft takes them; a hide glue or parchment craft would be new goods and buyers).
5. **world_check's exit 134** (stable9) was not found. An ASan and UBSan build (clang, RelWithDebInfo,
   `-fsanitize=address,undefined`) of stable9's own code (7dd7991) ran export64 for 60 days and then for the full three
   years with wharf7b's flags (2.4 hours): no error, exit 0, money conserved. Its numbers are not stable9's to the penny
   (another compiler; the end money 64,584p against 64,590p), so it did not walk stable9's exact path. `s10c` (gcc
   Release, this branch) also exited 0. The fault may be in glibc's allocator under one run's exact path, or in the
   teardown; not traced further. A rerun of stable9 itself with `MALLOC_CHECK_=3` would show whether it repeats.

### stable9b's 3-year run (ridgemere2 5724b04, 2026-11-05)

Round 5 on the merged build (`build-core`, gcc Release, rebuilt: no Core code changed in the merge), **build 65's
export (`export65`)**, wharf7b's flags (`--farms 0`, an area every 28 days); 1,095 days, 2,341 s, exit 0;
`stable9b-report.md`, its patch, `stable9b.out`. No DEV write this time: export65 is the round's.

**It is `s10c` to the byte.** The economy file, the loop state and the full log are identical to s10c's (`cmp`), the end
digests match (people 4023685e8be0a69d, society 3b0cc12e7c44c553), and the report differs only in the order of two
rows tied at the same value. So the merge changed nothing the economy sees, the run is deterministic across builds of
the same code, and every number in s10c's table above is this run's. A day, year 3:

| | stable9b (= s10c) | wharf7b | producers7 |
|---|---|---|---|
| Money conserved every day; at the end | yes; 64,581p (net to the beyond -14p in three years) | yes | yes |
| Estates' posts; nights short (year 3) | 736p of 736p; none in three years | 788 of 824p; Grayrock and Greatly short | 803 of 824p |
| Crown: rents in; grants out; purse at year ends | 723p; 441p; 1,590, 1,602, 1,333p | 464p; 502p; 625p at the end | 503p; 529p; 714p |
| Grayrock, Timberland, Greatly, Fell (year ends) | 45/19/15, 513/476/335, 52/33/52, 300/270/373p | 18, 361, 61, 29p (end) | 18, 318, 66, 28p |
| Treasury mean; wage support paid / wanted | 2,035p; 184 / 234p | 3,999p; 296 / 303p | 3,271p; 295 / 311p |
| Hunger: days; households short (economy line) | 6; 0.36 | 6; 0.31 | 3; 0.30 |
| Alms; of it the no-earner's | 435p; 81p | 399p; 0 | 403p; 0 |
| The valve: in / out; fees; basket (the run) | 86 / 86p; 7p; 15p | - | - |

Also in year 3: the foundry's wages paid in full (25.8p a day over the run) and 26.4p a day to its House; eggs 100 a day
grown and all sold, 5.1 pieces a day short at the shops; cider 124 a day sold, none short; the takes the Wharf's room
could not fit 270p a day (the run).

**Verdict: better, not stable** (the same as s10c's, since it is the same run). Against the definition: money conserved,
posts paid every night, the treasury over its float, the valve nets, no standing shortage a producer should cover (eggs
5 a day is the largest), hunger about wharf7b's (0.36 against 0.31 on the economy line, the shops'-last-hour table 0.09):
yes or nearly. Not yet: **the Crown's year-end purse falls** (1,602 -> 1,333p) as its grants to Grayrock climb (252,
423, 441p); **Timberland's falls** (476 -> 335p) with its works' rents (193 -> 164p); **wage support paid climbs** (83,
96, 184p) as the treasury lets it, though the wanted is flat (218 -> 234p); the carry-over goods (charcoal, stone, timber,
iron ore, hemp) still pile at 1,100-2,200 a year. The open points are s10c's 1-5.

### Round 6 of stabilizing (branch `r2-stable9c`, 2026-11-05): more of the sea trade's posts moved, alms for low wages, the apples

The user's go-ahead as for rounds 4 and 5 (posts moved in DEV, alms for the households with no earner and those living
alone, a few rounds, real fixes). No treasury lever (toll, wage support, float, the treasury's target), no move of the
Wharf's credit or its injection, cheese left as it is. **DEV written** (ten posts, below; backed up first); no NPC made.

**The trace** (stable9b's logs; a day, year 3 unless marked):

- **The estates spend what they hold over their keep, so a purse's year end says little.** The Crown took in 737p a day
  in year 3 and paid out 736.5p (grants 441p, its posts 94p, the basket 202p); Timberland 188.6p in, 188.3p out. Each
  purse sits at its keep plus what the day's basket left unspent, so the year-end figures (the Crown's 1,602 -> 1,333p,
  Timberland's 476 -> 335p) move with the day the year ends on. What does fall is the flow: the Crown's rents 762 ->
  723p, and its margin over its keep by quarter (~340p in year 2, ~250p in year 3) as its grants to Grayrock rise.
- **The money moves from the households to the treasury.** At the year ends (years 1-3): the households' purses 38,277,
  37,161, 36,665p; the treasury 1,715, 2,248, 2,940p; the tills 18,320, 19,145, 19,307p; the estates 2,500, 2,400,
  2,108p. The treasury keeps what the toll brings over its posts, the day's alms and the wage support it pays, and the
  toll's rate drifts up (30 -> 34.5, treasuryDrift: the treasury under its 5,500p target; a lever, not changed).
- **Grayrock's grants are its posts.** 48 sea-trade posts (376p of its 420p a night) on 33p of its own (land 33p, fees
  6p); the Crown's grants to it 381p a day. Of the city's tills, those whose keepers' profit pays a hand (7-22p a day
  over their wages in year 3) and most of whose crews' hours were used: Southway Provisions (one post, 87% of its hours,
  16.6p), The Rain Pot (84%, 15.0p), The Lamp & Wick (63%, 26.4p), Bridge Stores (52%, 11.4p), the Fell Distillery (98%,
  7.2p), The Council Press (65%, 11.4p), Needle & Drip (71%, 9.7p), Greywool (96%, 21.8p). The Market Hall's
  provisioner (86%, 29.9p) shares its hall with five other shops, so a hand there has no one keeper (as round 5 found).
- **The idlest makers**: the sawmill (3 posts, 24% of their hours, 9.8p of wage support a day), Gilbey's tileworks (3,
  28%, 6.7p), Oakhaven's press (2 posts that paid 4.0p of their 14p a day) and Southmeadow's kilns (6.4p of 14p). The two
  butchers bound by their crews (Cinder Butchers 97% of their hours, The Hook and Cleaver 93%).
- **The short households have earners.** Of the 33 households short at the shops' last hour in year 3, 29 had an earner
  (18 two, 9 one), 4 had none and 4 lived alone; most held just the day's alms a head (10p) and nothing over: a family of
  four on two hands' wages (~16-18p) or a lone hand on a floor wage (7-8p) gets less than the day's alms a head from its
  wages, and the no-earner's extra (round 4) passed them by.
- **Apples pile** (+7,224 in year 3; the orchards grow 287 a day and sell 258), and eggs are 5 a day short at the shops,
  flax short at the linen weavers in years 1-2.

**The fixes:**

- **Ten posts moved in DEV** (round 4's script, `move_posts.py`, new moves; each keeps home, evening and the rest, the
  work spot to a free floor tile beside the till's hands, the label, greeting and the description's workplace clause to
  match). Eight of the sea trade's pier hands, one to each of the tills above: rm_isolde_brineton to Southway Provisions,
  rm_lavinia_bilgedale to The Rain Pot, rm_erland_fogley to The Lamp & Wick, rm_agna_dripridge to Bridge Stores,
  rm_dagny_pitchton to the Fell Distillery, rm_runa_ashridge to The Council Press, rm_gerrit_tarmore to Needle & Drip,
  rm_signe_sootwell to Greywool (Grayrock's posts 420 -> 356p a night). And a hand from each of the two idlest makers to
  the two butchers bound by their crews: rm_beatha_millgate from the sawmill to Cinder Butchers, rm_rune_cinderwick from
  Gilbey's tileworks to The Hook and Cleaver. **DEV**: backed up first to
  `artifacts/backups/ratw_dev_20261105-115030.pgdump` (and its Atlas json, revision 87); saved as **revision 88**, the
  same map (its identity kept); `world_build.py dev`: **build 66**, exported to `artifacts/economy/r2/export66` (not
  committed). Against export65 only these ten residents differ.
- **Two idle country posts back to the pool** (workshops.json): Oakhaven's press to its printer alone (its vat hand), and
  Southmeadow's kilns to the potter alone (its kiln hand). Nobody is dropped: the pool's people are unplaced, not a
  household, and the villages' posts take from it as before.
- **Alms for low wages** (Core, `giveAlms`; no new rule, the existing `almsNoEarner`'s): a household whose earners' wages
  together are under the day's alms a head is made up to almsNoEarner percent of it a head, as one with no earner is
  (15p a head at 10p). It is paid after the no-earner's (the alms are now three passes: the day's for every household,
  then the no-earner's part, then the low wage's), of what the treasury holds over its cap and tonight's wage support
  wanted besides, so tomorrow's short tills still find their support. Logged apart: `alms_low_wage` and
  `alms_low_wage_homes` on the economy line, the journal's "alms: a household on low wages".
- **Yields** (country_farms.json): flax and eggs to stable9b's patch, the whole way (flax +4-12%, the linen weavers short
  in years 1-2; eggs +4-10%). After the first 3-year run (`s11c`, below), every season by the same factor: apples x0.95
  (grown 287 a day, sold 255 and eaten 10), grapes x0.88 (sold 26 of 32), hemp x0.93 (47 of 53), wool x0.95 (36 of 39).
- **Tests**: `econ_fix_tests` (a household of two on a hand's 8p made up to 15p a head and booked as low wages; a lone
  hand on 12p the day's alone); `workshops_tests` 30 posts (32), `farm_people_tests` the nine workshops' 30 posts. ctest
  **79 of 79**; `test_realm2`, `test_econ_report`, `test_farms_summary`, `test_country_industry` pass.

**What was tried and taken back** (3-year runs on export66, wharf7b's flags: `--farms 0`, an area every 28 days):

- `s11a` (the low wage's part of the cap alone, in the one pass with the no-earner's), stopped at day ~100: over the first
  60 days it cost 105p a day for 14 households, the treasury ~290p lower than stable9b's and its wage support ~16p a day
  more unpaid. So it keeps back tonight's wage support wanted.
- `s11b` (that, still one pass), stopped at day 379: the low wage's households, met first in their homes' order, took
  what the no-earner's would have had (46.5 -> 29.4p a day in year 1) and more of the lone were short (the hh_short
  table 62 rows against 45, 41 with no earner). So three passes, the no-earner's before the low wage's.
- `s11d` (this round without the low wage's part, for comparison; kept in the scratchpad): year 3's households short
  0.30, but the treasury at the year ends 1,937, 2,432, **4,526p** and the households' purses 38,396, 37,271, 36,517p: the
  money still gathering in the treasury, as in stable9b.
- `s11c` (the posts, the alms in three passes, flax and eggs; before the apples, grapes, hemp and wool): the moved
  distillery hand made more apple brandy of the orchards' cider, and the Coldwater still, which presses apples, sold
  less (it bought 30 apples a day against 47 in year 2): apples +15,000 in year 2 and +8,301 in year 3 (25,626 at the
  orchards at the end), grapes +2,224. Otherwise year 3 as below (households short 0.30, the treasury's year ends 1,903,
  2,190, 2,190p). So the four yields were cut.
- `s11f` (s11e below and its patch half way for the builders' and the iron goods, iron ore, clay, limestone, stone, and
  eggs, short 2.5-4.4 a day in year 3): year 3 worse on nearly every count (households short 0.32 against 0.22, the
  basket 276 against 316p, the Crown's rents 718 against 730p, Timberland's 195 against 216p, the households' purses
  falling again, 37,464 -> 37,181p). Taken back; s11e's yields are the branch's.

**The 3-year run, `s11e`** (the branch as committed, `build-s9c` gcc Release, **export66**, wharf7b's flags: `--farms
0`, an area every 28 days; 1,095 days, 2,242 s, exit 0; `s11e-report.md`, its patch, `s11e.out`; its first 60 days are
the 60-day smoke, deterministic). A day, year 3 unless marked; years 1-3 where given:

| | s11e | stable9b (= s10c) | wharf7b |
|---|---|---|---|
| Money conserved every day; at the end | yes; 64,594p | yes; 64,589p | yes |
| Estates' posts; nights short | 672p of 672p; none in three years | 736p of 736p; none | 788 of 824p |
| The estates' basket (years 1-3) | 907, 373, **316p** | 877, 333, 230p | - |
| The Crown: rents in; grants out (years 1-3) | 641, 756, 730p; 218, 363, **372p** | 660, 762, 723p; 252, 423, 441p | 464p; 502p |
| The Crown's purse: year ends; over its keep by quarter, years 2 and 3 | 1,457, 1,493, 1,327p; 447/404/398/401, 401/364/300/357p | 1,590, 1,602, 1,333p; 388/327/332/321, 321/242/202/234p | 625p at the end |
| Timberland: year ends; over its keep by quarter, years 2 and 3; its rents (years 1-3) | 566, 490, 398p; 164/142/128/303, 173/120/94/307p; 164, 226, 216p | 513, 476, 335p; 163/137/126/294, 150/111/80/227p; 166, 224, 189p | 361p at the end |
| Fell, Greatly, Grayrock (year ends) | 289/216/279, 50/40/73, 30/31/20p | 300/270/373, 52/33/52, 45/19/15p | - |
| The treasury: year ends; mean; least | **1,954, 2,139, 2,210p**; 1,987p; 1,166p | 1,715, 2,248, 2,940p; 2,035p; 1,086p | mean 3,999p |
| The households' purses (year ends) | 38,500, 37,348, **37,548p** | 38,277, 37,161, 36,665p | - |
| Wage support wanted / paid / unpaid (years 1-3) | 280/82/199, 225/106/119, **200/153/47p** | 287/83/204, 218/96/122, 234/184/50p | 303/296/7p |
| The keepers' profit (years 1-3); the toll; its rate at the end | 639, 461, 419p; 1,586p; 34 | 686, 491, 433p; 1,589p; 34.5 | - |
| Alms; the no-earner's (households a night); the low wage's | 467p; 78.5p (15.0); 72.8p (7.9) | 435p; 81.3p (15.8); - | 399p; 0 |
| Hunger: days, the most hours; households short (economy line; years 1-3) | 4, 5; 0.86, 0.61, **0.22** | 6, 8; 0.52, 0.53, 0.36 | 6, 9; 0.31 |
| The hh_short table, year 3: households; no earner; alone | 25; 5; 7 | 33; 4; 4 | - |
| Piles in year 3 (the farms and works' growth) | apples +1,596, charcoal +1,224, wine +884, salt +717, tallow +662, timber +597, flax +560 | apples +7,224, firewood +2,414, hemp +2,196, charcoal +2,195, iron ore +1,605, peat +1,446 | - |
| The farms' and works' stock at the end | 44,289 | 56,588 | - |
| Shortages in year 3 (pieces a day, cheese aside) | iron ore 4.4, pig 3.9, clay 3.9, eggs 2.9, resin 2.9, flax 2.7, limestone 2.5 | eggs 5.1, flax 4.5, resin 2.5, clay 2.4, pig 2.4 | - |
| The valve: the beyond in / out (years 1-3) | 107.6/107.6, 78.8/78.8, 61.2/61.1p; credit 1p at the end | 109.4/109.5, 79.5/79.5, 67.7/67.6p | - |
| The takes the Wharf's room could not fit; hides rotted | 325p; 19.2 a day | 322p; 19 | - |
| The foundry: wages paid; to its House | 25.8 of 26p; 25.1p | 25.9 of 26p; 22.2p | - |

The moved hands' tills paid them (year 3, against stable9b's): Greywool's takings 128 -> 150p a day, Cinder Butchers'
145 -> 190p, The Rain Pot's 94 -> 98p, each paying its hand in full; Southway Provisions' keeper kept 7p over its two
posts' wages (16.6p over one), the Lamp & Wick 18.9p (26.4p), Bridge Stores 10.9p (11.4p); the Fell Distillery and The
Hook and Cleaver paid 95-97% of theirs. The sawmill's wage support fell 9.8 -> 4.3p a day and the tileworks' 6.7 -> 1.8p;
Oakhaven's press and Southmeadow's kilns 7p of posts each, 4.8p and 5.0p of it paid by themselves.

**Against the stable definition, year 3:**

| test | result | stable? |
|---|---|---|
| Money conserved every day | yes | yes |
| Estates' posts paid every night | 672p of 672p, no night short in three years | yes |
| No estate purse trending down | over its keep by quarter, the Crown 401-447p in year 2 and 300-401p in year 3 (stable9b 321-388, 202-321p); Timberland level (94-307p against 128-303p); its rents 226 -> 216p. The year-end purses still fall for the Crown (1,493 -> 1,327p) and Timberland (490 -> 398p): they spend what is over their keep, so the year end is the day's basket | **nearly**: the Crown's margin over its keep falls ~10% from year 2 (stable9b's ~25%) |
| The treasury over its float, wage support not climbing | least 1,166p; year ends 1,954, 2,139, 2,210p (flat); wanted 280, 225, 200p (falling), paid 82, 106, 153p as the unpaid shrinks 199 -> 119 -> 47p | treasury yes; wage support paid **still rising**, toward a falling wanted |
| Hunger as low as wharf7b or lower | 4 days, 5 hours at most; households short 0.22 against 0.31 | **yes** |
| No runaway pile, no standing shortage a producer should cover | the largest pile apples +1,596 a year (~4 a day of 273 grown); iron ore 4.4 a day short at the ironworks, clay 3.9, limestone 2.5 | **nearly** |
| The valve nets | 61.2p in, 61.1p out; the credit 1p at the end | yes |

**Verdict: the closest yet, not quite stable.** Money conserved, the posts paid every night, the valve netting, hunger
under wharf7b's for the first time (0.22 against 0.31), the treasury level across the years (1,954 -> 2,210p, where
stable9b's climbed to 2,940p) and the households' purses no longer falling (37,348 -> 37,548p). What is open:

1. **The wage support paid still rises** (82, 106, 153p) while what is wanted falls (280, 225, 200p): the treasury pays
   more of a smaller want as the unpaid shrinks (199 -> 47p). It should level once nothing is unpaid; a fourth year would
   show it.
2. **The Crown's margin over its keep falls ~10% in year 3** (its land rents 756 -> 730p, its grants 363 -> 372p).
   Grayrock's 40 sea-trade posts (356p a night on 33p of its own) are still the grants: no more city tills had a keeper's
   profit to pay a hand without going onto the wage support. Next: work that sells for the rest (the takes the room could
   not fit, ~325p a day, are where the sea trade could earn if the Wharf's export room grew; that is the credit, a lever).
3. **The builders' and iron goods are a few pieces a day short** (iron ore 4.4, clay 3.9, limestone 2.5) now that the
   basket buys a third more; raising their yields half way (`s11f`) made year 3 worse, so a smaller step, or the iron ore
   alone, is next.
4. **Hides** still rot at ~19 a day (no craft takes them).
5. **The toll's rate** still drifts to 34 (treasuryDrift; the treasury at 2,000-2,200p against its 5,500p target): the
   user's lever.
