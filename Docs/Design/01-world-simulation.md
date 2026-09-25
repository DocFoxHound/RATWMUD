# World simulation and traversal

Status: implemented vertical-slice design, extended with pace and world travel on 2026-09-21.

## Responsibility and authority

`ratw::World` owns cells, terrain, actors, door state, an elapsed simulation clock, resident routines, and observer map memories. It uses C++17 and the standard library, with no rendering or Unreal headers. The Unreal runtime invokes the same rules as the portable tests. Clients submit movement and interaction intentions; a client never supplies an accepted position or a fixture state.

Coordinates use terrain-tile units: X points right, Y points down. Each cell has its own local coordinate space and an authored world X/Y/Z placement. Wolves use continuous coordinates and retain an independent facing angle in radians. The visible `W` and orbiting `>` are client presentation; neither determines occupancy.

## Authored slice

| Cell | Size | Role |
| --- | --- | --- |
| `tavern` / The Bent Bough | 32×24 | Standard reference space, low tables, counter, closed pantry and yard doors |
| `exterior` / Juniper Yard | 40×28 | Larger scrolling space, rain, shallow water, a stepped rise |
| `loft` / The Quiet Loft | 20×14 | Small upper cell, visible vertical relationship |

Every demo cell is stored in its own `Data/Cells/*.cell` file. Identical embedded fallbacks permit a packaged demonstration without external files. Demo doors and NPC fixtures are separately authored in the core. Atlas Workshop now exports separate custom cells and validated door/passage/stair manifests; NPC fixture authoring remains a later step. Terrain-file/fallback consistency is tested.

All extents are authored dimensions. Boundary rules do not impose a landmark-based subdivision of the world. The local view always remains one cell, including when an exit is open.

## Commands

| Operation | Result |
| --- | --- |
| `move(id,dx,dy)` | Replace route with normalized direct input; continues until zero/stop/new command |
| `moveTo(id,x,y)` | Replace direct input with an authoritative path |
| `stop(id)` | Clear input, route, and velocity; preserve facing |
| `setPace(id,pace)` | Validate a requested notch 0–10 without replacing the destination |
| `travelTo(id,cell)` | Start a remembered multi-cell journey; reject unvisited/unconnected destinations without replacing active travel |
| `cancelTravel(id)` | Clear an active world journey and its local movement |
| `face(id,x,y)` | Start a bounded shortest-arc idle turn toward a point; rejected while moving |
| `setPosture(id,posture)` | Validate standing/sitting/lying/crouching, cancel travel, and apply any rise delay |
| `interact(id,target,verb)` | Validate location, visibility, reach, verb, and fixture state before mutation |
| `actions(id,target)` | Return currently discoverable meaningful actions |
| `tick(dt)` | Advance in bounded fixed-size integration steps |

Nonfinite input and invalid destinations are rejected. Large simulation calls are limited to 60 seconds and subdivided into at most 1/30-second motion steps, preventing wall tunneling. The host should tick regularly instead of treating this cap as a fast-forward interface.

## Pathfinding and collision

A* uses a navigation lattice four times finer than the visible terrain grid (quarter-tile node spacing). It supports eight directions, disallows diagonal corner cutting, and weights terrain cost and height changes. A clearance-checked smoothing pass removes visible lattice zigzags while preserving the chosen route's approximate terrain cost. Wolves do not snap to this lattice: integration reaches the requested continuous destination and consumes remaining movement time when crossing a waypoint, so clicking does not introduce a lower movement speed than direct input.

The soft wolf core has radius 0.065 terrain units. Static terrain blocks at that small footprint; dynamic wolf overlap causes only a small separation correction. Other wolves are not hard path obstacles. Separation is relaxed near portals, and a transition never searches for an unoccupied destination. This preserves the authored arrival anchor and prevents doorway body blocking.

Direct movement can slide gently along solid terrain. An invalidated click route is canceled. Facing follows accepted motion and remains unchanged when stopped. A height jump greater than 0.75 tile-height units blocks a step; smaller authored rises can be traversed at an adjusted cost. These units and values are prototype tuning, not settled lore or physical scale.

## Pace, stamina, and world journeys

Walking remains 2.6 tiles/second; dexterity 0–100 sets the sprint cap to
`5.2 + .052*dexterity`. Eleven notches interpolate between walking and that cap.
Recovery is continuously +5 stamina/second up to 100; actual movement costs
`15*(effectivePace/10)^2` per second before recovery. A default full sprint
therefore spends ten net stamina/second, while a middle trot can recover.
Exhaustion forces walking until 20 stamina, preserving the requested notch.
Crouching uses walking pace with its existing 0.30 multiplier, never sprint.
Terrain and weather modifiers still apply after pace selection.

Stamina is integrated once per simulation step, not once per path waypoint.
Only accepted translation time consumes effort; wall-blocked input, passive
bumps, posture preparation, and portal teleport distance do not. Dexterity,
stamina, selected pace, and exhaustion persist, but transient rate and routes
do not. The statistics are server-owned hooks, not a finished training system.

World travel connects visited cells only through reciprocal portal endpoints
the character has observed. Its global estimate uses remembered dimensions;
local weighted exit ranking and quarter-tile A* handle current terrain. Only
the current cell's live conditions affect local execution. Blocked journeys
pause, closed doors require explicit Open, and a rejected replacement request
preserves the existing journey. Known Routes exposes only remembered destination
geometry, not remote live simulation. Exact contracts and initial tuning are in
[pace and world travel](13-pace-and-world-travel.md).

## Deliberate facing and posture

