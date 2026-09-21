# Perception, local visibility, and permanent map memory

Status: implemented vertical-slice design, 2026-09-20.

## Contract

`World::snapshot(observerId)` constructs the allowed local/world view for one player. `World::perceive(observerId,sourceId,voice)` provides hearing clarity, visual clarity, and whether a source can be identified. The runtime converts these values into a listener-specific roleplay event before sending text. Perception is calculated from authoritative world state at the time an event occurs.

These APIs deliberately distinguish rendered location knowledge from present perception. A map outline cannot make a hidden actor visible. Previously visiting a cell never grants ongoing access to occupants or changes there.

## Local map

The snapshot contains exactly the observer's current stored cell. Each tile has a current-visibility bit and a remembered bit. Unknown tiles have a blank glyph and default terrain properties. Previously observed tiles may display their retained glyph dimly; their current terrain properties are omitted while hidden. Entities are included only when visually perceived. Included actor snapshots omit their private path and input intentions.

Ray sampling against opaque tiles and closed doors determines line of sight. Low tables and counters block movement but permit sight. Range includes vision sensitivity, eye health, and outdoor weather. Facing currently orients the marker and does not impose a field-of-view cone: a wolf can perceive around itself without constantly rotating. This remains a morning review item if directional vision is desired.

Ordinary visual action clarity is full over roughly half the reference span, then degrades to zero. The first slice's tuning uses a 27-tile maximum range with a 13.5-tile full-detail radius; these values are easily adjusted. Fog strongly reduces range, rain moderately reduces it, and snow falls between them.

## World-map states

| State | How earned | Appearance out of sight | Live contents |
| --- | --- | --- | --- |
| Unknown | Never directly seen | Absent | None |
| Glimpsed | Directly sees through an open adjacent portal | Coarse dim outline | None |
| Visited | Physically enters cell | Dim detail of individually observed terrain | None |
| Currently visible | Current line of sight through an open adjacent portal | Normal outline | Perception-limited, never global |

Knowledge permanently belongs to the character. There is no decay with elapsed time, logout, or restart. Maps, rumors, NPC directions, and lore cannot mark a cell seen. Physically entering a cell records the visit; it does not automatically expose every obstructed tile. Observation updates only what the character actually sees.

The active neighborhood includes the current cell and first-degree portal neighbors only. Previously visited remote locations remain stored but are absent from this view. Current visibility requires range, line of sight, and an open portal. A closed door hides the cell beyond while any already earned outline persists.

Out-of-sight records use remembered dimensions, world placement, and glyphs, not fresh terrain from the live cell. This makes stale knowledge possible. The slice stores a single record per known cell; a future builder system should version those records when topology changes.

## Vertical projection

The default world map is 2D. If two currently visible neighborhood cells share horizontal placement but differ in Z, the snapshot requests isometric mode. The client can tilt and separate glyph planes; its accessibility option retains 2D with above/below labels. A remembered vertical outline alone never activates isometric mode. Local movement and interactions remain on the overhead map of the current cell.

## Spatial hearing

Normal speech is fully clear through 16 unobstructed terrain units, approximately half the standard 32-tile cell. A linear falloff reaches zero at twice the clear radius. Whisper's clear radius starts at 2 units; yell starts at 32. Hearing sensitivity and ear health scale the radius. Complete hearing loss eliminates incoming speech, while the author still sees their own submitted post.

Walls/closed barriers attenuate sound. Outdoor rain and snow add noise. A yell can cross a first-degree acoustic portal; path length includes distance to the exit, distance from the destination anchor, a crossing cost, portal openness, and occlusion on each side. Normal speech and whisper remain within one cell in this initial slice.

`SensoryResult` contains numeric clarity, never text. The roleplay component masks missed words deterministically for each event/listener with `...`, gates visual actions using `···`, retains one post container, and omits entirely unperceived events. An audible but visually hidden source receives an anonymous voice label and its validated speaking color; the network payload must not include its name or stable entity ID.

## Persistence and verification

`CellMemory` records knowledge tier, cell identity and remembered topology, observed glyphs, and an observed mask. These records are included in `PersistedWorld`. Memory does not store live actor locations, voice events, fixture openness, or current activity.

Behavior tests verify unseen cell omission, coarse glimpse, entering to gain detail, hidden mutation remaining stale, year-later restoration, first-adjacency filtering, vision occlusion, injured senses, weather impact, hearing across portals, visible Z activation, remembered Z returning to 2D, and hidden entity omission.

## Prototype limits

The perception model is deliberately testable and approximate: sampled 2D line of sight, linear clarity curves, authored heights, and one portal hop for sound. It does not yet model diffraction around complex geometry, separate hearing frequencies, age curves, scent, directional visibility, illumination fields, translucent materials, or visible slivers of an adjacent floorplan. Adjacent cells currently show their known shape and remembered detail; they do not stitch live adjacent terrain into the local view.
