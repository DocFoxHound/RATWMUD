# Perception, local visibility, and permanent map memory

Status: implemented vertical-slice design, updated 2026-09-21 for wind and scent.

## Contract

`World::snapshot(observerId)` constructs the allowed local/world view for one player. `World::perceive(observerId,sourceId,voice)` provides hearing clarity, visual clarity, scent clarity, and whether a source can be identified. The runtime converts hearing and vision into a listener-specific roleplay event before sending text; scent does not authorize otherwise unperceived speech or actions. Perception is calculated from authoritative world state at the time an event occurs.

These APIs deliberately distinguish rendered location knowledge from present perception. A map outline cannot make a hidden actor visible. Previously visiting a cell never grants ongoing access to occupants or changes there.

## Local map

The snapshot contains exactly the observer's current stored cell. Each tile has a current-visibility bit and a remembered bit. Unknown tiles have a blank glyph and default terrain properties. Previously observed tiles may display their retained glyph dimly; their current terrain properties are omitted while hidden. Entities are included only when visually perceived. Included actor snapshots omit their private path and input intentions.

Ray sampling against opaque tiles and closed doors determines line of sight. Low tables and counters block movement but permit sight. Range includes vision sensitivity, eye health, outdoor weather and shared daylight illumination. Facing currently orients the marker and does not impose a field-of-view cone: a wolf can perceive around itself without constantly rotating. This remains a morning review item if directional vision is desired.

Ordinary visual action clarity is full over roughly half the reference span, then degrades to zero. The first slice's tuning uses a 27-tile maximum range with a 13.5-tile full-detail radius; these values are easily adjusted. Fog strongly reduces range, rain moderately reduces it, and snow falls between them.

## Sneaking and visual detection

Moving from a lying posture prepares a crouch; crouching remains the stealth posture even after movement stops. Its smaller detection radius is calculated by the server, not by fading an already disclosed actor on the client. With sneak skill `S` clamped to 0–100, initial tuning is:

```text
close detection radius = (7 - 4*S/100) * (observer sight range / 27)
```

A novice croucher therefore appears only within 7 terrain units for an ordinary observer in clear conditions; at skill 100 the cutoff is 3 units. Observer vision, eye health, and weather scale those limits through the existing sight range. Walls and closed doors still block sight. Being inside the cutoff does not override ordinary visual clarity; being at or beyond it removes visual perception entirely.

The same decision gates actor snapshots, available actor actions, inspection, and visual roleplay segments. A hidden wolf's token, typing/speaking indicators, identity, position, posture, and current activity are absent from that observer's actor snapshot. Knowing a cell or guessing an entity ID does not grant inspection of its hidden occupants. There is no directional field of view or cover roll introduced by this change, and the distances are initial gameplay values rather than settled balance.

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

Normal speech is fully clear through 16 unobstructed terrain units, approximately half the standard 32-tile cell. A linear falloff reaches zero at twice the clear radius. Whisper's clear radius starts at 2 units; yell starts at 32. Hearing sensitivity, ear health, and hearing skill scale the radius by `hearing * earHealth * (1 + 0.75*hearingSkill/100)`, with skill validated to 0–100 and ear health to 0–1. Complete hearing loss eliminates incoming speech, while the author still sees their own submitted post.

Walls/closed barriers attenuate sound. Outdoor rain and snow add noise. A yell can cross a first-degree acoustic portal; path length includes distance to the exit, distance from the destination anchor, a crossing cost, portal openness, and occlusion on each side. Normal speech and whisper remain within one cell in this initial slice.

`SensoryResult` contains numeric clarity, never text. The roleplay component masks missed words deterministically for each event/listener with `...`, gates visual actions using `···`, retains one post container, and omits entirely unperceived events. An audible but visually hidden source receives an anonymous voice label and its validated speaking color; the network payload must not include its name or stable entity ID.

## Movement sounds