Holding Alt while stationary previews a faded `>` toward the mouse on the local map. The preview is client-only: it does not alter authoritative facing or reveal anything to another player. Alt-click submits the face-point intention; Ctrl-click remains a compatibility shortcut. The server turns through the shortest angular arc at an initial 180 degrees/second, without translating the wolf. Accepted movement cancels a manual turn and once again controls facing. Preview and navigation bindings are suppressed while composing text.

Posture is structured physical state, not merely a label. Initial tunings are:

| Intention | Preparation | Result |
| --- | --- | --- |
| Move while sitting | 0.65 seconds stationary | Standing, normal movement |
| Move while lying | 0.45 seconds stationary | Crouching, 30% of normal movement speed |
| `/stand` while lying | 1.0 second stationary | Standing |
| `/stand` while crouching | 0.5 seconds stationary | Standing |
| `/stand` while sitting | 0.65 seconds stationary | Standing |

The temporary `rising` posture exposes a target posture and remaining preparation time. Repeated held movement does not restart that timer. Direct input or a click route resumes only after preparation finishes; releasing/stopping cancels the travel intention while allowing the physical posture change to finish. Crouching persists when movement stops and until an explicit posture command changes it. Sitting down and lying down remain immediate in this slice. Preparation durations, turning rate, and the speed multiplier are gameplay tuning, not fixed animation promises.

Sneak and hearing skills use validated 0–100 values. Crouching reduces visual detection and movement-sound reach according to the perception component. This change provides the skill hooks, not a skill-training or progression system.

## Doors and transitions

Click routes never operate a fixture. When a valid route is blocked by a closed door, the actor approaches the usable side, stops, and receives the target to highlight. Door context includes Inspect, Listen, Knock, Open, and Close where applicable. Only an explicit Open operates the barrier.

Opening the pantry during ordinary local navigation cancels the local route and leaves the wolf in the same cell. A fresh movement command is required; the world-journey exception is described below. Opening a cross-cell door validates the interaction and opens both linked sides; if posture preparation is needed, the crossing waits for it before entering the destination and stopping at its authored anchor. It does not bypass sitting/lying delays or require another Open after preparation. A new move, stop, or posture command cancels that pending crossing. The queued crossing rechecks the open portal, current cell, and reach before transfer. Already-open boundary exits transfer automatically when direct or click movement crosses the edge, after the same posture rules have allowed movement to begin. Open stairs require Enter because they are interior portals rather than boundary exits; that intention can come from the player or an explicitly selected world journey.

Every transition clears local input, path, and velocity, and sets `Entity::transitioned`. The hosting input bridge consumes this flag to suppress carried held movement until release/reissue. The core does not know physical keyboard state. Arrival position remains exact even in a crowded doorway. An explicit world journey is the narrow continuation exception: it retains its high-level destination and issues the next local leg after a 0.25-second stop. Ordinary WASD/local-click crossings remain stopped until fresh input.

A world journey paused by a same-cell barrier may continue after an explicit
Open, rather than requiring a new destination. Already-open interior portals
can receive Enter from the selected journey. It cannot silently operate a
closed door or bypass posture preparation. Manual navigation, Stop/Wait,
navigation Escape, or posture/facing commands cancel the world intention.

Close refuses when an actor occupies either linked threshold. Stairs have no Close action. The Atlas importer enforces reciprocal fixtures and valid destination anchors for custom content; the slice's fixed portal pairs are also covered by tests.

## Residents and weather

Six residents now use persistent hunger/fatigue and a deterministic priority selector tied to the shared four-hour day. They physically navigate to food, sleep, paid work, herb gathering, cooking and trade/delivery. Effects require actual arrival and elapsed work; no teleportation or LLM decision is used. NPCs explicitly open permitted doors. Bracken's normal work pauses while `leaderId` is set; party needs remain a follow-up. This supersedes the earlier 60-second demonstration loop. See `14-calendar-aging.md` and `15-npc-society-economy.md`.

Rain and snow slow outdoor movement; fog/rain/snow affect perception. Indoor cells ignore outdoor weather modifiers. The screen overlay belongs to presentation. Calendar time, seasonal weather generation, shelter zones, and energy needs are future simulation extensions.

## Persistence contract

`save()` creates a portable `PersistedWorld` containing players, residents, fixture states, weather, clock, and map memory. Position, facing, stable posture, sneak/hearing skills, dexterity, stamina, requested pace, exhaustion, declared state, activity, and companion leader identity survive. World journeys, local paths, direct input, velocities, pending portal crossings, manual turn intentions, typing, and speaking timers do not resume after reload. Stamina rate resets without awarding offline recovery. An interrupted rise is normalized to its target posture in the saved/restored representation, with no remaining timer or latent movement. `restore()` validates finite values, skill/stat/pace ranges, canonical postures, and transition consistency before applying state, preserving the current world when validation fails. The runtime owns storage encoding, atomic file replacement, schema/version migration, and ownership of player identities.

## Verification and remaining limits

`Tests/world_tests.cpp` exercises continuous movement, normalized diagonals, bounded shortest-arc idle facing, movement cancellation of turning, posture delays and cancellation, retained click/held intentions, slower crouching, stealth detection, rejection of nonfinite commands, no tunneling, routes around fixtures, door approach/open/resume semantics, delayed portal interactions, both portal movement modes, exact arrival anchors, crowd traversal, injury/weather/elevation, resident routines, and persistence without restored movement intentions.

`Tests/pace_tests.cpp` and `Tests/travel_tests.cpp` add energy conservation,
dexterity scaling, exhaustion, remembered-route privacy, multi-cell execution,
interruption, and safe persistence. Executed results belong to the test report.

This slice uses an authored terrain grid plus low-cost navigation, not a
production navmesh. It has no combat resolver, full stat-training progression,
climbing animation, procedural generation, or production-scale route/load proof.
Human gait/stamina balance and party-pace coordination remain open.
