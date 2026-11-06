# Economy report: 1095 days (spring 276, summer 276, autumn 273, winter 270)

Money conserved every day: True. Residents 559 -> 781; births 57, deaths 58.

## 1. Imbalances
Goods short (wants no open shop or draw met; "-" is every shop):

| good | shop | season | pieces | buyer-days | days | taken instead |
|---|---|---|---|---|---|---|
| stone | rmx_frida_netherby | winter | 3022 | 135 | 135 | - |
| flax | rm_gudrun_targate | spring | 2702 | 216 | 216 | - |
| flax | tn_cw_iris_greywater | spring | 2368 | 213 | 213 | - |
| flax | rm_gudrun_targate | winter | 2276 | 180 | 180 | - |
| stone | rmx_frida_netherby | spring | 2244 | 118 | 118 | - |
| apples | rm_edmund_kelpstack | spring | 2155 | 53 | 53 | - |
| apples | rm_desmond_grantham | spring | 2039 | 52 | 52 | - |
| flax | tn_cw_iris_greywater | winter | 2028 | 180 | 180 | - |

Goods piling up (stock in shops and farms after the first week, growth a year; sales a day of any seller; the days of sales a year adds):

| good | growth/yr | mean stock | sold/day | days of sales/yr |
|---|---|---|---|---|
| berries | 128 | 256 | 0.2 | 703 |
| small_pelt | 108 | 145 | 1.3 | 84 |
| herbs | 472 | 1797 | 8.7 | 54 |
| straw | 106 | 223 | 3.2 | 33 |
| wine | 675 | 741 | 21.9 | 31 |
| willow_bark | 71 | 134 | 2.4 | 30 |
| comfrey | 25 | 64 | 0.9 | 27 |
| wool | 834 | 1547 | 34.2 | 24 |

The farms' stock short of a shop's want (draws), by good and season:

| good | season | draws short |
|---|---|---|
| iron_bar | winter | 36787 |
| cheese | summer | 34987 |
| cheese | winter | 34437 |
| cheese | autumn | 34381 |
| iron_bar | spring | 32971 |
| cheese | spring | 28196 |
| iron_bar | summer | 14983 |
| pig | autumn | 8348 |

## 2. Price factors pinned at the band (1 +/- 25%)

| good | share at the top | share at the foot | till-days |
|---|---|---|---|
| comfrey_salve | 0% | 54% | 3256 |
| cheese | 54% | 0% | 2247 |
| paw_balm | 0% | 45% | 3282 |
| garlic_salve | 0% | 44% | 3267 |
| thyme_syrup | 0% | 31% | 3223 |
| leather_bindings | 23% | 0% | 1095 |
| flea_powder | 0% | 23% | 3199 |
| willow_tea | 0% | 21% | 3285 |

The shops' stock factors ranged 0.75 to 8.00.

## 3. Tills short of their wages

| trade | tills | short days | shortfall | support | takings/day | costs/day | owed/day | why |
|---|---|---|---|---|---|---|---|---|
| inn | 12 | 4643 | 32558 | 17033 | 62 | 46 | 17 | input costs 11, low sales 1 |
| preserver | 5 | 1655 | 18677 | 10950 | 84 | 66 | 17 | input costs 5 |
| smithy | 5 | 1995 | 16464 | 9210 | 36 | 27 | 11 | input costs 5 |
| bakery | 12 | 1530 | 14273 | 6950 | 72 | 44 | 24 | low sales 12 |
| general | 8 | 2099 | 11889 | 7128 | 69 | 52 | 16 | input costs 8 |
| cooperage | 2 | 1399 | 9809 | 5420 | 21 | 13 | 12 | low sales 2 |
| butcher | 8 | 826 | 8829 | 6551 | 92 | 66 | 26 | input costs 7, low sales 1 |
| carpenter | 3 | 847 | 8551 | 3149 | 21 | 7 | 16 | low sales 3 |

