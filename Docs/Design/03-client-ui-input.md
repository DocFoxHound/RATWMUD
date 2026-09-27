# Client interface and input

Status: implemented native Unreal Slate vertical slice. See `Source/RATWMUD/UI/SRatwGame.*`.

## Intent

The window is a shared stage for text roleplay. The narrative occupies the left pane and the current cell occupies the larger right pane. Muted charcoal, warm amber, sage, and cool blue distinguish structure, self, residents, and other players. Equipment and character illustrations appear only in their focused panels.

The client accepts observer-filtered JSON snapshots and roleplay events from the player controller. All actions produce JSON intentions. It neither simulates movement nor reveals terrain or identity hidden by the server.

## Rendering

- A 1600×1000 reference canvas scales uniformly to the viewport and centers within other aspect ratios.
- Settings offers four pane-balance presets: compact, balanced, wide narrative and text-first. Story text reflows and the composer resizes; the map keeps square tiles instead of stretching its graphics. The pane-balance automation verifies bounds and draft retention without server commands.
- Terrain is independently drawn glyph tiles. Blank unknown tiles remain blank; remembered tiles use reduced opacity. Entities use continuous coordinates and interpolation between snapshots.
- Every wolf is an upright `W`; a separate literal `>` glyph rotates and orbits the center. The pointer has no collision. Identity colors are distinct from speaking colors.
- The local pane contains only the current cell. Larger cells preserve glyph scale, and the map follows the wolf, stopping at the cell's edges; Shift-wheel pans vertically and Ctrl-wheel horizontally until the wolf next moves. Only on-screen tiles are drawn. Unmodified wheel over the local map adjusts pace.
- The Nearby world pane includes only the server-supplied current cell and adjacent cells. Unknown cells are omitted, glimpsed cells have coarse outlines, and visited cells can show the supplied remembered glyphs. The renderer never fills unobserved spaces with invented terrain.
- Known Routes is a separate destination view of cached names, bounds, and placement for all visited cells. It has no remote glyphs, live occupants, door state, or weather. Clicking a remembered cell/list entry requests cell-level travel, not a remote tile destination.
- Visible vertical neighbors switch the overview to offset planes. The prototype constructs the projection in Slate; it does not yet use a 3D scene camera. An always-flat option keeps explicit above/below labels.
- Outdoor rain and snow use sparse animated marks drifting with server-supplied wind. Reduced-motion mode keeps precipitation static. Fog adds a restrained screen veil. Server perception remains the source of visibility; a client effect cannot create or hide authoritative content.
- Anonymous scent cues are lavender broad arcs and `~~` marks at fixed radii around self, aggregated into eight directions. They are not map entities, exact bearings, distance markers or clickable targets. They appear only on the local map and never unlock exploration memory. A separate SIGHT / HEARING / SCENT strip distinguishes visible residents, audible unseen movement and unseen body scent. Hearing status is movement awareness, not a claim that every audible speech event is represented there.
- The weather label shows airflow direction and strength, or sheltered still air indoors. The own-character sheet includes scent skill and nose health. Speaking colors and the amber Alt-facing preview are independent of scent presentation.
- Local-map weather is visibly distinct: layered rain/splashes, drifting snow, fog veils, warm daylight/twilight and a cool night vignette. The shared clock and environmental sight/hearing/scent/footing percentages are visible without covering narrative text or the pace strip. Reduced motion freezes effect geometry, not server simulation. Indoor views suppress outdoor effects and do not claim knowledge of adjacent weather. Development-only settings expose clock presets; ordinary player controls cannot change the shared environment.
- Cell atmosphere follows the actual room rectangle, not the map pane: warm/neutral/cool artificial-light halos outside its boundary, soft darkness and weather fades inside. Lit taverns glow after sunset while staying clear; daylight suppresses their glow; unlit interiors darken mechanically and visually even while sheltered. Larger panned cells retain their true boundary. Effects are map-clipped, have no hit targets, and leave narration untouched. Whole-cell artificial intensity, daylight access and tone come from the server; ordinary players cannot change them. Reduced motion retains static atmosphere.

## Input and discoverability

| Input | Result |
| --- | --- |
| WASD | Continuous movement intention, periodically refreshed while held |
| Click terrain | Server path request |
| Wheel over local map / Page Up / Page Down | Request a higher/lower pace notch in navigation mode |
| Shift-wheel / Ctrl-wheel over local map | Vertical / horizontal viewport pan |
| Hold Alt + mouse movement | Faded `>` previews cursor direction around the stationary wolf |
| Alt-click / Ctrl-click | Gradual stationary facing request, taking priority over entity and door hits |
| Click a visible entity or door | Anchored action menu |
| E | Open the nearest visible entity/door action menu |
| 1–6 | Invoke the matching open contextual-menu action |
| Enter | Enter writing; while writing, submit and return to navigation |
| Shift-Enter | Insert a newline in the real multiline editor |
| Escape | Close a focused panel; leave writing with draft intact; otherwise cancel world travel |
| M / I / C / L | Map toggle / inventory / character / listen |

