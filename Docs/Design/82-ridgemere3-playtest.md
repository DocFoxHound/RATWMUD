# Ridgemere3 map and playtest direction

Ridgemere3 is the active development world for a console-style character map, reminiscent of old MUD and MUSH clients. Repeat monospaced characters to make terrain textures (`:` for moss, punctuation for grass), visible blocked trunk footprints (`||`), layered canopies (`^`), walls (`#`) and roofs (`/` or `=`). Colour and dark tile backgrounds may support the text, but painted rectangles, geometric trunks and roof illustrations must not become the objects themselves. Do not commission or generate raster artwork for this map pass. A tree has a solid trunk tile and a canopy above it; a building has a solid footprint and an overhead roof. Keep the player's amber `[W]` visible behind overhead layers. Preserve room interiors and their doors while making exterior footprints legible.

## Visual audit, 2026-11-14

- Captured the actual client against a read-only Ridgemere3 scratch server (world build 74) in `w_ridgemere_1_1`, around local `(95–104, 56–59)`, at daylight and rainy night, with 22 and 28 pixels per tile. The amber `[W]` remains visible behind a tree canopy and a roof edge.
- Rendered actual build-74 terrain from `w_ridgemere_2_0` (Rainward Woods, North) at 24 pixels per tile, local `(120–149, 120–137)`, and at editor scale, 12 pixels per tile, local `(105–164, 111–146)`, using the shared character texture routines. This checks the map data and low-zoom fallback; it is not a screenshot of the Atlas UI.
- At player scale, dense canopies overlap each other and the moss beneath them. The blocked `||` trunk footprint becomes hard to locate, despite moss and trunks having different characters. The many `&` fern glyphs add a regular noise pattern.
- The `&` marks came from the fern and shrub catalog's ASCII fallback, which the first texture pass did not cover. Ferns now use `v` fronds and shrubs `*` clusters at player scale; their low-zoom and plain-glyph fallbacks use `v` and `*` too. This removes the ampersands, though dense foliage still needs a separate readability pass.
- At editor scale, blocked trees collapse to `T` and moss to `:`, which are distinguishable, but dense forest still lacks a clear passable route at a glance.
- In the city, broad roofs read as uninterrupted repeated `/` fields. Roof eaves and doors need a stronger building edge and entrance cue. Rainy night leaves the ground and roof characters faint against their backgrounds.
- Before changing these patterns, inspect a forest location and zoom levels in the actual Atlas UI and capture a player standing behind a canopy in the live forest client. The current forest capture is data-backed but does not include the complete app presentation.

## Playtest observations, 2026-11-14

- The minimap switches between World Map and Local Map.
- Outdoor cells must feel like one route: a held direction persists across a seam, the camera eases through it, and a click on visible ground in the next cell routes through a valid seam to the clicked tile. Unknown ground should explain why it cannot be reached.
- Show resident names immediately. Player names still depend on introductions.
- NPCs must answer the current utterance. A player introduction gets acknowledged; an authored fallback must not restart the doorstep greeting. Ambient exchanges and barks give an active player conversation room.
- Ridgemere3's hosted launcher must start the configured NPC Mind when enabled. An offline launcher was the main cause of repetitive, generic answers in this playtest.
- Tracking is an explicit local map state: scent reveals `∴` trails, the marks become easier to read, and HUNT starts the existing encounter. Sitting is optional; smelling works without it.

## The seven community towns (revision 104 / build 80)

The active communities are the `cn_` places: Coldwater, Larkfield, Southmeadow, Fenshore, Saltwell, Oakhaven and Frosthollow. The old `tn_` interiors and Mirebridge crossroads are historical remnants, not these settlements. Every active community already had streets, a store, an inn or alehouse, homes and 8–45 placed residents. The smaller places were still sparse, and `communities.json` marked all seven as plain regions rather than towns.

