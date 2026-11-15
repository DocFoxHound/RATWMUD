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

## Town content pass still needed

Ridgemere3 already contains the six authored towns Frosthollow, Coldwater, Oakhaven, Mirebridge, Larkfield and Southmeadow, with about 19–22 interior areas each. Their existing world data is more than a name, but the playtest says their streets and density do not yet read as towns. Build them out **in Ridgemere3** with additional distinct building footprints, public destinations and street activity. Audit each town on the local map and at walking scale before adding content, then keep shops, homes, routes, residents and doors aligned. The town pass should use the same console character language as the rest of the map.

## Street and access pass, 2026-11-15

Ridgemere3 revision 101 / build 76 levels 63,608 built city tiles to a shared street height. The coast, waterways, transition hills, and Palace remain distinct. Of the previous 735 stair glyphs in the four central cells, 715 routine street stairs were replaced with cobbles or flagstones, and six neutral stone stair tiles now mark the three Palace approaches. The 20 remaining old stairs lie on the eastern hillside outside the built town. The Sump and its adjoining fringe lost 786 timber fence tiles. Small doorless roof sheds became rubble, while 26 larger private frontages use a solid `⊠` character, distinct from working `+` doors. The revision-checked edit is reproducible with `tools/refresh_ridgemere3_streets.py`; the pre-edit source snapshots were saved in `/tmp/ridgemere3-before-streets-rev99.json.gz` and `rev100.json.gz` on the development machine.

Forty `†` street lamps are authored as one-character solid tiles. Their nearby known ground is warmer at night. Stairs use stone colors and carry no light. The map remains character and block based.

Ridgemere3 exterior doors now expose their real access state: private interiors are locked to players, merchant workplaces open during the merchant's hours, and inns, alehouses, taverns, and taprooms stay open all day and night. A player inside can always leave when the shift ends. NPCs retain their home and work access. Click movement, direct interaction, walking through an open door, pending transitions, and planned travel all check the same player access rule. The client marks locked doors with `×`; sealed frontages use `⊠` and have no interior.

The outdoor client now compares visible terrain content before rebuilding its offscreen ground, so fresh snapshot arrays no longer redraw a whole city view every movement update. It updates the walking grid's newly revealed terrain and door bytes in place instead of rebuilding all 256×256 tiles. Mild weather fronts use the cell tint; uniform heavy weather draws directly as a repeating character sheet. In a 3.5-second rainy night walk in the Commons after local walking had started, the browser used eight partial walking-grid updates, zero full reloads, one ground redraw, and about 0.9 ms median / 5.5 ms 99th-percentile frame work. An occasional frame interval still reached 33 ms in that sample.