The general-action row offers Listen, Look, Smell, Wait, and Sit. Opening an interaction never auto-runs its action. Closed-door navigation and portal transitions are server rules. A transition clears held input and local viewport pan. Losing widget focus clears held movement, preventing a stuck movement key.

The pace strip exposes eleven clickable notches, named gait, amber sprint
emphasis, server-confirmation feedback, and posture/exhaustion limits. Its stamina
bar shows actual net recovery/drain, dexterity, and top speed. Chat and modal
guards prevent scrolling or typing from accidentally changing pace; wheel over
the transcript continues scrolling prose, and wheel in Known Routes pages its
destination list.

An explicitly selected world journey continues through chat and panels. Ordinary
local-click movement still stops on entering chat; releasing held keys does not
cancel a world journey. Nonzero WASD, a local destination, Stop/Wait, navigation
Escape, or a posture/facing command cancels it. Crossings preserve their anchors;
the server resumes the next world leg after 0.25 seconds. Nearby mapping and
local perception do not expand. See [pace and world travel](13-pace-and-world-travel.md).

The preview sends no command until clicked and never rotates the actual marker
locally. Received facing angles interpolate along the shortest arc. Movement,
pending movement, posture transitions, writing mode, modal/world views, Alt release,
focus loss and cell changes suppress the preview. Root `self` movement state takes
precedence over sanitized public entity rows. The faded preview is an orientation
aid, not a second collision shape.

The map status strip and character sheet display rising/crouching states. Moving
from sitting waits for standing; moving from lying enters slow crouching. `/stand`
returns to normal movement after its server-controlled rise. The client never
decides who is hidden or predicts a successful stealth result.

## Movement presentation (2026-09-21)

The server still simulates at 20 Hz. Observer-filtered pose frames now also arrive
at 20 Hz; heavier terrain, perception-grid, inventory and sheet snapshots remain
at 5 Hz. Each pose contains only ID, position, facing and a movement boolean for
the observer and currently visible actors. Paths and hidden positions are absent.

The client renders a 100 ms buffered timeline, linearly interpolating positions
and shortest-arc angles on every frame. This replaces exponential chasing of
200 ms snapshots, which repeatedly accelerated and decelerated the glyph. The
same timeline serves the local player, other players and NPCs. It is presentation,
not speculative movement or collision prediction. Long packet gaps hold the last
known pose; reconnects, room generations, large corrections and long timing
interruptions reset history. The buffer adds about 100 ms of presentation latency
on top of transport; very poor connectivity can still cause holds/corrections.

Entry-session tokens, ordered revisions and room-generation checks reject stale
packets, including returning to the same room. A room transition sends metadata
immediately and snaps to its entrance. Visibility loss removes a glyph immediately,
and delayed metadata cannot resurrect it. Static ASCII terrain stays pixel-aligned;
wolves, their facing indicators and attached UI use subpixel rendering.

`RATW.Movement.BufferedFrameInterpolation` checks 30/60/144 Hz, packet jitter/loss,
stops, angle wrapping and bounded history. `RATW.UI.MotionVisibilityAndTransitions`
checks the actual Slate tick, hidden-actor removal, stale metadata, room entry and
stall recovery. `RATW.Network.MotionPerceptionPrivacy` checks the light frame's
visibility boundary. The real two-client network scenario also requires more than
three pose frames per full snapshot.

## Acceptance and verification

`RATW.UI.InputAndDraftRecovery` exercises navigation, typing, the real editor's newline behavior, preserved drafts, send behavior, failed-send recovery, and movement suppression. `RATW.UI.SequentialRoleplayReveal` verifies that one post completes before another starts. Native screenshots should cover local play, world overview, character, inventory, and preferences with a real server snapshot.

Review on a 1600×1000 viewport and a smaller window for clipped text, overlay placement, and visible focus. Check a second connected player to verify observed movement and indicators.

`RATW.UI.StationaryFacingPreview` exercises preview versus commit, entity-hit
priority, coordinates under scaling, gradual angle presentation, suppression
states and Ctrl-click compatibility. The native movement scenario verifies real
command routing and replicated intermediate facing/posture states.

`RATW.UI.ScentAwareness` verifies bounded anonymous cue parsing, sector aggregation,
wind descriptions, invalid input, no scent hit targets and dev-only wind commands.
The native two-client scent scenario tests hidden-source omission, inspection
denial and detection changes after the server reverses wind.

## Confirmed next component: readable combat

Combat will have sparse, observer-authorized local-map effects and one expandable
narrative entry per encounter. Its collapsed form shows only the latest permitted
action and updates in place; its expanded form exposes the full perceived log.
Neither form interrupts the sequential IC reveal queue or scrolls a player away
from a passage they are reading. These are pending requirements, not current
features of the implemented Slate slice. See
[combat presentation](18-combat-presentation.md) for privacy, timing, history,
input/accessibility and acceptance rules.

## Remaining refinements

Free-drag splitters, independent font sizing, accessible focus outlines for every painted control, full horizontal map dragging, animated projection transitions, and richer client prediction remain later work. Four selectable pane proportions are implemented. The painted contextual controls have keyboard equivalents but do not yet expose a complete screen-reader accessibility tree.