Their crew-hours (a till's day: put in, used by its crafts, lost past its bank; the work table):

| trade | hours/till-day | used | lost |
|---|---|---|---|
| inn | 23.0 | 69% | 31% |
| preserver | 18.0 | 32% | 68% |
| smithy | 12.6 | 53% | 47% |
| bakery | 19.8 | 56% | 44% |
| general | 15.1 | 27% | 73% |
| cooperage | 15.4 | 32% | 68% |
| butcher | 22.0 | 85% | 15% |
| carpenter | 18.0 | 36% | 64% |

Bound by their crew (used 95% or more of their hours): cty_nell_plummeadow (country_press, 41.6 of 43.2 h), rm_edmund_kelpstack (brewery, 10.4 of 10.8 h), rm_wynn_rustworth (butcher, 8.9 of 9.1 h), rm_theodora_ropemarsh (butcher, 8.4 of 8.5 h), rm_ivo_tarmarsh (butcher, 8.4 of 8.5 h), rmx_ivo_wainwright (butcher, 8.3 of 8.5 h).

Wage support by year (a day): what the short tills could not pay (wanted), what the treasury paid, what went unpaid; and the wanted by trade (the largest, before the support), where logged:

| year | wanted | paid | unpaid | inn | preserver | smithy | bakery | general | cooperage |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 279 | 91 | 188 | 47.3 | 20.4 | 19.7 | 15.4 | 19.1 | 11.1 |
| 2 | 208 | 118 | 91 | 23.7 | 17.1 | 12.6 | 13.4 | 7.1 | 8.0 |
| 3 | 191 | 149 | 42 | 18.4 | 13.6 | 12.8 | 10.2 | 6.3 | 7.8 |

## 4. Money: flows by season and the treasury

| season | largest flows (pennies a day) |
|---|---|
| spring | hh:middle>shops 3870; hh:lower>shops 2659; shops>farm 2571; shops>hh:middle 2285; shops>treasury 1632; shops>hh:lower 1071 |
| summer | hh:middle>shops 3808; shops>farm 2845; hh:lower>shops 2830; shops>hh:middle 2213; shops>treasury 1516; shops>hh:lower 1130 |
| autumn | hh:middle>shops 3824; shops>farm 2980; hh:lower>shops 2908; shops>hh:middle 2170; shops>treasury 1545; shops>hh:lower 1143 |
| winter | hh:middle>shops 4023; shops>farm 3324; hh:lower>shops 2985; shops>hh:middle 2249; shops>treasury 1719; shops>hh:lower 1151 |

The treasury paid out (a day): hh:middle 928, hh:lower 402, hh:upper 245, shops 31, carters 0; took in: shops 1602, wharf 4, hh:middle 0, hh:lower 0.

The treasury: 1200 -> 2191p; spring 1200 -> 2169 (least 1037); summer 1483 -> 2013 (least 505); autumn 1174 -> 2170 (least 228); winter 1593 -> 2191 (least 1090).

## 5. Welfare by class (a season's mean)

| season | class | residents | hungry h/head/day | purse/head | household median | comforts/home/day | spent/home/day |
|---|---|---|---|---|---|---|---|
| spring | lower (the Sump, the Rents) | 315 | 0.00 | 48 | 114 | 2.26 | 28.0 |
| spring | middle | 376 | 0.00 | 49 | 64 | 2.04 | 23.5 |
| spring | upper (the Houses) | 44 | 0.00 | 53 | 187 | 2.80 | 40.4 |
| summer | lower (the Sump, the Rents) | 334 | 0.00 | 51 | 138 | 2.59 | 29.7 |
| summer | middle | 387 | 0.00 | 48 | 67 | 2.01 | 22.9 |
| summer | upper (the Houses) | 44 | 0.00 | 52 | 197 | 2.79 | 39.7 |
| autumn | lower (the Sump, the Rents) | 340 | 0.00 | 53 | 146 | 2.77 | 30.5 |
| autumn | middle | 391 | 0.00 | 48 | 69 | 2.03 | 22.8 |
| autumn | upper (the Houses) | 43 | 0.00 | 53 | 208 | 2.70 | 38.5 |
| winter | lower (the Sump, the Rents) | 342 | 0.00 | 49 | 131 | 2.49 | 31.6 |
| winter | middle | 391 | 0.00 | 46 | 60 | 1.92 | 24.0 |
| winter | upper (the Houses) | 44 | 0.00 | 48 | 171 | 2.45 | 39.8 |

Households short of food at the shops' last hour (the hh_short table): 0.13 a day, 41 households; 49% with no earner, 46% alone, a mean purse of 22.3p; by class lower 22, middle 107, upper 8; their earners' payers treasury 27, rm_liv_slagstack 14, rm_edric_millton 10, rm_winifred_pitchdale 8, rm_hilde_greyby 8, rm_jorunn_ropewick 6, rm_imogen_cinderwell 6, outside:wharf 5.

## 6. The largest buyers and sellers by good (the goods with the most pennies)

| good | pieces/day | pennies/day | price factor | largest buyer | largest seller |
|---|---|---|---|---|---|
| meal | 153.4 | 742 | 0.81 | hh:middle 36% | cattle_and_dairy_farm_1 9% |
| raw_meat | 198.1 | 488 | 0.82 | hh:middle 30% | cattle_and_dairy_farm_3 13% |
| porridge | 445.7 | 450 | 1.01 | hh:middle 51% | rm_rosamund_tarridge 19% |
| honey_cake | 107.6 | 429 | 1.00 | hh:middle 52% | tn_sm_eskar_cornley 18% |
| cider | 125.0 | 342 | 0.91 | hh:middle 41% | rm_desmond_grantham 11% |
| shellfish | 222.7 | 339 | 0.76 | hh:middle 34% | sea_fishing_boat_1 9% |
| butter | 143.0 | 335 | 0.78 | hh:middle 35% | wharf 14% |
| eggs | 215.7 | 330 | 0.76 | hh:middle 35% | rm_thyra_plankridge 11% |
| bread | 315.7 | 317 | 1.00 | hh:middle 51% | tn_sm_eskar_cornley 18% |
| firewood | 417.0 | 306 | 0.73 | hh:middle 22% | rm_josephine_dripgate 30% |
| candle_tallow | 142.5 | 285 | 1.00 | hh:middle 56% | rm_sten_kelpwell 47% |
| salt_pork | 47.8 | 285 | 0.99 | hh:middle 53% | rm_ragnar_millmarsh 8% |

## The Wharf and the Houses' estates

In (a day): the Wharf's goods to the shops 339p (after the toll); the estates' rents 301p (House Timberland 203p, the Crown 57p, House Greatly 36p, House Fell 5p); the farms' land rents 812p (the Crown 654p, House Fell 127p, House Grayrock 31p).
Out (a day): the estates' posts 648p of 648p owed (the Houses' people 304p, 38 posts, the sea trade 288p, 36 posts, the Houses' heads 56p, 4 posts); their basket 565p.
The Wharf's purse (House Grayrock's; the other estates' below): 551 -> 11p (least 0).

The Houses' estates (a day over the 1095 days they were split; the Wharf's own sales are House Grayrock's, which runs it):

| estate | posts | rents in | grants in | grants out | levy in/out | posts paid/owed | nights short | bought | purse |
|---|---|---|---|---|---|---|---|---|---|
| House Grayrock | 40 | 31 | 237 | 0 | 0/4 | 332/332 | 0 | 186 | 551 -> 11 (least 0) |
| House Timberland | 9 | 203 | 2 | 0 | 0/3 | 72/72 | 0 | 128 | 207 -> 417 (least 0) |
| House Greatly | 11 | 36 | 57 | 0 | 0/0 | 94/94 | 0 | 0 | 151 -> 93 (least 0) |
| House Fell | 7 | 132 | 1 | 0 | 0/1 | 56/56 | 0 | 76 | 90 -> 331 (least 0) |
| the Crown | 11 | 711 | 0 | 297 | 7/0 | 94/94 | 0 | 324 | 225 -> 1298 (least 225) |

The estates by year (a day; land: the food farms' land rents, in brackets their surplus over their design's basket; works: the works' rents; shops: the workshops' and the Houses' city businesses'; fees: the Wharf's; grants and levy net; the purse at the year's end; keep: its mean keep a night, estateKeep, its reserve's share, its posts' nights and, the Crown's, what the others lack of theirs; over: its purse over its keep a night, what it may spend):

| estate | year | rents in | land (surplus) | works | shops | fees | grants | levy | posts paid/owed | nights short | purse | keep | over |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| House Grayrock | 1 | 28 | 28 (17) | 0 | 0 | 10 | +122 | -11 | 332/332 | 0 | 53 | 765 | -437 |
| House Grayrock | 2 | 33 | 33 (20) | 0 | 0 | 7 | +292 | +0 | 332/332 | 0 | 15 | 766 | -732 |
| House Grayrock | 3 | 31 | 31 (19) | 0 | 0 | 5 | +295 | +0 | 332/332 | 0 | 11 | 766 | -734 |
| House Timberland | 1 | 167 | 0 (0) | 116 | 52 | 0 | +5 | -1 | 72/72 | 0 | 537 | 166 | +81 |
| House Timberland | 2 | 228 | 0 (0) | 194 | 34 | 0 | +0 | -3 | 72/72 | 0 | 415 | 166 | +187 |
| House Timberland | 3 | 213 | 0 (0) | 181 | 32 | 0 | +0 | -6 | 72/72 | 0 | 417 | 166 | +169 |
| House Greatly | 1 | 22 | 0 (0) | 22 | 0 | 0 | +71 | +0 | 94/94 | 0 | 56 | 217 | -194 |
| House Greatly | 2 | 47 | 0 (0) | 42 | 5 | 0 | +47 | +0 | 94/94 | 0 | 49 | 217 | -170 |
| House Greatly | 3 | 40 | 0 (0) | 37 | 3 | 0 | +54 | +0 | 94/94 | 0 | 93 | 217 | -177 |
| House Fell | 1 | 140 | 135 (114) | 0 | 5 | 0 | +4 | -0 | 56/56 | 0 | 278 | 129 | +90 |
| House Fell | 2 | 131 | 127 (102) | 0 | 4 | 0 | +0 | -0 | 56/56 | 0 | 311 | 129 | +90 |
| House Fell | 3 | 124 | 120 (95) | 0 | 4 | 0 | +0 | -1 | 56/56 | 0 | 331 | 129 | +81 |
| the Crown | 1 | 648 | 597 (229) | 0 | 51 | 0 | -203 | +12 | 94/94 | 0 | 1487 | 775 | +391 |
| the Crown | 2 | 752 | 689 (268) | 0 | 64 | 0 | -339 | +3 | 94/94 | 0 | 1568 | 989 | +434 |
| the Crown | 3 | 733 | 676 (261) | 0 | 58 | 0 | -350 | +7 | 94/94 | 0 | 1298 | 998 | +390 |

## The Wharf: beyond the realm

The valve on (1095 days). A day: the beyond paid in 86p and was paid 86p (net -0p into the realm); the Wharf's fees 8p; its export basket 16p; the credit moved +0p.
The credit: 26 -> 8p (least 0, most 272). What the takes wanted that the room could not fit: 262p a day.
Rotted at the makers (by-products no buyer took, a day): hide 12.8.

| season | in a day | out a day | net a day |
|---|---|---|---|
| spring | 102 | 102 | -0 |
| summer | 77 | 77 | +0 |
| autumn | 78 | 78 | +0 |
| winter | 87 | 87 | -0 |

| good | landed a day | paid for it a day | taken a day (of it from shops) | the beyond paid a day |
|---|---|---|---|---|
| butter | 27.5 | 45.8 | 0.0 | 0.0 |
| handbell | 0.0 | 0.0 | 1.2 (1.2) | 28.8 |
| firewood | 42.9 | 28.3 | 0.0 | 0.0 |
| charcoal | 9.4 | 26.2 | 0.0 | 0.0 |
| hide | 0.0 | 0.0 | 4.7 (4.7) | 23.3 |
| fish | 11.2 | 16.8 | 0.0 | 0.0 |
| linen_cloth | 4.3 | 14.9 | 0.0 | 0.0 |
| iron_bar | 1.9 | 12.2 | 0.0 | 0.0 |
| tallow | 3.2 | 2.2 | 9.7 | 9.7 |
| honey | 4.4 | 11.3 | 0.0 | 0.0 |
| salt | 7.3 | 10.2 | 0.0 | 0.0 |
| raw_meat | 5.5 | 10.1 | 0.0 | 0.0 |
| shellfish | 8.9 | 10.1 | 0.0 | 0.0 |
| peat | 15.1 | 10.0 | 0.0 | 0.0 |
| clay | 14.1 | 9.9 | 0.0 | 0.0 |
| apples | 9.9 | 9.1 | 0.0 | 0.0 |
| meal | 2.6 | 8.9 | 0.0 | 0.0 |
| iron_ore | 12.7 | 8.9 | 0.0 | 0.0 |
| stone | 6.3 | 8.8 | 0.0 | 0.0 |
| timber | 6.2 | 8.7 | 0.0 | 0.0 |
| cider | 3.9 | 7.9 | 0.0 | 0.0 |
| eggs | 6.3 | 6.7 | 0.0 | 0.0 |
| cheese | 2.0 | 5.4 | 0.0 | 0.0 |
| limestone | 7.3 | 5.1 | 0.0 | 0.0 |
| mutton | 1.2 | 5.0 | 0.0 | 0.0 |
| copper_ore | 3.3 | 4.6 | 0.0 | 0.0 |
| milk | 3.8 | 3.9 | 0.0 | 0.0 |
| pig | 0.4 | 3.7 | 0.0 | 0.0 |
| clay_pot | 3.4 | 3.7 | 0.0 | 0.0 |
| tin_ore | 1.7 | 3.4 | 0.0 | 0.0 |
| rabbit | 4.8 | 3.3 | 0.0 | 0.0 |
| sand | 4.6 | 3.1 | 0.0 | 0.0 |
| copper_bar | 0.5 | 2.7 | 0.0 | 0.0 |
| wine | 0.3 | 2.3 | 0.0 | 0.0 |
| iron_hoops | 0.8 | 2.3 | 0.0 | 0.0 |
| cord | 3.3 | 2.3 | 0.0 | 0.0 |
| tin_bar | 0.3 | 2.2 | 0.0 | 0.0 |
| bones | 0.7 | 0.5 | 1.4 | 1.4 |
| fruit_preserves | 0.8 | 1.7 | 0.0 | 0.0 |
| dried_apples | 1.1 | 1.7 | 0.0 | 0.0 |
| brandy | 0.1 | 1.5 | 0.0 | 0.0 |
| apple_brandy | 0.2 | 1.5 | 0.0 | 0.0 |
| oak_bark | 2.0 | 1.4 | 0.0 | 0.0 |
| wool | 1.0 | 1.4 | 0.0 | 0.0 |
| wool_scarf | 0.5 | 1.4 | 0.0 | 0.0 |
| fine_soap | 0.4 | 1.3 | 0.0 | 0.0 |
| pamphlet | 0.3 | 1.2 | 0.0 | 0.0 |
| ink | 0.4 | 1.1 | 0.0 | 0.0 |
| grapes | 0.5 | 0.9 | 0.0 | 0.0 |
| lime | 0.5 | 0.7 | 0.0 | 0.0 |
| herbs | 0.5 | 0.6 | 0.0 | 0.0 |
| perfume_cedar | 0.0 | 0.3 | 0.0 | 0.0 |
| planks | 0.2 | 0.2 | 0.0 | 0.0 |
| wormwood | 0.3 | 0.2 | 0.0 | 0.0 |
| beeswax | 0.1 | 0.2 | 0.0 | 0.0 |
| nails | 0.1 | 0.1 | 0.0 | 0.0 |
| linseed_oil | 0.1 | 0.1 | 0.0 | 0.0 |
| resin | 0.1 | 0.1 | 0.0 | 0.0 |
| willow_bark | 0.1 | 0.1 | 0.0 | 0.0 |
| small_pelt | 0.1 | 0.1 | 0.0 | 0.0 |
| berries | 0.1 | 0.1 | 0.0 | 0.0 |
| steel_bar | 0.0 | 0.1 | 0.0 | 0.0 |
| garlic | 0.1 | 0.1 | 0.0 | 0.0 |
| charm | 0.0 | 0.1 | 0.0 | 0.0 |
| thyme | 0.1 | 0.1 | 0.0 | 0.0 |
| bronze_bar | 0.0 | 0.1 | 0.0 | 0.0 |
| comfrey | 0.1 | 0.1 | 0.0 | 0.0 |
| sword | 0.0 | 0.0 | 0.0 | 0.0 |
| plaster | 0.0 | 0.0 | 0.0 | 0.0 |
| lavender | 0.0 | 0.0 | 0.0 | 0.0 |
| oak_galls | 0.0 | 0.0 | 0.0 | 0.0 |
| mushrooms | 0.1 | 0.0 | 0.0 | 0.0 |
| glass_beads | 0.0 | 0.0 | 0.0 | 0.0 |
| whitewash | 0.0 | 0.0 | 0.0 | 0.0 |
| valerian | 0.0 | 0.0 | 0.0 | 0.0 |
| feathers | 0.0 | 0.0 | 0.0 | 0.0 |
| mint | 0.0 | 0.0 | 0.0 | 0.0 |
| chamomile | 0.0 | 0.0 | 0.0 | 0.0 |
| yarrow | 0.0 | 0.0 | 0.0 | 0.0 |
| reeds | 0.0 | 0.0 | 0.0 | 0.0 |
| leather | 0.0 | 0.0 | 0.0 | 0.0 |
| paper | 0.0 | 0.0 | 0.0 | 0.0 |

The realm's money 51194 -> 64587p; the Wharf's net over the same days +18p.

## Phase 5: the makers' goods sold on, the workshops, the luxuries

Bought to sell on from the makers (a day): 46.2 pieces, 244.4p (fine_soap 13.1, pamphlet 11.4, clay_pot 8.5, wool_scarf 4.9, nails 3.0, apple_brandy 2.6, bandages 1.1, brandy 0.9, cookpot 0.6, linen_cloth 0.0).
The workshops and the Houses' city businesses (a day open): cn_coldwater_soapworks wages 58.8p of 58.9p, to its House 20.5p; cn_coldwater_still wages 33.5p of 36.0p, to its House 4.7p; cn_coldwater_weavers wages 33.3p of 39.1p, to its House 0.0p; cn_fenshore_pottery wages 14.2p of 16.8p, to its House 2.8p; cn_larkfield_press wages 45.5p of 45.5p, to its House 26.5p; cn_larkfield_smithy wages 17.2p of 19.0p, to its House 1.2p; cn_larkfield_weavers wages 45.4p of 46.4p, to its House 9.8p; cn_oakhaven_press wages 6.7p of 9.4p, to its House 0.4p; cn_southmeadow_pottery wages 6.6p of 8.8p, to its House 0.1p; rm_the_grayrock_foundry wages 25.8p of 25.8p, to its House 28.1p; rm_the_grayrock_ironworks wages 46.1p of 48.5p, to its House 10.8p.
The luxuries (a day): 1.6 pieces for 13.2p, 0.8 households (upper 1.6 at 8.1p, middle 0.0 at 5.3p); the Houses' tables 3.9 pieces for 95.8p (24.5p a piece).

## 7. Farms

| area | farms | days on | takings/farm-day | wages paid/owed | support | basket/farm-day | rent/farm-day | crew spent/farm-day | crew short | floats | crew purses |
|---|---|---|---|---|---|---|---|---|---|---|---|
| fish | 7 | 1011 | 43 | 224078/224084 | 0 | 6 | 5 | 0 | 0 | 399 | 0 |
| fruit | 7 | 1067 | 71 | 289064/290080 | 357 | 15 | 17 | 0 | 0 | 697 | 0 |
| fuel (works) | 5 | 927 | 65 | 166860/166860 | 0 | 0 | 29 | 0 | 0 | 366 | 0 |
| grain | 2 | 1095 | 195 | 144360/144360 | 0 | 42 | 86 | 0 | 0 | 371 | 0 |
| honey | 3 | 983 | 37 | 76674/76674 | 0 | 6 | 5 | 0 | 0 | 165 | 0 |
| iron (works) | 2 | 871 | 56 | 54002/54002 | 0 | 0 | 25 | 0 | 0 | 124 | 0 |
| lime (works) | 1 | 843 | 16 | 8430/8430 | 0 | 0 | 6 | 0 | 0 | 20 | 0 |
| meat | 10 | 1039 | 124 | 498280/498280 | 0 | 24 | 51 | 0 | 0 | 1190 | 0 |
| salt | 2 | 955 | 39 | 68760/68760 | 0 | 3 | 0 | 0 | 0 | 144 | 0 |
| stone (works) | 3 | 899 | 27 | 41090/41354 | 203 | 0 | 12 | 0 | 0 | 92 | 0 |

The works (a day; their rent to their House's estate, the Country's purse at first):

| works | House | days on | takings | wages paid/owed | support | rent | float |
|---|---|---|---|---|---|---|---|
| charcoal_upper | Timberland | 871 | 76.1 | 31356/31356 | 0 | 40.1 | 72 |
| clay_fens | Greatly | 899 | 25.0 | 8990/8990 | 0 | 15.0 | 20 |
| clay_mirebridge | Greatly | 899 | 25.1 | 8990/8990 | 0 | 15.1 | 20 |
| coppice_greenholt | Timberland | 927 | 73.2 | 33372/33372 | 0 | 37.2 | 76 |
| coppice_rainwood | Timberland | 927 | 65.5 | 33372/33372 | 0 | 29.5 | 73 |
| logging_oakhaven | Timberland | 927 | 56.3 | 42642/42642 | 0 | 10.3 | 93 |
| logging_rainwood | Timberland | 927 | 56.4 | 33372/33372 | 0 | 20.4 | 72 |
| mine_frost_iron | Timberland | 871 | 35.4 | 22646/22646 | 0 | 9.4 | 52 |
| peat_fens | Timberland | 927 | 75.2 | 24102/24102 | 0 | 49.2 | 52 |
| quarry_granite | Greatly | 899 | 31.2 | 23110/23374 | 203 | 5.5 | 52 |
| quarry_lime | Greatly | 843 | 15.7 | 8430/8430 | 0 | 5.7 | 20 |

The food farms' land rent (a day; to the House whose land each is on, the Country's purse at first; left: takings less wages less rent):

| farm | House | days on | takings | wages owed | rent | rent % | basket | left |
|---|---|---|---|---|---|---|---|---|
| eel_and_fowl_marsh_1 | Grayrock | 1011 | 63.9 | 25.9 | 19.7 | 30.8 | 17.9 | 18.3 |
| lake_and_river_boat_1 | Grayrock | 1011 | 34.2 | 26.0 | 2.7 | 7.9 | 5.4 | 5.5 |
| lake_and_river_boat_2 | Grayrock | 1011 | 34.2 | 26.0 | 2.7 | 7.9 | 5.3 | 5.5 |
| sea_fishing_boat_1 | Grayrock | 1011 | 44.4 | 35.9 | 2.8 | 6.2 | 5.6 | 5.7 |
| sea_fishing_boat_2 | Grayrock | 1011 | 41.8 | 35.9 | 1.9 | 4.5 | 3.9 | 4.0 |
| sea_fishing_boat_3 | Grayrock | 1011 | 41.4 | 36.0 | 1.8 | 4.4 | 3.5 | 3.6 |
| sea_fishing_boat_4 | Grayrock | 1011 | 41.0 | 36.0 | 1.7 | 4.1 | 3.3 | 3.3 |
| orchard_1 | Fell | 1067 | 125.9 | 46.0 | 43.8 | 34.8 | 35.3 | 36.1 |
| orchard_2 | Fell | 1067 | 55.8 | 36.0 | 8.9 | 16.0 | 10.6 | 10.9 |
| orchard_3 | Fell | 1067 | 123.8 | 46.0 | 41.6 | 33.6 | 35.4 | 36.2 |
| orchard_4 | Fell | 1067 | 53.2 | 35.9 | 7.0 | 13.2 | 10.1 | 10.3 |
| orchard_5 | Fell | 1067 | 52.2 | 36.0 | 6.1 | 11.6 | 9.8 | 10.1 |
| vineyard_1 | Fell | 1067 | 43.0 | 36.0 | 4.9 | 11.4 | 2.5 | 2.1 |
| vineyard_2 | Fell | 1067 | 42.1 | 36.0 | 4.4 | 10.4 | 2.2 | 1.7 |
| grain_farm_1 | the Crown | 1095 | 194.4 | 65.9 | 85.8 | 44.1 | 41.8 | 42.7 |
| grain_farm_2 | the Crown | 1095 | 194.6 | 65.9 | 85.8 | 44.1 | 41.9 | 42.8 |
| beekeeper_1 | Fell | 983 | 37.0 | 26.0 | 5.0 | 13.5 | 5.9 | 6.0 |
| beekeeper_2 | Fell | 983 | 37.0 | 26.0 | 5.0 | 13.6 | 5.9 | 6.0 |
| beekeeper_3 | Fell | 983 | 37.0 | 26.0 | 5.0 | 13.6 | 5.8 | 5.9 |
| cattle_and_dairy_farm_1 | the Crown | 1039 | 249.2 | 56.0 | 145.8 | 58.5 | 46.4 | 47.4 |
| cattle_and_dairy_farm_2 | the Crown | 1039 | 247.9 | 55.9 | 144.5 | 58.3 | 46.4 | 47.4 |
| cattle_and_dairy_farm_3 | the Crown | 1039 | 247.1 | 56.0 | 143.9 | 58.2 | 46.2 | 47.2 |
| pig_farm_1 | the Crown | 1039 | 75.2 | 55.9 | 10.8 | 14.3 | 8.3 | 8.5 |
| pig_farm_2 | the Crown | 1039 | 74.1 | 55.9 | 10.5 | 14.2 | 7.5 | 7.6 |
| pig_farm_3 | the Crown | 1039 | 72.5 | 56.0 | 10.1 | 13.9 | 6.3 | 6.4 |
| rabbit_warren_1 | the Crown | 1039 | 41.6 | 26.0 | 6.1 | 14.6 | 9.3 | 9.6 |
| rabbit_warren_2 | the Crown | 1039 | 41.6 | 26.0 | 6.1 | 14.6 | 9.3 | 9.6 |
| sheep_and_goat_run_1 | the Crown | 1039 | 93.5 | 45.9 | 15.0 | 16.0 | 31.9 | 32.6 |
| sheep_and_goat_run_2 | the Crown | 1039 | 93.4 | 45.9 | 15.0 | 16.0 | 31.8 | 32.5 |
| salt_works_1 | the Crown | 955 | 39.1 | 36.0 | 0.2 | 0.4 | 2.8 | 2.9 |
| salt_works_2 | the Crown | 955 | 39.2 | 36.0 | 0.2 | 0.5 | 2.9 | 3.0 |

Farms short of their crews' wages: quarry_granite, sea_fishing_boat_4, vineyard_1, vineyard_2.

Supply-bound (its farms out at the day's end on most days of a season while the shops' draws fell short: what sold was the supply, not the demand; `--apply` takes their whole step in those seasons): butter (spring, summer, autumn, winter), cheese (spring, summer, autumn, winter), clay (spring, summer, winter), copper_ore (spring, summer, autumn, winter), flax (spring, summer, autumn, winter), iron_ore (spring, summer), lime (spring, summer, winter), limestone (spring, summer, winter), pig (spring, summer, autumn, winter), resin (spring, summer, autumn, winter), sand (spring), sheep (spring, summer, autumn, winter), stone (spring, summer, winter), timber (spring), tin_ore (spring, summer, autumn, winter).

Yields that would meet demand + 5% (pieces a day by season, before the yield scale): 129 changes, the patch in `artifacts/economy/r2/stable9e-report.md.patch.json` (`--apply` it to country_farms.json). The largest:

| farm | good | now | balanced |
|---|---|---|---|
| quarry_lime | lime | 2.24 / 1.54 / 1.48 / 1.39 | 2.46 / 1.8 / 1.26 / 2.47 |
| mine_frost_iron | tin_ore | 0.95 / 0.83 / 0.94 / 1.21 | 1.56 / 1.17 / 1.29 / 1.15 |
| sheep_and_goat_run_2 | sheepskin | 0.4 / 0.3 / 0.6 / 0.6 | 0.06 / 0.07 / 0.01 / 0 |
| sheep_and_goat_run_1 | sheepskin | 0.4 / 0.3 / 0.6 / 0.6 | 0.06 / 0.07 / 0.01 / 0 |
| logging_rainwood | resin | 0.04 / 0.07 / 0.16 / 0.04 | 0.37 / 0.45 / 0.75 / 0.4 |
| cattle_and_dairy_farm_3 | meal | 24.22 / 23.53 / 24.21 / 25.28 | 18.21 / 16.51 / 16.18 / 18.53 |
| cattle_and_dairy_farm_1 | meal | 24.22 / 23.53 / 24.21 / 25.28 | 18.21 / 16.51 / 16.18 / 18.53 |
| pig_farm_1 | meal | 9.31 / 9.67 / 10.04 / 9.58 | 7 / 6.79 / 6.71 / 7.02 |

Goods of a farm area no farm grows (no yield to raise: give a farm one in country_farms.json), with the draws they lacked: iron_bar (iron) 6123, brandy (fruit) 30, apple_brandy (fruit) 13.

Made at the farms of what would have spoiled and of their own milk for it (a day over the run; doc 72, the dairies): butter 44.0 made of milk (sold 44.0, 0 held at the end); cheese 3.6 made of milk (sold 3.6, 0 held at the end); milk 102.2 used, 1.0 spoiled.

Farm stock growing over the run: beekeeper_1 honey 127->456, beekeeper_2 honey 128->456, beekeeper_3 honey 128->457, cattle_and_dairy_farm_1 bones 11->173, cattle_and_dairy_farm_1 tallow 65->658, cattle_and_dairy_farm_2 bones 11->174, cattle_and_dairy_farm_2 tallow 66->658, cattle_and_dairy_farm_3 bones 12->174.

