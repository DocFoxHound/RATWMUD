# Pace, stamina, and remembered world travel

Implemented follow-up, September 21, 2026. This adds deliberate overland travel
without replacing the separate-cell world or the roleplay-first interface.
Numbers below are prototype tuning, not final balance or a physical tile scale.

## Player workflow

While in navigation mode, roll the mouse wheel **over the local map** or press
**Page Up / Page Down** to raise/lower the requested pace by one notch. The
eleven-segment pace control also accepts clicks. Shift-wheel pans the local map
vertically; Ctrl-wheel pans horizontally. Scrolling prose does not change pace.
Chat and modal panels suppress pace changes and other movement bindings.

The pace strip names the gait, highlights sprint in amber, and distinguishes a
requested change from server confirmation, posture limitation, and exhaustion.
The adjoining stamina bar shows the percentage, actual recovery/drain rate,
dexterity, and flat-terrain top speed. Words and segmented controls accompany
color, so sprint and fatigue do not rely on color alone.

Open **World → Known Routes**, then click a previously visited cell or its list
entry. The server selects a remembered route and walks the wolf through its
local cells. This is travel on foot, not fast travel: movement, weather, posture,
stamina, perception, and local interactions continue normally. The destination
is a **cell**, with the journey ending at its connection anchor; selecting an
exact tile in a remote cell is not implemented.

## Authoritative pace and energy

The server owns dexterity, stamina, and exhaustion. A client can request only a
pace notch from 0 to 10, not speed or additional stamina. New/legacy characters
default to dexterity 50, stamina 100, and pace 0. Dexterity and stamina are bounded
to 0–100. These are working statistics, not a completed skill-training system.

```text
walk speed = 2.6 terrain tiles / second
sprint cap = 5.2 + 0.052 × dexterity
flat speed = 2.6 + (sprint cap − 2.6) × effective pace / 10
recovery   = +5 stamina / second, continuously, up to 100
gross drain = 15 × (effective pace / 10)² per second of accepted movement
```

| Notch | Gait | Speed at dexterity 50 | Net stamina/s while moving |
| --- | --- | --- | --- |
| 0 | Walk | 2.60 | +5.00 |
| 1–5 | Trot | 3.12–5.20 | +4.85 to +1.25 |
| 6–8 | Run | 5.72–6.76 | −0.40 to −4.60 |
| 9–10 | Sprint | 7.28–7.80 | −7.15 to −10.00 |

Walking remains 2.6 across the dexterity range; dexterity changes the top speed
and the intermediate notches. Terrain cost and outdoor rain/snow modify movement
after pace selection. Slow terrain does not make full-effort sprinting free.
Crouching always uses walking pace with the existing 0.30 speed multiplier;
selecting sprint cannot create a fast sneak. Sitting/lying preparation still
consumes time before movement, with recovery continuing during the rise.

A full, uninterrupted notch-10 sprint lasts about ten seconds from 100 stamina.
At zero, exhaustion forces walking until stamina reaches 20, normally four
seconds of recovery. The requested pace remains selected and can resume after
that recovery band; players can lower it instead. This hysteresis avoids rapid
walk/sprint flickering at zero. A middle trot can replenish stamina during travel.

Integration awards recovery once per simulation step, even when that step
crosses many short path waypoints. Drain uses actual accepted travel time:
blocked input, passive collision bumps, stationary facing, unused time after
arrival, and portal teleport distance do not spend sprint energy. The UI's rate
is the actual bounded change, so a full bar can report steady rather than +5/s.

## Remembered route planning and privacy

The existing **Nearby** world map stays current-cell-plus-adjacent and retains
its established visibility, dim memory, and visible-height presentation.
**Known Routes** is a separate remembered destination index. It can show all
visited cells using cached names, dimensions, and X/Y/Z placements, including
non-adjacent ones. It does not send remote glyph detail, occupants, current door
state, or weather, and it never upgrades exploration memory.

The server's hierarchical planner connects only visited cells whose reciprocal
portal endpoints have both been observed. Merely glimpsing a destination,
possessing a map, or visiting two places without seeing their connecting exit
does not establish a shortcut. Unknown and unvisited IDs receive the same
rejection, and an invalid replacement destination leaves the active route intact.

The inter-cell estimate uses remembered dimensions, not unseen live conditions.
Inside the current cell, a weighted terrain flood ranks candidate exits and the
existing quarter-tile A* supplies the precise route. This avoids running an
expensive precise search for every exported border seam. It is a practical
hierarchical route, not a promise of a globally optimal path through stale terrain.

Each crossing still clears local movement and preserves its arrival anchor.
An explicitly selected world journey waits 0.25 seconds there, then issues its
next local leg. Ordinary WASD/local-click transitions remain stopped until the
player supplies new movement. A closed barrier pauses the world journey and
requires an explicit **Open** action; it is never automatically unlocked or
opened. Once opened, the selected journey may continue, including after a
same-cell barrier. Already-open stairs/passages can be entered as part of that
explicit world-travel intention. Unresolvable or repeatedly blocked routes pause
with a status message instead of moving forever against an obstacle.

## Cancellation, writing, and persistence

An explicit world journey continues while composing roleplay or opening panels.
Ordinary local click movement still stops on entering chat. Releasing WASD or
changing focus must not accidentally send a zero-input cancellation of an
active world route. Simulation and prose-reveal order remain independent.

Nonzero manual WASD, a local destination click, Stop/Wait, navigation-mode Escape,
or an explicit posture/facing command cancels world travel. Closing a modal with
Escape or leaving chat with a preserved draft is not a travel cancellation;
Escape in navigation mode is. The travel panel also provides a cancel control.

Dexterity, selected pace, stamina, and exhaustion survive a checkpoint. Restore
validates the fields before replacing the live state, clears transient rates,
and awards no offline recovery. World routes, local paths, input, and pending
crossings never resume after reload; choosing a destination again is deliberate.
Private statistics and route intentions belong to the owning player's snapshot,
not other observers' public entity rows.

## Verification and limits

`Tests/pace_tests.cpp` covers gait bounds, dexterity scaling, energy conservation
across timestep/path subdivisions, collision/posture/weather, exhaustion, and
atomic save validation. `Tests/travel_tests.cpp` covers remembered connectivity,
privacy, multi-cell execution, barriers, interruption, and restart boundaries.
See [the current test report](../TEST_REPORT.md) for executed portable, native,
packaged, and screenshot evidence rather than treating this design as a test log.

Human pace/energy balance, party-wide pace matching, stamina training, hostile
load, and production-scale route performance remain open. Existing authored
world limits still apply. The first destination selector is cell-level; no
remote-tile routing, offline travel, unexplored scouting, or teleportation is
implied. Follow-up tuning questions are in [the next-session list](../MORNING_QUESTIONS.md).
