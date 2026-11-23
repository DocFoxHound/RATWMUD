# World-lore survey: Ridgemere 3 (world key `ridgemere3`, db `ratw_ridgemere3`, `world.worlds.id = 'upper_accord'`, name "Ridgemere 3.0", revision 106)

I changed nothing. Every database query ran in a READ ONLY session as `ratw_game`.

## 0. How to read the world (read only)
- Connection: `tools/world_db.py`. `world_db.connect('ridgemere3','game')`, then `SET SESSION CHARACTERISTICS AS TRANSACTION READ ONLY`. Settings live in `Database/.env` (host 127.0.0.1, port 5433). For psql: `python3 tools/world_db.py psql ridgemere3 --role game`.
- The `game` role cannot see `dm.story_*`, `dm.stories` or `public.schema_migrations`; those need the editor or dm role.
- A ready helper is at `/tmp/lore_survey/q.py`: `python3 -I q.py "<sql>" [role]`.
- The list of reachable interiors is at `/tmp/lore_survey/reachable.txt`: 585 ids, worked out by a walk outward from every outdoor cell through `world.links`.
- Rows in `live.*`, `world.*` and `game.*` all use `world_id = 'upper_accord'`.

## 1. Hard rules for world text
These are enforced by `tools/mantle_rules.json` "forbidden", `tools/dialect_check.py`, `Data/Talk/truth.json` and doc 84's writers guide §10.
- Never "dog" or "dogs". Say wolves, and pups for the young.
- Never "god", "gods" or "goddess". There are only two deities:
  - **the Bright One**: kindly, order, civilization, duty, community. He is "he" and "ascended" ("Why did the Bright One go up?").
  - **the Dark One**: malevolent, chaos, temptation, anarchy. The fen oath is "Dark One's mire".
- Players' messages are "private messages", never "tells".
- Characters are male or female only, and pronouns follow sex. The regex also refuses "non-binary" and "genderless".
- The game is called "Run Against the World".
- Ids must never appear in spoken words. The truth.json `ids` regex is `\b(camp|born|cty|rm|tn|w|cn|fs|r2|r3|mk|inc|npc)_[a-z0-9_]{2,}\b`.
- Mantles must not name people literally; use slots. Capitalised words mid-sentence are limited to the allow list:
  - towns: Ridgemere, Coldwater, Larkfield, Southmeadow, Fenshore, Saltwell, Oakhaven, Frosthollow, Mirebridge;
  - places and features: Glasswater, Larkdowns, Long River, High Road, Rag Market, Commons, Heights, Sump, Wharf, Ochre, Oasis Inn, Withy Creek, Fenside, Rivermouth, Fen Shore, Dune;
  - Houses and institutions: Greatly, Fell, Grayrock, Timberland, House(s), Watch, Council, Market Board, Times;
  - the deities, and the weekdays Dawnday, Hearthday, Stoneday, Riverday, Emberday, Marketday, Restday.
- Further place names are allowed in `tools/dialect_allow.json` "places": Great Palace, Old Keep, Records Square, Market Hall, Market Square, Customs House, Fish Hall, Chapel of the Tide, Rainwood, Red Waste, Saltpan, Sallow Fens, Whitefangs, Ochre Hills, North/East/South Gate, Pass Road, Sound Road, Coast/River/Fen/Downs Roads and more. "erased": ["mirebridge"].
- A regional line must name a place that exists in Ridgemere 3 (doc 83 dialect §5). `dialect_check` lists the ones it cannot find.
- Another town's dialect words are refused: a Saltwell wolf never says "dry feet", and a Fenshore wolf never says "peace on you".
- Old-lore inconsistencies to avoid or treat as legend:
  - `Data/Lore/fragments.json` speaks of "the Sound", which is legend for Ridgemere Bay or Cedar Sound.
  - `oh-wood-1` speaks of "the Warmwood". In Ridgemere 3 it is the Rainwood.
  - The Rain Hall exists as `rm_the_rain_hall`, but it is unreachable. The Council sits in the Antechamber (`r2_the_antechamber`).
  - `RATW_Factions.md` (Ashcombe, Brinewater, Vesk, Tobias Grayrock, "Fell = timber") is from the old world and wrong for Ridgemere 3.

## 2. What the player is
- A sapient, four-legged wolf. Civilization is built for paws and mouths: mouth-pull latches, floor cushions, mouth-held quills, harnessed weapons. Scent and body language matter.
- The level is late-medieval to early-modern. Money is silver pennies, plus marks of about 10 pennies. Ridgemere has no magic labour.
- Species seen: timber, arctic, red, maned, ethiopian. Sex is male or female only.
- Classifications (setting bible, `Docs/References/RATW_World_Setting_Bible.md`; doc 43; `Data/Gifts/families.json` tiers):
  - **Normal**: no Gift; progresses by skill.
  - **Gifted**: a small Gift, for utility and support, never damage.
  - **Quickened**: extreme and rare. Quickened magic that others see raises `wardenAttention`. A Quickened wolf can enter a Trance.