`tools/build_ridgemere3_towns.py` added 19 compact street-front houses and five guest inns with upstairs beds. Coldwater and Saltwell already had inns. Every new building has a character-tile footprint, an exterior door onto an existing street, an interior and the appropriate stair link. The original shops, occupied homes, residents and their dialogue records were left alone. The five new guest inns have no new staff posts yet; existing shop and alehouse staff remain at their old workplaces. The community contract now marks all seven as towns so `World::setupTowns` can include them on restart. The same pass runs in fresh realm generation.

Community exterior doors now use the city's player access rule: homes are private, staffed shops open during their merchants' working hours, and inns and alehouses stay open overnight. A guest inn remains open even before a new keeper is assigned. NPCs keep access to their homes and workplaces.

The map editor validated the revision 104 project; `path_check.py` now audits the actual seven `cn_` town bounds and found no blocked paths or doors in any of them. The pre-edit world is backed up at `Saved/Backups/ridgemere3-before-towns-rev103.json.gz` on the development machine. A walking-scale client review of each town, including signs and street activity, remains useful after the rebuilt world is loaded.

The server loaded build 80 after the updated door rule was compiled; build 79 was the first map build of revision 104.

### Distinct town layouts (revision 105 / build 81)

Each community now has a different character-map street plan and ground texture while keeping its existing buildings and doors:

| Town | Street plan and appearance |
| --- | --- |
| Coldwater | Cobbled crossroads, short market blocks and wildflower verges. |
| Larkfield | A long flagstone market street, grain lanes and crop-field edges. |
| Southmeadow | Diagonal vine lanes, red tile roofs and flowered dry grass. |
| Fenshore | Branching plank boardwalks through reeds and water. |
| Saltwell | A caravan arc and a lane following the oasis, with sandstone in the sand. |
| Oakhaven | Staggered woodland lanes with moss beside cedar roofs. |
| Frosthollow | A straight pass road, miners' spurs, scree and slate roofs. |

The pass uses existing terrain characters, with no raster artwork. It preserves water, trees, gardens, doors and the green's well. Two Southmeadow lamps moved to the verge when the new lanes crossed their old positions. Revision 104 is backed up at `Saved/Backups/ridgemere3-before-towns-rev104.json.gz` on the development machine.

## Street and access pass, 2026-11-15

Ridgemere3 revision 101 / build 76 levels 63,608 built city tiles to a shared street height. The coast, waterways, transition hills, and Palace remain distinct. Of the previous 735 stair glyphs in the four central cells, 715 routine street stairs were replaced with cobbles or flagstones, and six neutral stone stair tiles now mark the three Palace approaches. The 20 remaining old stairs lie on the eastern hillside outside the built town. The Sump and its adjoining fringe lost 786 timber fence tiles. Small doorless roof sheds became rubble, while 26 larger private frontages use a solid `⊠` character, distinct from working `+` doors. The revision-checked edit is reproducible with `tools/refresh_ridgemere3_streets.py`; the pre-edit source snapshots were saved in `/tmp/ridgemere3-before-streets-rev99.json.gz` and `rev100.json.gz` on the development machine.

Forty `†` street lamps are authored as one-character solid tiles. Their nearby known ground is warmer at night. Stairs use stone colors and carry no light. The map remains character and block based.

Ridgemere3 exterior doors now expose their real access state: private interiors are locked to players, merchant workplaces open during the merchant's hours, and inns, alehouses, taverns, and taprooms stay open all day and night. A player inside can always leave when the shift ends. NPCs retain their home and work access. Click movement, direct interaction, walking through an open door, pending transitions, and planned travel all check the same player access rule. The client marks locked doors with `×`; sealed frontages use `⊠` and have no interior.

The outdoor client now compares visible terrain content before rebuilding its offscreen ground, so fresh snapshot arrays no longer redraw a whole city view every movement update. It updates the walking grid's newly revealed terrain and door bytes in place instead of rebuilding all 256×256 tiles. Mild weather fronts use the cell tint; uniform heavy weather draws directly as a repeating character sheet. In a 3.5-second rainy night walk in the Commons after local walking had started, the browser used eight partial walking-grid updates, zero full reloads, one ground redraw, and about 0.9 ms median / 5.5 ms 99th-percentile frame work. An occasional frame interval still reached 33 ms in that sample.

