# Ridgemere3 map and playtest direction

Ridgemere3 is the active development world for a console-style character map, reminiscent of old MUD and MUSH clients. Repeat monospaced characters to make terrain textures (`:` for moss, punctuation for grass), visible blocked trunk footprints (`||`), layered canopies (`^`), walls (`#`) and roofs (`/` or `=`). Colour and dark tile backgrounds may support the text, but painted rectangles, geometric trunks and roof illustrations must not become the objects themselves. Do not commission or generate raster artwork for this map pass. A tree has a solid trunk tile and a canopy above it; a building has a solid footprint and an overhead roof. Keep the player's amber `[W]` visible behind overhead layers. Preserve room interiors and their doors while making exterior footprints legible.

## Playtest observations, 2026-11-14

- The minimap switches between World Map and Local Map.
- Outdoor cells must feel like one route: a held direction persists across a seam, the camera eases through it, and a click on visible ground in the next cell routes through a valid seam to the clicked tile. Unknown ground should explain why it cannot be reached.
- Show resident names immediately. Player names still depend on introductions.
- NPCs must answer the current utterance. A player introduction gets acknowledged; an authored fallback must not restart the doorstep greeting. Ambient exchanges and barks give an active player conversation room.
- Ridgemere3's hosted launcher must start the configured NPC Mind when enabled. An offline launcher was the main cause of repetitive, generic answers in this playtest.
- Tracking is an explicit local map state: scent reveals `∴` trails, the marks become easier to read, and HUNT starts the existing encounter. Sitting is optional; smelling works without it.

## Town content pass still needed

Ridgemere3 already contains the six authored towns Frosthollow, Coldwater, Oakhaven, Mirebridge, Larkfield and Southmeadow, with about 19–22 interior areas each. Their existing world data is more than a name, but the playtest says their streets and density do not yet read as towns. Build them out **in Ridgemere3** with additional distinct building footprints, public destinations and street activity. Audit each town on the local map and at walking scale before adding content, then keep shops, homes, routes, residents and doors aligned. The town pass should use the same console character language as the rest of the map.