- Families: fire, earth, water, wind, sound, blinker, gravity, seer. `death_walker` exists but is NPC-only and rare. Healer is a profession, not a family. Hemlock is the only Blood Wolf ever.
- Wardens are an institution under the Concord (black cloaks with silver trim). They find, assess, train and contain Quickened wolves. Upper Accord is the Concord's city and is off the map.
- Code hooks:
  - `World::tellWardens` and `World::vouchToWardens` only work at a resident who is a member of faction `warden_order`. **No `warden_order` faction or member exists in Ridgemere 3**, so those library objectives cannot complete until one is made.
  - The relation log only holds stale October rows for concord, warden_order and accord_watch.
- Skills (`Data/Progression/skills.json`): fighting, sneak, listening, tracking, craft, labour, commerce, service, travel, watch.
- Attributes: strength, dexterity, wisdom, stamina, hearing, vision, smell.
- Social standing titles (`standing.json`): Stranger (1), Known (3), Familiar Face (5), Respected (8), Notable (12), Renowned (18), Legend (25).
- Earned Gift unlocks: Gifted needs social level 3 and 10 normal scenes. **Quickened needs social level 8, 100 stars from 30 givers, `closedStories: 2` and `reportDays: 30`**, so finished storylines count toward Quickening.
- Levels and XP are being removed (doc 48 §2.2).
- Fame (`Data/Fame`):
  - deeds such as `broke_camp` ("drove the bandits off the road at {place}"), `escorted_caravan`, `report_thief`, `tended`, `kept_promise`, `carried_letter`, `festival_won` and `award`;
  - nickname families: road ("the Roadwarden", "the Bandit-Breaker", "Waywolf", "the Lantern"), healer, honest, watch, festival, neighbour;
  - criers call out the season's deeds at the festivals.
- Wild game (`Data/Wild/animals.json` ids): rabbit, hare, pheasant, grouse, duck, fox, badger, roe_deer, red_deer, boar (fierce), ibex, elk, bear (fierce). Wolves are never wild animals.

## 3. Calendar and weather
- Weekdays: Dawnday, Hearthday, Stoneday, Riverday, Emberday, Marketday, Restday.
  - Restday is church, from 9 to 11, and nobody works.
  - Marketday is the market day.
- Seasons run 92, 92, 91 and 90 days. The festival is on day 46 of each season.
- Each town's festival name is picked by hash from `Core/RatwSchedules.cpp` `FestivalNames`:
  - spring: the Blossom Fair, the Greening, Seedtide;
  - summer: Midsummer, the Long Light, the Sun Feast;
  - autumn: Harvest Home, the Gathering, the Last Sheaf;
  - winter: Midwinter, the Long Night, the Hearth Feast.
- Now: game day about 65.7, Spring day 66 of Year 1. The spring festivals (Seedtide, the Blossom Fair, the Greening) have already happened.
- Story trigger enums (`tools/story_triggers.py`):
  - calendar: weekday, season (spring, summer, autumn, winter), period (dawn, day, dusk, night);
  - weather: clear, overcast, rain, storm, fog, snow, sandstorm.
- Regional weather: rain (Ridgemere, Rainward Hills, Rainwood, Cedar Sound, Western Sea); snow (Whitefangs, Greyteeth); clear (Greenholt, Larkdowns, Sunfields); fog (Sallow Fens); sandstorm (Ochre Hills, Red Waste, Saltpan).