`World::movementAudibility(observerId,sourceId)` is separate from speech. It returns zero for a stationary source, a missing actor, or a source in another cell; turning in place and waiting to rise do not create footsteps. Initial clear radii are 6 terrain units for normal motion and `2.5 - 1.7*sneakSkill/100` for crouching motion (2.5 units at novice skill, 0.8 at skill 100). They use the same observer hearing/ear-health/skill multiplier as speech. Outdoor rain multiplies reach by 0.72, snow by 0.85, and a blocked line of sight by 0.38. Height difference adds to acoustic distance. Clarity falls linearly after the clear radius and reaches zero at twice that radius.

The runtime may deliver an anonymous nearby-pawstep cue when another player is moving, audible, and visually hidden. Cues are rate-limited to at most one per listener every three seconds and do not carry the source's name, stable actor ID, position, or a map token. NPC footstep cues are not emitted in this initial presentation, to avoid routine-driven feed spam. Hearing movement is evidence that something moved, not permission to visualize or identify it.

**Provisional interpretation pending user clarification:** sneaking reduces movement noise, not the player's deliberately selected whisper/speak/yell volume. A crouching wolf who speaks normally can still be heard at normal speech range; a yell remains a yell. Hearing skill affects both speech and movement detection. Automatic voice attenuation while sneaking remains an open product decision, not a silently imposed rule.

## Scent and independent senses

Sight, hearing, and smell are independent channels. A blind or deaf wolf can still scent another wolf; a wolf with no functioning sense of smell can still see or hear. `World::scentClarity(observerId,sourceId)` calculates current body scent for players and NPC wolves in the same stored cell. It excludes the observer's own scent. Sneaking does not erase body odor: sneak skill changes visual and movement-sound detection, not scent strength.

Observer sensitivity is `smell * noseHealth * (1 + 0.75*scentSkill/100)`. Nose health is validated to 0–1, scent skill to 0–100, and base smell sensitivity to a finite nonnegative value. A ruined nose or zero sensitivity prevents detection. These are implemented sensory hooks, not a complete injury, aging, or skill-training system.

Wind is represented as air flow: heading zero means air moving east, `pi/2` means south in the map's downward-positive Y coordinates. Thus eastward flow carries a western source's scent toward an eastern observer; the scent cue points roughly west/upwind, not east with the air. Crosswind and opposite-direction sources have much shorter reach.

Initial gameplay tuning is deliberately approximate:

```text
alignment = max(0, dot(source-to-observer unit vector, air-flow unit vector))
carry = wind strength * alignment²
full-clarity distance = (1 + 10*carry) * observer sensitivity * weather factor
maximum distance      = (3 + 30*carry) * observer sensitivity * weather factor
```

Clarity declines linearly between the two distances. Distances and air-route searches are capped at 64 terrain units. At ordinary sensitivity with clear weather and strength 0.5, a directly downwind observer gets full clarity through 6 units and decreasing clarity to 18. Calm or fully cross/upwind reach is full through 1 unit and decreases to zero at 3. Outdoor rain multiplies both distances by 0.65, snow by 0.80, and fog by a provisional 1.05 humidity bonus. Day/night does not change scent sensitivity. These values are tunable game rules, not claims about real wolf olfaction or atmospheric physics.

Scent needs a connected air path, not visual line of sight. Opaque walls and closed doors block air; low tables and counters do not. A bounded eight-neighbor shortest-path field permits scent to bend around an obstacle, forbids cutting diagonally through sealed corners, and rejects enclosed routes. The route field is reused for sources in one observer's scent-cue query. For an obstructed route, the cue follows the first open-air path direction near the observer rather than exposing a source's exact bearing through a wall. The range multiplier still uses broad wind alignment; this is not a computational fluid simulation.

## Anonymous scent presentation

`World::scentCues(observerId)` considers only currently unseen scentable wolves, aggregates them into at most one cue per 45-degree sector, and returns only `sector`, a strength category from 1–3, and `windborne`. Sectors are E, SE, S, SW, W, NW, N, NE. Aggregation intentionally prevents a cue count from becoming a wolf count. No source name, stable ID, speaking color, player/NPC label, coordinates, precise distance, or hidden posture travels with a cue.

