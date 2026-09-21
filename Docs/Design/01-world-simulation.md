# World simulation and traversal

Status: implemented vertical-slice design, 2026-09-20.

## Responsibility and authority

`ratw::World` owns cells, terrain, actors, door state, an elapsed simulation clock, resident routines, and observer map memories. It uses C++17 and the standard library, with no rendering or Unreal headers. The Unreal runtime invokes the same rules as the portable tests. Clients submit movement and interaction intentions; a client never supplies an accepted position or a fixture state.

Coordinates use terrain-tile units: X points right, Y points down. Each cell has its own local coordinate space and an authored world X/Y/Z placement. Wolves use continuous coordinates and retain an independent facing angle in radians. The visible `W` and orbiting `>` are client presentation; neither determines occupancy.

## Authored slice

| Cell | Size | Role |
| --- | --- | --- |
| `tavern` / The Bent Bough | 32×24 | Standard reference space, low tables, counter, closed pantry and yard doors |
| `exterior` / Juniper Yard | 40×28 | Larger scrolling space, rain, shallow water, a stepped rise |
| `loft` / The Quiet Loft | 20×14 | Small upper cell, visible vertical relationship |

Every cell is stored in its own `Data/Cells/*.cell` file. Identical embedded fallbacks permit a packaged demonstration without external files. Doors and NPC fixtures are separately authored in the core for this first slice; data-driven fixture editing is a documented next step. Terrain-file/fallback consistency is tested.

All extents are authored dimensions. Boundary rules do not impose a landmark-based subdivision of the world. The local view always remains one cell, including when an exit is open.

## Commands

| Operation | Result |
| --- | --- |
| `move(id,dx,dy)` | Replace route with normalized direct input; continues until zero/stop/new command |
| `moveTo(id,x,y)` | Replace direct input with an authoritative path |
| `stop(id)` | Clear input, route, and velocity; preserve facing |
| `face(id,x,y)` | Change idle facing toward a point; rejected while moving |
| `interact(id,target,verb)` | Validate location, visibility, reach, verb, and fixture state before mutation |
| `actions(id,target)` | Return currently discoverable meaningful actions |
| `tick(dt)` | Advance in bounded fixed-size integration steps |

Nonfinite input and invalid destinations are rejected. Large simulation calls are limited to 60 seconds and subdivided into at most 1/30-second motion steps, preventing wall tunneling. The host should tick regularly instead of treating this cap as a fast-forward interface.

## Pathfinding and collision

A* uses a navigation lattice four times finer than the visible terrain grid (quarter-tile node spacing). It supports eight directions, disallows diagonal corner cutting, and weights terrain cost and height changes. A clearance-checked smoothing pass removes visible lattice zigzags while preserving the chosen route's approximate terrain cost. Wolves do not snap to this lattice: integration reaches the requested continuous destination and consumes remaining movement time when crossing a waypoint, so clicking does not introduce a lower movement speed than direct input.

The soft wolf core has radius 0.065 terrain units. Static terrain blocks at that small footprint; dynamic wolf overlap causes only a small separation correction. Other wolves are not hard path obstacles. Separation is relaxed near portals, and a transition never searches for an unoccupied destination. This preserves the authored arrival anchor and prevents doorway body blocking.

Direct movement can slide gently along solid terrain. An invalidated click route is canceled. Facing follows accepted motion and remains unchanged when stopped. A height jump greater than 0.75 tile-height units blocks a step; smaller authored rises can be traversed at an adjusted cost. These units and values are prototype tuning, not settled lore or physical scale.

## Doors and transitions

Click routes never operate a fixture. When a valid route is blocked by a closed door, the actor approaches the usable side, stops, and receives the target to highlight. Door context includes Inspect, Listen, Knock, Open, and Close where applicable. Only an explicit Open operates the barrier.

Opening the pantry cancels any route and leaves the wolf in the same cell. A fresh movement command is required. Opening a cross-cell door validates the interaction, opens both linked sides, enters the destination, and stops at its authored anchor. Already-open boundary exits transfer automatically when direct or click movement crosses the edge. Open stairs require the explicit Enter action because they are interior portals rather than boundary exits.

Every transition clears input, route, and velocity, and sets `Entity::transitioned`. The hosting input bridge consumes this flag to suppress carried held movement until release/reissue. The core does not know physical keyboard state. Arrival position remains exact even in a crowded doorway.

Close refuses when an actor occupies either linked threshold. Stairs have no Close action. A future portal-authoring validator should enforce reciprocal fixtures and valid destination anchors for arbitrary user content; the slice's fixed portal pairs are covered by tests.

## Residents and weather

Six residents have names, descriptions, speaking colors, locations, and four-part routines. The demonstration uses a 60-second schedule phase so behavior can be observed quickly. Residents move along ordinary routes, choose Open as an explicit NPC decision, and cross cells to work, eat, check the road, or rest. They do not teleport to schedule points. Bracken's schedule pauses while `leaderId` is set; recruited-party behavior belongs to the runtime component.

Rain and snow slow outdoor movement; fog/rain/snow affect perception. Indoor cells ignore outdoor weather modifiers. The screen overlay belongs to presentation. Calendar time, seasonal weather generation, shelter zones, and energy needs are future simulation extensions.

## Persistence contract

`save()` creates a portable `PersistedWorld` containing players, residents, fixture states, weather, clock, and map memory. Position, facing, posture, declared state, activity, and companion leader identity survive. Routes, direct input, velocities, typing, and speaking timers are cleared in the saved representation. `restore()` validates before applying state and preserves the current world when validation fails. The runtime owns storage encoding, atomic file replacement, schema/version migration, and ownership of player identities.

## Verification and remaining limits

`Tests/world_tests.cpp` exercises continuous movement, normalized diagonals, idle facing, rejection of nonfinite commands, no tunneling, routes around fixtures, door approach/open/resume semantics, both portal movement modes, exact arrival anchors, crowd traversal, injury/weather/elevation, resident routines, and persistence.

This slice uses an authored terrain grid plus low-cost navigation, not a production navmesh. It has no combat resolver, stamina, climbing animation, procedural cell generation, arbitrary world editor, or reconnect interpolation. Those should be added behind the same authority boundary after this interaction model is reviewed.