## 4. Geography and ids
- The map is 16 × 16 cells of 256 tiles, 4096 tiles square. 241 cells are kept (the south-western open sea is left off).
- Cell id `w_ridgemere_<cx>_<cy>`, with cx and cy 0–15. A world tile (X, Y) is in cell (X//256, Y//256) at local (X%256, Y%256).
- Story regions are rectangles of local tiles in one place, meaning one cell or one interior.
- `world.cells.region` is the **territory** region: ridgemere 9, rainward_hills 11, rainwood 17, cedar_sound 7, western_sea 6, whitefangs 30, greyteeth 6, greenholt 32, larkdowns 29, sunfields 12, ochre_hills 19, sallow_fens 23, red_waste 34, saltpan 6.
- `world.interiors.region` is the **community** (the "town"): ridgemere, coldwater, larkfield, southmeadow, fenshore, saltwell, oakhaven, frosthollow, and mirebridge (remnant).
- Story "town" ids, which also form `board:<town>` and the purse `town:<town>`: **ridgemere, coldwater, larkfield, southmeadow, fenshore, saltwell, oakhaven, frosthollow**.
- Talk `here.region` adds "wild".
- **Pitfall: only 585 of 1000 interiors are reachable.** Unreachable ones include:
  - every `tn_*` interior (old town remnants, linked only by their own stairs);
  - all 27 Mirebridge interiors;
  - unplaced farm and bunkhouse stock, and many `rm_*` houses;
  - `rm_the_gaol` (the cells are `r2_the_keep_down`, reached through the Watch Office), `rm_the_rain_hall`, `rm_the_city_arsenal`, `rm_the_heights_watch_post`, `rm_the_rookery`, `rm_the_hulks`, `rm_the_warrens`, `rm_the_sinks`, `rm_the_leaks`, `rm_the_hollows`, `rm_the_salt_pans`, `rm_the_wagon_yard`.
- Every NPC's home and work place is reachable.
- **Pitfall: id is not name.** Several interiors kept their old ids after renames:

| id | name |
|---|---|
| `rm_the_grayrock_ironworks` | The Timberland Ironworks |
| `rm_the_grayrock_foundry` | The Timberland Foundry |
| `rm_the_grayrock_iron_store` | The Timberland Iron Store |
| `rm_the_fell_sawmill` | The Timberland Sawmill |
| `rm_the_fell_timber_store` | The Grayrock Long Store |
| `rm_mossgrave_court` | Timberland Court |
| `rm_the_harbourmaster_s_office` | The Grayrock Wharf Office |
| `rm_the_hall_of_records` | The Couriers' Office |
| `r2_the_keep` | The Watch Office |
| `rm_the_cough_jar` | The Honey Crock |
| `rm_oilcloth_wool` | The Apple Loft |
| `rm_the_harness_shop` | The Mortar and Pestle |
| `rm_southgate_farriers` | Eastgate Farriers |

  The faction `house_mossgrave` is House Timberland.

### The regions (doc 70)
- **Ridgemere** (north-west corner, rain): the city plus cleared ground and fir woods.
- **Rainward Hills**: wet fir and cedar hills and Rainmere lake. Granite quarry `w_ridgemere_4_1`; slate quarry, a reserved site at (1316, 660) that may be unbuilt.
- **Rainwood**: old cedar and fir rain forest, ferny ravines.
- **Cedar Sound**: a sea arm 900 tiles long; Oakhaven at its head.
- **Whitefangs** and **Greyteeth** (north and north-east): snow peaks; tarns Highmere, Slate Tarn, Kestrel Tarn and Greytarn; the Pass north to the map's edge.
- **Mines**:
  - Frosthollow iron, `w_ridgemere_9_2` (212, 213);
  - Kestrel lead and silver, `w_ridgemere_8_2`;
  - Greyteeth iron `w_ridgemere_13_4` and Greyteeth silver, both reserved;
  - Greyteeth tin, `w_ridgemere_14_5` cell, reserved;
  - Ochre copper `w_ridgemere_11_9`, a spare.
- **Greenholt** (middle, temperate): meadow and woods; Glasswater, the great lake; Alder Mere; the lime quarry `w_ridgemere_6_3`.
- **Larkdowns**: dry grass swells; the Upper Mere.
- **Sunfields**: warm grain, orchards and vines.
- **Ochre Hills**: red sandstone, arid; Dustmere; the sandstone quarry `w_ridgemere_11_11`.
- **Sallow Fens** (south-west, fog): reeds, bog, willow carr, mist, the delta.
- **Red Waste** (south-east, sandstorm): dunes, mesas, cactus; Saltwell oasis.
- **Saltpan**: a dry salt lake round a brine pool; Saltpan workings `w_ridgemere_13_12`.
- **Rivers**: the Long River (Greyteeth → Upper Mere → Glasswater → Larkfield → fens → sea, 7000 tiles), the Alder, the Lark, Greybeck, Shiver, Kestrel, Rainrun, Coldbeck, Whitewater, plus 4 dry washes. Deep water `W` is impassable except by bridge.
- **Roads** (3 wide, bridged): North Road, which leads to the Pass Road (Frosthollow) and the Pass (north to the edge); East Road (Fellhall → Coldwater); South Road; Cinder Road (to Timberland Court); Wharf Road (to Grayrock Hall); Sound Road (to Oakhaven); Coast, River, Fen, High, Waste and Downs Roads between the towns.
- **Walks from the spawn**:

| Destination | Tiles |
|---|---|
| Oakhaven | 1,546 |
| Frosthollow | 3,252 |
| Coldwater | 3,184 |
| Larkfield | 4,046 |
| Southmeadow | 5,532 |
| Saltwell | 5,648 |
| Mirebridge crossroads | 3,678 |

- **Mirebridge** is an erased village: the crossroads in the open at world (1468, 2868), `w_ridgemere_5_11` local (188, 52), with no buildings. Its 27 `tn_mb_*` interiors are unreachable. It still has a dialect (used by Fenshore) and legend in lore fragment `mb-fens-1`. It makes a good ruin or legend site.
- **Ships** (port only at Ridgemere): the Fair Wind, the Grey Tern, the Cormorant, the Long Tide.
- **"The Wharf"** is also the abstract outside market, run from the Grayrock Wharf Office.

## 5. Ridgemere (city): quarters, landmarks, ids
- Cells:
  - `w_ridgemere_0_0`: North Headland, the abbey, the north wharf;
  - `1_0`: Northgate and the Greatly compound;
  - `0_1`: the Wharf, the Sump and the Cinders;
  - `1_1`: the Commons, the admin sector, the Palace, the Keep and the Rise;
  - `2_1`: Fellhall;
  - `0_2`: Grayrock Hall;
  - `1_2`: Timberland Court;
  - `2_0` and `2_2`: Rainward Woods.
- The walls enclose world tiles about 168–471 × 150–455, all level at height 2 since the Ridgemere 3 flattening.
- The spawn is on Market Square at `w_ridgemere_1_1` (104, 56).
- Gates, approximate local positions from doc 67's dense layout:
  - North Gate: `1_0` about (104, 150);
  - East Gate: `1_1` about (212, 44);
  - South Gate: `1_1` about (156, 196);
  - Cinder Gate: `0_1` about (240, 196);
  - Harbour Gate: `0_1` about (190, 196).
- Talk quarter values and resident voices:

| Quarter | Voices | What it is |
|---|---|---|
| commons | 212 | middle class: Market Square, the shops |
| sump | 179 | slums by the wharf: alleys, snickets, courts, the Rag Market, three-family tenements |
| heights | 104 | the Rise: Minor-House mansions under the South Gate |
| estate | 51 | the Houses' estates |
| works | 32 | the Cinders: heavy industry, south-west by the slipway |
| office | 32 | the admin sector: Records Square |
| wharf | 12 | the quay |

  Facts.json still lists the legacy values docs, harbour_hill and suburbs.
- Shared backstory threads already seeded across residents. These are hooks; the count is how many residents carry each:
  - "the Houses will fall out with each other before the next winter" (29);
  - "owes a House factor money... by the new moon" (34);
  - "carries a letter meant for Lord Grayrock, who is never home" (28);
  - "keeps a secret list of which Houses pay the Watch to look away" (26);
  - "lost a brother on the wharf, does not forgive House Grayrock" (38);
  - "saw the Chancellor weep in the Rain Hall" (38);
  - "petition lost in the City Office" (25);
  - "saw something big move under the ice" (7);
  - "the marsh lights are the drowned" (7).
- Door links, written `interior: cell (x, y)`:
  - **Crown and government**:
    - The Antechamber (the court and Council seat), `r2_the_antechamber`: `1_1` (80, 146);
    - King's Residence, `r2_the_king_s_residence` with `_up` and `_down`: `1_1` (51, 146);
    - Administrative Wing, `r2_the_administrative_wing`: `1_1` (109, 146);
    - Records Office, `r2_the_records_office` with `_up`: `1_1` (74, 108);
    - City Office, `rm_the_city_office`: `1_1` (16, 114);
    - Magistrate's Court, `rm_the_magistrate_s_court`: `1_1` (40, 118);
    - Couriers' Office, `rm_the_hall_of_records`: `1_1` (38, 114);
    - Council Press, `rm_the_council_press`: `1_1` (93, 59);
    - Market Hall, `r2_the_market_hall`: `1_1` (84, 16) and (123, 16).
  - **Watch**:
    - Watch Office, `r2_the_keep` (cells below in `r2_the_keep_down`, captain upstairs in `_up`): `1_1` (148, 82);
    - Watch House (barracks), `rm_the_watch_house`: `1_1` (164, 83);
    - Pillory Square (gallows, pillory, stocks): about world (412, 311).
  - **Church**:
    - Cathedral of Ridgemere, `r2_the_cathedral_of_ridgemere`, with crypt `r2_the_cathedral_of_ridgemere_crypt`: `0_0` (231, 175);
    - Chapel of the Tide (Ridgemere's own sea rite), `rm_the_chapel_of_the_tide`: `0_1` (210, 11);
    - Abbey Infirmary `r2_the_abbey_infirmary` `0_0` (228, 180), plus `r2_the_abbey_school`, `r2_abbey_quarters`, `r2_the_refectory`, `r2_the_scriptorium` and `r2_the_abbey_brewhouse`.
  - **Wharf and Sump**:
    - Customs House (the Crown's), `r2_the_customs_house`: `0_1` (192, 132);
    - Grayrock Wharf Office: `0_1` (195, 94);
    - Grayrock New Store, `r2_the_grayrock_new_store`;
    - Ropewalk, `rm_the_ropewalk`: `0_1` (192, 160);
    - Fish Hall, `rm_the_fish_hall`: `0_0` (205, 216);
    - Knucklebones (inside the hulk "the Mercy"), `r2_the_knucklebones`: `0_1` (210, 67);
    - Sailors' Rest, `rm_the_sailors_rest`: `0_0` (187, 227);
    - Soup Kitchen `rm_the_soup_kitchen` `0_1` (253, 69); Sump Infirmary `rm_the_sump_infirmary` `0_0` (249, 250); The Bilge `rm_the_bilge` `1_1` (22, 24); Pawn and Ledger `rm_the_pawn_and_ledger` `1_1` (22, 78);
    - Sunken Street (the old buried street level, z −1), `r2_the_sunken_street`: `1_1` (17, 44) and (62, 67);
    - Tallow Arcade, `r2_the_tallow_arcade`: `1_1` (65, 13) and (77, 13).
  - **Landmarks**, world coordinates from `artifacts/ridgemere2/city-report.json`:
    - Bay Light and the Mole, `r2_the_bay_light`: door `0_0` (71, 179);
    - Clock Tower, `r2_the_clock_tower`: `1_0` (113, 181);
    - the Wall-Builders statue on Greatly Square: (438, 231);
    - Cisterns and the Runnel on Cistern Court: (270, 278);
    - the Hundred Steps: (262, 230).
  - **Houses**:
    - Greatly House, `rm_greatly_house`: `1_0` (174, 189);
    - Greatly Forge & Workshop, `r2_the_greatly_forge_workshop`: `1_0` (152, 170);
    - Greatly Bank, `rm_the_greatly_bank`: `1_1` (159, 19);
    - Fellhall, `rm_fellhall`: `2_1` (150, 70);
    - Fell Distillery, `r2_the_fell_distillery`: `2_1` (92, 58);
    - Fell Wine Vaults, `r2_the_fell_wine_vaults`: `1_1` (136, 6);
    - Grayrock Hall, `rm_grayrock_hall`: `0_2` (182, 205);
    - Timberland Court, `rm_mossgrave_court`: `1_2` (102, 72);
    - Timberland Ironworks and Foundry: `0_1` (238, 170) and (238, 181).
  - **Other**: Carters' Yard, `r2_the_carters_yard`: `1_1` (177, 34); Grey Gull Inn, `rm_the_grey_gull_inn`: `1_1` (154, 41).
- A door's story id is `link_<world.links.id>_a` or `_b`. For example, `rm_the_chapel_of_the_tide_door` gives `link_rm_the_chapel_of_the_tide_door_a`.
- Patrol routes (`live.patrol_routes`): `r2_palace_round`, `r2_sump_patrol`, `rm_grayrock_journey` ("Lord Grayrock's road"), `rm_street_patrol`, `rm_wall_walk`, `rm_wharf_patrol`. The town routes `tn_*_watch` have no posts, and the towns have no placed guards.

## 6. Factions, Houses and power
- **Factions** (`live.factions`): `council_of_houses`, `ridgemere_watch`, `house_greatly` (blue), `house_fell` (green), `house_grayrock` (grey), `house_mossgrave` (House Timberland, brown; the talk scene `rg_rm_colours` names the colours).
- Every description is empty. `live.faction_relations` has 0 rows, and `live.faction_members` has 0.
- Claims: 615 rows, mostly the Council's over the city.
- There is no church, Concord, Warden, outlaw, Sump-gang or Crown faction.
- There is no ownership mechanic: a House shows only through names, work labels and claims.
- The Houses, with resident ids:
  - **Greatly**: construction (built the walls); stone, bank, Counting House, masons, lime, tiles, plaster.
    - Ambrose Greatly `rm_ambrose_greatly`, 63: made Greatly first through loans no one could refuse, and means his heir to inherit a city.
    - Celestine Greatly `rm_celestine_greatly`: hosts the salon every House attends and remembers everything.
    - Julian Greatly `rm_julian_greatly`, 29: wants to go to sea; drinks at the Sailors' Rest.
    - Steward Edric Kelpham `rm_edric_kelpham`.
  - **Fell**: wines, spirits and fruit (press house, distillery, preserving house, cellars, vaults).
    - Isolde Fell `rm_isolde_fell_`, rules the House: widowed when her husband drowned bringing a wine barge down the coast; never lets a debt go.
    - Corwin Fell `rm_corwin_fell_`: wants the Council to give Fell all the Rainward Hills.
    - Gudrun Fell `rm_gudrun_fell_`: a Timberland cousin who writes to them secretly.
    - Steward Millicent Rustwell `rm_millicent_rustwell`.
  - **Grayrock**: owns the Wharf (shipwrights, wharf office, stores, cranes; the outside trade).
    - Lord Aldric Grayrock `rm_aldric_grayrock_`: always on the road; trusts contracts more than people.
    - Lady Maren Grayrock `rm_maren_grayrock_`: never leaves the Hall.
    - Ingrid Grayrock `rm_ingrid_grayrock_`, 16: writes to her father every week.
    - Steward Lavinia Plankwick `rm_lavinia_plankwick`.
  - **Timberland**: the forges, ironworks, foundry, sawmill, charcoal, nailers, anchor forge, wool.
    - Signe Timberland `rm_signe_mossgrave`: holds the Council seat.
    - Halvard Timberland `rm_halvard_mossgrave`, 66: every Cinders smith would lie for him.
    - Steward Hollis Nailwick `rm_hollis_nailwick`.
- **The Crown**:
  - King Gunnar Harrowgate `rm_gunnar_harrowgate`, Sump-born, **died of old age on day 41** (`alive=false`).
  - Queen Oda Harrowgate `rm_oda_harrowgate`, 60, counts the purse coin by coin.
  - Princess Ottilie Harrowgate `rm_ottilie_harrowgate`, 39, coughs in the damp and carries the Crown's petitions.
  - **Emergent oddity:** on day 46 the vacancy rule gave "reigning at the Great Palace" to **Dagfinn Thornham `rmx_dagfinn_thornham`**, a Watch guard on Sump patrol (event `succession`; resident memory "took over the work of reigning"). His `live.npcs` work label still says "on patrol in the Sump". Either write it in on purpose (a commoner on the throne) or have the DM fix it.
  - Treasurer Karin Stanmore `rm_karin_stanmore` keeps a secret list of Houses that pay the Watch.
  - Customs collector Jasper Bilgeford `rm_jasper_bilgeford`.
- **Government**:
  - Chancellor of Ridgemere Oskar Gullworth `rm_oskar_gullworth` (City Office), Sump-born and climbing.
  - Chancellor of the Council Gerrit Mudhand `rm_gerrit_mudhand` (Antechamber), saw the Chancellor weep.
  - Harbourmaster Dagny Rustcot `rm_dagny_rustcot`.
  - Market Hall warden Fenwick Mossdale `rmx_fenwick_mossdale`.
- **Watch**:
  - Captain Corra Slagmore `rm_corra_slagmore`: **owes a House factor money by the new moon**.
  - Sergeant Bettany Nailley `rm_bettany_nailley`: hates Grayrock.
  - Sergeant Dagny Ropeley `rm_dagny_ropeley`: carries a letter for Lord Grayrock.
  - Weapon racks: Brand Mudridge `rm_brand_mudridge`.
- **Church**: Abbot Lionel Ropestack `rm_lionel_ropestack` (secret list of Watch bribes); priests Agnes Wetley `rm_agnes_wetley` and Aldric Fogwell `rm_aldric_fogwell`; Solveig Tarwell `rm_solveig_tarwell` of the Chapel of the Tide.
- **No resident has `protected=true`** (0 of 1086), so the `resident killed` effect could kill any of these. Protect the key cast first.
- **Figures** (`live.figures`, currently EMPTY):
  - fields: id, name, title, role, known_in, about, npc_id;
  - role is one of ruler, lord, captain, high_priest, hero, villain;
  - `known_in` is `realm`, `region:<x>` or `house:<x>`;
  - saved through DM `save_figure`, then `voice.sync`.
  - Existing talk lines in `Data/Talk/rules/town7_people.talk` already use `#world.figure_hero#` and `#world.figure_villain#` ("They say #world.figure_villain# has been seen on the roads"). Filling this table makes them play at once, which suits a campaign's hero or villain.

## 7. The seven towns
Each town has a square with its notice board `board:<town>`, where wagons load, at the place given below. All are towns with `town=true`, and there are no chapels outside the city.

### Coldwater
- Dialect: `coldwater`. Calm lake fishers: greet "Still water", farewell "Fair winds" or "Calm water", "It'll float", "fair catch", "minnow" for a child, oath "Net and line" or "Bright One's tide", tag ", eh?".
- Character: the lakeside village of the Greenholt where the East, River, High and Downs Roads meet, north of Glasswater. Cobbled crossroads and market blocks.
- 68 residents. Square: `w_ridgemere_7_7` (164, 4).
- Places:
  - Coldwater Stores `cn_coldwater_store`, kept by Myrtle Stillford `tn_cw_myrtle_stillford`;
  - The Glasswater Inn `cn_coldwater_inn`, kept by Loam Pinefell `tn_fh_loam_pinefell`;
  - Bakehouse `cn_coldwater_bakehouse`, Lichen Hedgemead;
  - Boiling House (soap) `cn_coldwater_soapworks`;
  - Loom Shed `cn_coldwater_weavers`, Fallow Lakewick;
  - Still `cn_coldwater_still`, Linden Harvestton.
- Farms: Greenholt and Meresend orchards; Oakhurst and Alder piggeries; Larkdowns and Greyteeth folds; Glasswater and Upper Mere landings.

### Larkfield
- Dialect: `larkfield`. Quick trader's speech: "Fair price to you", "Sold", "short-weighed", "lambkin", oath "Scales and silver" or "By the market cross".
- Character: the Long River crossing among the grain. A long flagstone market street and grain lanes.
- 36 residents. Square: `w_ridgemere_6_9` (140, 76).
- Places:
  - Stores `cn_larkfield_store`, Pike Chalkley;
  - The Sheaf `cn_larkfield_alehouse`, Bramble Reedford;
  - Bakehouse;
  - The Long River Inn `cn_larkfield_guest_inn`;
  - Linen Shed `cn_larkfield_weavers`, Aspen Marshholm;
  - Press and Paper Mill `cn_larkfield_press`, Dun Harvestley;
  - Smithy `cn_larkfield_smithy`, Tansy Eelbridge.
- Farms: Longmead Farm and Mill (the granary `fs_grain_long_granary`); Plumtree Orchard; Southwood Piggery; the Greenholt charcoal hearths.

### Southmeadow
- Dialect: `southmeadow`. Cheerful: "Good harvest", "Ripe days", "chaff", "a windfall", "my apple", oath "Sheaf and sickle" or "Blight take it".
- Character: a Sunfields hamlet of limewash and red tile. Diagonal vine lanes.
- 19 residents. Square: `w_ridgemere_8_13` (100, 20).
- Places:
  - Stores, Rusk Plummeadow;
  - The Vine and Sickle `cn_southmeadow_alehouse`, Rowan Downridge;
  - The Sunfield Inn;
  - Pottery `cn_southmeadow_pottery`, Orrin Appleley.
- Farms: Larkholm Farm and Mill; Southmeadow Vineyard with its press and cellar; Sunfields Bee Garth, Aster Cornley; the Sallow clay pits.

### Fenshore
- Dialect: speaks **mirebridge** (`settings.json` `dialect_of`). Wry and damp: "Dry feet" for hello and goodbye, "Fen-sure", "boggy", "tadpole", "mizzle", oath "Mud and eels" or "Dark One's mire", tag ", no?".
- Character: thatch on stilts and boardwalks on a dry island in the Sallow Fens. Branching plank boardwalks.
- 24 residents. Square: `w_ridgemere_3_13` (92, 148).
- Places:
  - Stores, Roan Pikewater;
  - The Heron `cn_fenshore_alehouse`, Nell Plummeadow;
  - The Reed and Ferry inn;
  - Pottery (its potter Iris Greywater died on day 27).
- Farms and works: Dune Warren; Withy Creek eel marsh `fs_marsh_eels_eelhouse`; Rivermouth and Fen Shore landings; Fenside Salterns; peat cutting; the Mirebridge clay pits.

### Saltwell
- Dialect: `saltwell`, the oasis. A formal hospitality voice:
  - "Peace on you", "Water and shade to you", "Go in peace", "May your well never fail", "honoured guest", "my brother" or "my sister";
  - oath "By salt and water" or "By bread and salt";
  - proverbs such as "The well remembers who drew from it";
  - "brackish" for bad, "mirage-talk" for nonsense;
  - borrowed words only "wadi" and "Baba"; no real-world religion.
- Character: adobe round a spring pool in the Red Waste, on the way to the Saltpan. An oasis crescent and caravan arc.
- 13 residents. Square: `w_ridgemere_12_11` (220, 116).
- Places: Stores, Thistle Stonepick; The Oasis Inn `cn_saltwell_inn`, Oriel Marshditch.
- Farms: Ochre Vineyard with its press and cellar; Redsand Warren, Lavinia Mudworth; Saltpan Workings `fs_salt_saltpan_store`.

### Oakhaven
- Dialect: `oakhaven`. Slow and warm: "Shade on you", "Root well", "sapling", "windthrown", "good bark", oath "Bark and branch", "Old Oak" or "Root and rain", tag ", mm?".
- Character: a feller village at the head of Cedar Sound. Staggered woodland lanes.
- 8 residents. Square: `w_ridgemere_3_4` (244, 196).
- Places: Stores, Cairn Oakhaven; The Felled Cedar, Tamsin Icefell; The Sound Road Inn; Press and Paper Mill, Moss Blossomwell.
- Works: the Oakhaven cut `w_ridgemere_4_4`.

### Frosthollow
- Dialect: `frosthollow`. Short, dry, dark: "Warm hearth" (broad: "'Arth"), "Keep your lamp lit", "deep" for very, "dead rock", "a good seam", "whelp", oath "Black seam", "Cold iron" or "Lamp and pick".
- Character: a mining village in the high valley under the Whitefangs pass, where the Pass Road, the Pass and the High Road meet. A straight pass road with miners' spurs.
- 9 residents. Square: `w_ridgemere_9_3` (4, 28).
- Places:
  - Stores, Aster Coldfell;
  - The Lamp and Pick, Thorn Ashwood;
  - The Pass House inn;
  - Mine Captain's House, for captain Fenwick Plankworth `cty_fenwick_plankworth`;
  - Limeburner's House and Collier's House.
- Works: the iron mine `w_ridgemere_9_2`, the lime quarry, the Greyteeth charcoal.

### Ridgemere (city)
- Dialect: `plain` (480 voices). Devout residents use "Bright One keep you", and "Blood and bread" is a common oath.
- About 15% of every town's people grew up elsewhere and keep their home accent: `self.from`, with accent none, light or broad.

## 8. Threats and conflicts already in the data
- **Bandit camps** are staged: actor `camp_<cell>`, with the events `bandits gather`, `raid` ("the wagon to Oakhaven was robbed"), `bandits scattered` ("starved out") and `camp cleared`. A camp's strength is at most 12 (`market.json campStrengthMost`) and a bounty at most 60 pennies.
- Where camps keep forming: `w_ridgemere_6_8` (Glasswater roads, 7), `1_3`, `7_12`, `6_12`, `5_4` (road to Oakhaven), `3_3`, `8_6` (Frosthollow road), `2_3`, `6_3`.
- The talk claims `bandits.camp` and `bandits.robs` already gossip about them.
- **Roads**: wagons held up ("mud to the axles", "a washed-out ford", "a wheel off in a rut", "a lame ox"); wagon and carter notices on the boards ("Escort wanted").
- **Crime**: thefts, attempted thefts, warrants (charges such as "attempted theft (inc-134)"), arrests, fines.
- **Staged market events**: blight in the orchards, flooded salt pans, a broken mill wheel, fire at the works, the drovers, a brewers' glut, rich cargoes, a House wedding. "Times" causes include "Silver struck in the hills".
- **Open town projects**: proj-1, a Frosthollow watch post in `w_ridgemere_8_6`; proj-2, an Oakhaven watch post in `w_ridgemere_5_4`; proj-3, a Ridgemere waystation in `w_ridgemere_1_1`. Each needs stone and timber, and none has been worked yet.
- **Deaths**: King Gunnar (day 41), Iris Greywater (day 27), Aud Kemp (day 56). Births are matched to deaths.
- **Political tension** already written into backstories: Greatly's debts and the factors' new-moon deadline (even the Watch Captain and the Greatly steward owe), Watch bribery lists, Lord Grayrock always absent, Fell's grab for the Rainward Hills, Gudrun's Timberland letters, the Houses expected to fall out before winter, a vacant throne held by a Sump guard, a Sump-born Chancellor.
- **Talk issues** that already sway residents (`opinions.json`): watch, houses, dole, newcomers, church, merchants, city, road, festivals, drink, times, sump, strangers.

## 9. Talk data an author must match
- `Data/Talk/facts.json`: the namespaces self/other/a/b, here, topic, world, conv, pair, x and player. Values to match:
  - `self.from` and `topic.accent_of`: the 8 towns plus mirebridge;
  - `self.dialect`: plain, coldwater, frosthollow, larkfield, mirebridge, oakhaven, southmeadow, saltwell;
  - `topic.kind`, which includes succession, raided, camp and death;
  - `topic.claim_kind`: death, marriage, violence, theft, victim, wanted, bandit, promise_kept, promise_broken, prowling, newcomer, other;
  - `world.*` prices, bandits, crime, festival, `figure_*`, ships and wagons.
- Topics (`topics.json`): gossip, bandits (aspects where, fear and watch), deed, news, quarrel, event, times, crime, newcomer, lore, faith, land, travel and others.
- Claims (`claims.json` kinds): death, marriage, theft(.attempt/.unseen), assault(.unseen), violent, warrant, promise.broken/.kept, prowling, newcomer, bandits.camp, bandits.robs.
- Truth rules (truth.json): weather words need the weather; "my wife" needs married; pity and gladness must match the news kind.
- Grammars are `Data/Talk/grammar/<dialect>.grammar`, 237–481 entries each. Every symbol must exist in `plain`. Layers stack own → quarter → dialect → plain. `!` marks a habit, `.broad` a broad form, `^m`/`^f` a sex-only choice.
- Regional scene files: `Data/Talk/scenes/regions/{ridgemere,ridgemere_lore,ridgemere_wharf,coldwater,larkfield,larkfield_downs,southmeadow,mirebridge,oakhaven,frosthollow,saltwell,towns,towns2,cities,land7_regions,...}.talk`.
- Dialogue trees: `Data/Talk/dialogue/{church,companion,inn,master,official,receiver,resident,shop,watch,work}.tree`.
- Lore fragments (`Data/Lore/fragments.json`) are shown after archive work at the Records Office: the regions ridgemere, frosthollow, coldwater, oakhaven, mirebridge, larkfield and southmeadow, plus shared ones on the old count, the Gifts, roads, pennies, howling, names, hard winters, Chapters and the wild.

## 10. Pitfalls for loadable content
1. Use only reachable interiors: `/tmp/lore_survey/reachable.txt`, or check `world.links` outward from the cells. Never use `tn_*` or Mirebridge interiors.
2. Resident, place and faction ids must exist in ridgemere3. `Data/Storylines/event-library.json` uses placeholder ids that do not exist here (`rm111`, `rm112`, `rm113`, `city_watch`, `road_company`), and the `Data/Storylines/programs/*` and `scenes/greyfen-oath.json` files belong to the Greyfen town world.
3. Any Warden, Concord, church, outlaw or Crown faction a story needs, and any faction relations, must be created first.
4. Mark the key cast `protected` before using kill effects.
5. Town residents are few: Oakhaven has 8 and Frosthollow 9. Prefer visitors (`story_visitors`, empty) or city NPCs for big casts.
6. Fill `live.figures` for the hero, villain and ruler, and decide what to do about the Dagfinn Thornham "reigning" post.
