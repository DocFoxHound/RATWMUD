# Recording test: 60 days at --economy-level full (r2-farms, data_r5, --farms 0)

Built from the worktree r2-farms (clean build-core, 36f050d plus its uncommitted edits); run with run.sh's switches
(`--simulate 6 1440 --no-check --deterministic --threads 4 --no-profile`), `taskset -c 0-5`, the export farms_export.

| | normal | full |
|---|---|---|
| wall clock | 1:12.3 | 1:13.7 (+2.0%) |
| user time | 92.4 s | 94.6 s (+2.3%) |
| peak memory | 570 MB | 570 MB |
| economy file | 629 KB | 629 KB (byte for byte the same) |
| loop state | 104 KB | 104 KB |
| full tables (gzipped) | - | 679 KB (2.83 MB unpacked, 61 lines) |

Three years (1095 days) at that rate: about 12.4 MB gzipped (52 MB unpacked) for the full tables, beside the normal
files (r5's three years: 18.1 MB economy, 1.4 MB loop state). Well under 150 MB: nothing needs cutting.

The tables' shares (unpacked JSON): ware 52% (56k rows), trade 32% (44k), till 10%, short 3%, the rest 3%. If it ever
must shrink, ware and trade are the ones: fewer goods or shops a row (only those that moved), or weekly rather than daily.

econ_report.py on the full run: 6.1 KB, in 0.1 s. The report follows.

---

# Economy report: 60 days (spring 60)

Money conserved every day: True. Residents 559 -> 559; births 2, deaths 2.

## 1. Imbalances
Goods short (wants no open shop or draw met; "-" is every shop):

| good | shop | season | pieces | buyer-days | days | taken instead |
|---|---|---|---|---|---|---|
| peat | - | spring | 13606 | 13606 | 239 | firewood 13449 |
| apples | rm_desmond_grantham | spring | 1737 | 28 | 28 | - |
| apples | rm_arne_wickdale | spring | 1717 | 28 | 28 | - |
| apples | rm_edmund_kelpstack | spring | 1553 | 28 | 28 | - |
| apples | rm_rosamund_tarridge | spring | 832 | 28 | 28 | - |
| apples | rm_mab_carlisle | spring | 824 | 28 | 28 | - |
| apples | rm_gorm_rustmarsh | spring | 763 | 27 | 27 | - |
| candle_tallow | - | spring | 750 | 750 | 120 | - |

Goods piling up (stock in shops and farms after the first week, growth a year; sales a day of any seller; the days of sales a year adds):

| good | growth/yr | mean stock | sold/day | days of sales/yr |
|---|---|---|---|---|
| hide | 4532 | 560 | 6.1 | 747 |
| bones | 5005 | 574 | 21.1 | 237 |
| tallow | 10515 | 1129 | 66.7 | 158 |
| straw | 386 | 61 | 2.9 | 134 |
| wine | 5675 | 296 | 46.1 | 123 |
| berries | 648 | 109 | 5.5 | 117 |
| leather_cap | 10 | 8 | 0.0 | 103 |
| flour | 5025 | 1115 | 71.7 | 70 |

The farms' stock short of a shop's want (draws), by good and season:

| good | season | draws short |
|---|---|---|
| - | - | - |

## 2. Price factors pinned at the band (1 +/- 25%)

| good | share at the top | share at the foot | till-days |
|---|---|---|---|
| linen_neckerchief | 35% | 0% | 75 |
| garlic_salve | 0% | 31% | 180 |
| comfrey_salve | 0% | 9% | 180 |
| thyme_syrup | 0% | 9% | 180 |
| paw_balm | 0% | 7% | 180 |
| dewormer | 0% | 4% | 180 |
| willow_tea | 0% | 2% | 180 |

The shops' stock factors ranged 0.75 to 1.25.

## 3. Tills short of their wages

| trade | tills | short days | shortfall | support | takings/day | costs/day | owed/day | why |
|---|---|---|---|---|---|---|---|---|
| inn | 4 | 114 | 2565 | 2470 | 80 | 61 | 29 | input costs 4 |
| preserver | 5 | 132 | 1919 | 1892 | 85 | 70 | 20 | input costs 5 |
| butcher | 8 | 110 | 1890 | 1802 | 82 | 65 | 21 | input costs 8 |
| builders_works | 1 | 51 | 1005 | 935 | 23 | 13 | 28 | too many hands 1 |
| distillery | 1 | 36 | 687 | 653 | 56 | 41 | 30 | input costs 1 |
| brickworks | 2 | 56 | 629 | 568 | 61 | 40 | 26 | low sales 2 |
| smithy | 3 | 44 | 574 | 527 | 82 | 62 | 21 | input costs 3 |
| mason | 1 | 30 | 414 | 375 | 112 | 90 | 28 | input costs 1 |

## 4. Money: flows by season and the treasury

| season | largest flows (pennies a day) |
|---|---|
| spring | hh:middle>shops 3593; shops>country 3237; shops>hh:middle 2511; country>shops 2383; shops>treasury 1691; hh:lower>shops 1608 |

The treasury paid out (a day): hh:middle 893, hh:lower 438, hh:upper 232, shops 44, carters 1; took in: shops 1691, hh:middle 0.

The treasury: 1362 -> 6016p; spring 1362 -> 6016 (least 1362).

## 5. Welfare by class (a season's mean)

| season | class | residents | hungry h/head/day | purse/head | household median | comforts/home/day | spent/home/day |
|---|---|---|---|---|---|---|---|
| spring | lower (the Sump, the Rents) | 201 | 0.00 | 38 | 61 | 1.26 | 18.8 |
| spring | middle | 313 | 0.00 | 58 | 67 | 2.60 | 24.8 |
| spring | upper (the Houses) | 45 | 0.00 | 68 | 213 | 4.14 | 38.4 |

## 6. The largest buyers and sellers by good (the goods with the most pennies)

| good | pieces/day | pennies/day | price factor | largest buyer | largest seller |
|---|---|---|---|---|---|
| raw_meat | 236.5 | 554 | 0.78 | hh:middle 28% | country 44% |
| cider | 169.5 | 505 | 0.99 | country 43% | rm_arne_wickdale 18% |
| meal | 99.7 | 469 | 0.78 | hh:middle 36% | country 29% |
| wine | 46.1 | 453 | 0.82 | hh:middle 51% | rm_delphine_saltwick 44% |
| firewood | 535.5 | 402 | 0.75 | hh:middle 24% | country 55% |
| honey_cake | 98.9 | 392 | 0.99 | hh:middle 60% | rm_liv_pitchhand 15% |
| iron_bar | 55.4 | 381 | 0.76 | shop:smithy 54% | country 72% |
| shellfish | 246.6 | 370 | 0.75 | hh:middle 34% | country 37% |
| pamphlet | 57.2 | 356 | 0.78 | hh:middle 52% | rm_josephine_dripgate 59% |
| apples | 371.0 | 340 | 0.92 | shop:brewery 44% | country 48% |
| salt_fish | 85.5 | 334 | 0.98 | hh:middle 41% | rm_philippa_dripworth 14% |
| smoked_fish | 65.8 | 320 | 0.97 | hh:middle 44% | rm_philippa_dripworth 11% |

## 7. Farms

| area | farms | days on | takings/farm-day | wages paid/owed | support | basket/farm-day | crew spent/farm-day | crew short | floats | crew purses |
|---|---|---|---|---|---|---|---|---|---|---|
| fruit | 7 | 32 | 86 | 9344/9344 | 0 | 43 | 40 | 0 | 656 | 1826 |
| grain | 2 | 60 | 108 | 6720/6720 | 0 | 51 | 55 | 0 | 264 | 690 |
| meat | 10 | 4 | 122 | 1920/1920 | 0 | 69 | 47 | 0 | 1113 | 2527 |

Yields that would meet demand + 5% (pieces a day by season, before the yield scale): 76 changes, the patch in `/tmp/claude-1000/-home-martinb-Documents-GitHub-MUDProject/da6a1181-05f7-491a-a334-c3e7343d5540/scratchpad/farms_sim/rec60.patch.json` (`--apply` it to country_farms.json). The largest:

| farm | good | now | balanced |
|---|---|---|---|
| sheep_and_goat_run_2 | mutton | 23.7 / 28.3 / 22.7 / 27.6 | 125.74 / 28.3 / 22.7 / 27.6 |
| sheep_and_goat_run_1 | mutton | 23.7 / 28.3 / 22.7 / 27.6 | 125.74 / 28.3 / 22.7 / 27.6 |
| cattle_and_dairy_farm_3 | raw_meat | 44.5 / 41.9 / 39.1 / 43.4 | 186.75 / 41.9 / 39.1 / 43.4 |
| cattle_and_dairy_farm_2 | raw_meat | 44.5 / 41.9 / 39.1 / 43.4 | 186.75 / 41.9 / 39.1 / 43.4 |
| cattle_and_dairy_farm_1 | raw_meat | 44.5 / 41.9 / 39.1 / 43.4 | 186.75 / 41.9 / 39.1 / 43.4 |
| rabbit_warren_2 | rabbit | 93.4 / 88.1 / 93.5 / 100.8 | 385.88 / 88.1 / 93.5 / 100.8 |
| rabbit_warren_1 | rabbit | 93.4 / 88.1 / 93.5 / 100.8 | 385.88 / 88.1 / 93.5 / 100.8 |
| pig_farm_3 | raw_meat | 23.9 / 22.5 / 21 / 23.4 | 94.47 / 22.5 / 21 / 23.4 |

Farm stock growing over the run: grain_farm_1 flour 312->701, grain_farm_2 flour 312->702, orchard_1 cider 36->158, orchard_2 cider 36->158, orchard_3 cider 36->158, orchard_4 cider 36->158, orchard_5 cider 37->158, vineyard_1 wine 102->340.

