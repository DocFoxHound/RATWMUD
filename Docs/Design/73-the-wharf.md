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
