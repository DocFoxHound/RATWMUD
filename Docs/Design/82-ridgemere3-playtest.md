# Ridgemere3 map and playtest direction

Ridgemere3 is the active development world for a console-style character map, reminiscent of old MUD and MUSH clients. Repeat monospaced characters to make terrain textures (`:` for moss, punctuation for grass), visible blocked trunk footprints (`||`), layered canopies (`^`), walls (`#`) and roofs (`/` or `=`). Colour and dark tile backgrounds may support the text, but painted rectangles, geometric trunks and roof illustrations must not become the objects themselves. Do not commission or generate raster artwork for this map pass. A tree has a solid trunk tile and a canopy above it; a building has a solid footprint and an overhead roof. Keep the player's amber `[W]` visible behind overhead layers. Preserve room interiors and their doors while making exterior footprints legible.

## Visual audit, 2026-11-14

- Captured the actual client against a read-only Ridgemere3 scratch server (world build 74) in `w_ridgemere_1_1`, around local `(95–104, 56–59)`, at daylight and rainy night, with 22 and 28 pixels per tile. The amber `[W]` remains visible behind a tree canopy and a roof edge.
- Rendered actual build-74 terrain from `w_ridgemere_2_0` (Rainward Woods, North) at 24 pixels per tile, local `(120–149, 120–137)`, and at editor scale, 12 pixels per tile, local `(105–164, 111–146)`, using the shared character texture routines. This checks the map data and low-zoom fallback; it is not a screenshot of the Atlas UI.
- At player scale, dense canopies overlap each other and the moss beneath them. The blocked `||` trunk footprint becomes hard to locate, despite moss and trunks having different characters. The many `&` fern glyphs add a regular noise pattern.
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