## Flat city, open paths and lit streets, 2026-11-15

The playtest found an impassable north–south line through the Commons and the Rise (local x 159/160 of `w_ridgemere_1_1` and `w_ridgemere_1_0`: the street pass had levelled the city only west of it, leaving the Rise half a step to a step and a half higher with its stairs gone) and a barrel closing a path. Revision 102 / build 77 (`tools/refresh_ridgemere3_map.py`, re-runnable; the pre-edit snapshot is `/tmp/ridgemere3-before-map-rev101.json.gz` on the development machine) does three things:

- **One ground height inside the walls.** Every tile inside Ridgemere's wall (world tiles 168–471 × 150–455) but water is at height 2: the Rise, the Palace terrace and the quays included. Its 74 remaining stairs and slopes became paving, including the Palace approach; a later run of `refresh_ridgemere3_streets.py` would put six Palace stair tiles back, harmlessly. Each gate through the wall is graded in half steps to the ground outside (the quay gates down to the piers, the east gate up to the road), and the road leaving the Wharf's south gate, which dropped a step and a half outside the wall, is graded too. The Sunken Street's door onto the canal keeps the water's level. Interiors and their floors are rooms and were not touched.
- **Every path open.** `tools/path_check.py` (`python3 tools/path_check.py ridgemere3`; rules in `tools/test_path_check.py`) walks the city and the six towns tile by tile under Core's step rule, joining areas through their buildings' doors. It fails on a path or door that cannot be reached and that obstacles (props, fences, trees) close off, on an obstacle across a path with no open tile beside it and a long way round, and on a step too steep between two path tiles. A yard walled round with no door is only noted (21 in the city). Before the edit it found 401 steep path edges, two doors in the Rise (the Watch House, the Greatly forge workshop) beyond the line and the whole Rise cut off. After the flattening, 45 obstacle problems remained, all in the city: 16 closed paths and 26 small yards or alleys closed by fences or props, plus the south road's step. 22 props were carried aside, one crate was removed for lack of room, and 17 obstacles were cleared (15 fences and a tree among them). The towns had none. Mirebridge was erased in doc 72 and has no buildings, so only its crossroads is walked.
- **Street lamps.** There are 171 new `†` lamps beside Ridgemere's cobbled streets and wider lanes, added to the 40 already there, and 95 beside the roads within 40 tiles of each town's middle: Frosthollow 18, Coldwater 19, Oakhaven 18, Larkfield 21, Southmeadow 19. They stand about 16 tiles apart, on open ground with three open sides, never beside another prop, a door or a place the world names, and never closing a path. Glowing is the client's work.

### Lamps at the street's edge (revision 103 / build 78)

A lamp was found standing in the middle of a five-tile street south of Market Square (`w_ridgemere_1_1` 104, 66), square in the line of a walk north up it; the check passed it because an open tile was beside it. `tools/path_check.py` now also fails a **lamp in the way**: a lamp that interrupts a run of path tiles (along either axis) must stand in the street's edge row, against a building, a wall, water or the ground beyond the paving, and leave at least two open path tiles across the street beside it. A one- or two-tile lane gets its lamps beside it, off the paving, never in it. Props carried aside by the fix keep to the same rule.

`tools/refresh_ridgemere3_map.py` carries each lamp in the way to the nearest edge tile within 12 that still passes every placement rule and keeps 12 or more tiles from the other lamps, then lights any gap afresh at the usual 16. Applied as revision 103 / build 78 (pre-edit snapshot `Saved/Backups/ridgemere3-before-map-rev102.json.gz`): 108 lamps moved, 106 in the city (42 in the Commons and the Rise, Market Square's four among them, 32 in Northgate, 24 in the Wharf and the Sump, 8 on the Bay and North Headland) and one each in Oakhaven and Southmeadow, where the lamp stood in a two-tile road's row; two new lamps fill gaps in the city (213 there now). One barrel carried aside in revision 102 stood in a two-tile yard's row and was carried to its corner. The lamps' median spacing is 15.6 tiles (least 12). A second run changes nothing.