The local map renders restrained lavender arcs and `~~` marks on a fixed-radius compass around the observer. Their direction is meaningful; their radius is not source distance. A text status repeats the broad direction, and **Smell** provides an observer-specific narrative result. A cue is not selectable as a hidden actor and disappears once its source becomes visible or no longer scentable. Cues are not placed as wolf tokens in hidden terrain and do not expose adjacent cells on the world map. An empty result means no distinct scent was detected, not that the area is empty.

The snapshot also carries anonymous `movementHeard` state for unseen moving players, independently of scent cues. The existing rate-limited pawstep events remain separate. Seeing, hearing, and scenting must remain distinguishable in the UI rather than being combined into a single omniscient "nearby actors" list.

Scent never permits inspection, reveals a wolf token, supplies unseen speech/actions, establishes identity, or earns/refreshes visual map memory. A direction is deliberately uncertain evidence for player inference, not authoritative knowledge of who or exactly where someone is.

## Persistence and verification

`CellMemory` records knowledge tier, cell identity and remembered topology, observed glyphs, and an observed mask. These records are included in `PersistedWorld`. Memory does not store live actor locations, voice events, fixture openness, or current activity.

Smell sensitivity, nose health, scent skill, and each cell's base wind are persisted. The saved world clock preserves deterministic gust phase on restart. Effective gust samples and ephemeral scent cues are not persisted as world knowledge. Missing wind data remains compatible with older saves; malformed wind or sensory data is rejected before restoring any world state.

### Weather and daylight integration

All perception consumes the server's shared environment sample. Outdoor illumination multiplies vision (night floor 0.28); rain/snow/fog apply their additional sight factors. This controls visible terrain, wolves, action clarity, inspection and new visual memory—not merely a dark overlay. Rain/snow and wind mask speech and pawsteps; weather also scales the existing directional scent calculation. Fog has a provisional 1.05 scent multiplier. Darkness does not reduce hearing or smell, so a wolf can hear or scent an unseen neighbor without gaining a visual token or identity. Indoor shelter excludes outdoor penalties, but lighting is independent: `max(0.08, artificial, daylight × daylightAccess)` scales sight. Thus unlit sealed interiors remain dark even at noon, and window-only rooms darken after sunset. The 8% close-awareness floor is provisional. The client's cell-edge glow adds no perception or hidden identities. See [environment design](10-interactions-environment.md) for exact tuning and limits.

Behavior tests verify unseen cell omission, coarse glimpse, entering to gain detail, hidden mutation remaining stale, year-later restoration, first-adjacency filtering, vision occlusion, injured senses, weather impact, hearing across portals, visible Z activation, remembered Z returning to 2D, hidden entity omission, skill-sensitive stealth cutoffs, blocked hidden-actor interaction, and movement-sound detection separately from speech. Scent tests additionally cover reversed/cross/calm wind, weather and nose/skill effects, independence from sight/hearing, sealed rooms, opened doors, paths around obstacles, diagonal corner sealing, all eight sectors, anonymous aggregation, unchanged visual memory, source disappearance, persisted gust continuity, and atomic invalid-save rejection.

## Prototype limits

The perception model is deliberately testable and approximate: sampled line of sight over a heightfield (see `22-elevation-weather.md`), linear clarity curves, authored heights, one portal hop for sound, and bounded same-cell air routes for live body scent. It does not yet model acoustic diffraction around complex geometry, separate hearing frequencies, age curves, directional visibility, illumination fields, translucent materials, or visible slivers of an adjacent floorplan. Scent does not yet leave lingering trails, persist after its source leaves, identify a unique wolf, distinguish personal odor signatures, or cross cell portals. Indoor drafts, cross-cell airflow, detailed plumes, deposited traces, and scent-based tracking are later design work. Adjacent cells currently show their known shape and remembered detail; they do not stitch live adjacent terrain into the local view.
