# Interactions, height and weather

Status: implemented environment slice, updated 2026-09-21 for shared weather, wind, live body scent, and day/night lighting.

## Shared interaction rules

Contextual menus and typed actions invoke the same validated server verbs. The interface opens a small anchored menu when a visible entity or fixture is selected. Menus offer only appropriate verbs. General Listen, Smell, Look, and Wait do not require a target; posture controls provide sitting, lying, and standing. A fuller Rest mechanic remains planned rather than implied by those posture controls.

Inspect requires visibility. Manipulation requires reach and a valid target in the current cell. Pathing toward a closed door approaches its usable side and stops. It never opens the door on behalf of the player. A same-cell Open clears the barrier but leaves movement stopped. An inter-cell Open moves the actor to the authored destination anchor and stops. Open edge crossings transition automatically. The UI suppresses held movement until it is released and reissued after crossing.

An NPC is an interaction target, not an authority bypass. Recruiting a companion, speaking and inspecting use the actor's server identity. Context menus cannot grant items, XP, quests or powers without a corresponding validated rule.

## Height

Each terrain tile carries elevation. Finer navigation geometry limits impassable height changes and permits authored slopes or steps. Height is sampled for movement and sight. The local renderer uses glyph/style cues; world-map vertical stacking uses its authored relative Z. Only a currently visible adjacent stack invokes isometric presentation. A remembered loft alone must not cause the camera to tilt.

## Weather

The prototype supports clear, rain, fog, and snow conditions per cell. A single authoritative `Environment` sample supplies illumination and the multipliers used by vision, speech/pawstep hearing, scent, movement, and the client conditions display. Outdoor rain and snow change movement, sight, hearing, and airborne scent reach; fog reduces sight and has a small provisional scent bonus. Indoor shelter excludes those outdoor penalties. Seasonal forecasts now choose conditions; regional fronts, exposure and temperature are not simulated.

Initial multipliers are gameplay tuning:

| Outdoor condition | Movement speed | Sight range | Hearing reach | Scent reach |
| --- | --- | --- | --- | --- |
| Clear | 1.00 | 1.00 | 1.00 | 1.00 |
| Rain | 0.85 | 0.78 | 0.72 | 0.65 |
| Fog | 1.00 | 0.40 | 1.00 | 1.05 |
| Snow | 0.70 | 0.65 | 0.85 | 0.80 |

These are noon/calm factors. Sight also multiplies outdoor illumination; hearing also multiplies `1 − 0.25 × wind strength`. Fog's 5% scent bonus is explicit game tuning, not a claim of biological realism. Darkness alone does not penalize ears, nose, movement, or stamina recovery: those independent senses become more useful when sight fails.

Rain uses layered wind-driven streaks and splashes; snow has drifting flakes; fog has moving translucent veils. Daylight, dawn/dusk, and night have distinct map tint and lighting treatments. The effects are map-only and never wash over the roleplay pane, controls, or menus. Reduced-motion settings freeze atmospheric animation without changing simulation; static condition cues and readable labels remain. Actors, actions and hit targets still come only from server-filtered visible data, not from a cosmetic visibility calculation.

## Shared daylight clock

One game day lasts **14,400 simulation seconds** (four hours), with two hours from morning to evening and two back to morning. The shared calendar has 365-day years and temperate seasons. Dawn is 05:00–07:00 and dusk 17:00–19:00, with smooth transitions. See `14-calendar-aging.md` for lunar-clock policy, dates, seasonal forecasts and exact light curves.

Outdoor night illumination ranges from 0.08 at new moon to 0.40 at clear full moon; rain, fog and snow attenuate moonlight. Daylight reaches 1.0. This multiplies sight alongside weather, age, vision and eye health, affecting terrain, wolves, action clarity, inspection and exploration. Remembered terrain remains dim without revealing current occupants.

### Interior lighting and cell-edge atmosphere

Shelter and light are independent. Each cell has an authored whole-cell `lighting` profile: artificial light (0–1), daylight access (0–1), and warm/neutral/cool tone. Outdoors uses the sky and weather; the profile is retained for authoring but does not override outdoor illumination. Missing legacy profiles default to `1 1 warm`.

Inside, natural light is `daylight × daylightAccess`; illumination is `max(0.08, artificial, natural)`. This illumination actually scales server sight, action clarity, actor detection, inspection and newly observed terrain. The 8% minimum is a provisional close-awareness floor, not a claim that a sealed room has a light source. Hearing, scent and footing remain sheltered at 100%; a dark room is not automatically deafening or windy.

| Room profile | Day | Night |
| --- | --- | --- |
| Warm tavern: `1 1 warm` | Clear, no atmospheric glow | Clear, warm perimeter glow |
| Windowed, unlit: `0 1 warm` | Daylight | Dark, faded edges |
| Sealed, unlit: `0 0 warm` | Dark, faded edges | Dark, faded edges |
| Sealed, cool-lit: `1 0 cool` | Clear, cool glow | Clear, cool glow |

Artificial glow is `artificial × (1 − natural)`, so a bright day suppresses unnecessary glow and fading daylight gradually brings it back. Soft halo contours sit outside the **actual cell rectangle**, with inward edge fades for darkness and weather. Rain, snow, fog and twilight have restrained separate edge colors alongside their existing screen effects. Effects are clipped to the map pane, never the story or menus. For a panned oversized cell, effects remain anchored to the cell's real boundary rather than inventing a nearby edge at the viewport crop. A daylit interior has neither a darkness fade nor an artificial glow.

Atlas Workshop exposes all three lighting fields and exports `lighting: artificial daylightAccess tone` in each `.cell` file. Development settings also offer warm/unlit/daylit/cool presets, unavailable to ordinary players. Lighting profiles persist atomically with world state; invalid records reject the complete restore. The global phase is visible indoors without revealing weather in unseen neighboring cells. NPC dialogue receives the same local conditions and does not describe every interior as lit.

This is whole-cell ambient lighting, not simulated individual lamps, window-shaped light pools, shadows, moon phases, roof patches within outdoor cells, or dark adaptation. Those remain later extensions.

Persisted absolute `calendarDays` preserve dates, lunar phase and seasonal forecasts across restart. The old `clockOffsetHours` migrates legacy checkpoints without retroactive birthdays. Server downtime does not advance time; absent characters catch up to the running world's calendar. NPC life follows daily time and needs, independently of dialogue. Memory consolidation still uses **one real hour of inactivity**, not game hours. Local conditions enter NPC dialogue context.

## Wind

Each outdoor cell has a persisted base direction, normalized strength from 0–1, and optional variation. `World::windAt` derives effective direction and gust strength from that base, a stable cell phase, and the saved world clock. Small smooth heading shifts and gusts therefore continue across restart instead of resetting randomly. Default outdoor wind flows west to east at base strength 0.5 with variation enabled; indoor air is calm and cannot be assigned nonzero wind in this slice.

Direction means **where air travels**, not the meteorological direction it comes from. East is zero radians and south is `pi/2` because map Y increases downward. The HUD makes this explicit with an airflow label such as `W → E`. A scent arriving from an unseen wolf west of the observer is consequently an upwind/west cue, even though the air moves east.

Development-only controls offer east, west, north, calm, and live/variable wind presets, weather presets, and dawn/day/dusk/night clock presets. Normal players cannot change shared weather or time. Wind strength affects scent carriage, precipitation drift and sound masking, not direct movement force or spoken-voice direction. Indoor drafts, interconnected cell winds, fronts, and storm evolution remain future work.

## Smell as an interaction and passive sense

The server continuously supplies permitted, anonymous scent directions for currently unseen wolves. The local map draws fixed-radius lavender arcs with `~~` around the observer; the glyph marks a broad direction, not a hidden wolf's position or distance. Scent status text and the general **Smell** action convey the same observer-specific information without requiring the player to click an unseen target.

Smell depends on live source position, connected air paths, wind, outdoor rain/snow, smell sensitivity, nose health, and scent skill. Closed doors and opaque walls seal scent routes, but air can take a bounded path around obstacles and pass low furniture. Wind carries scent farther downwind, while nearby wolves can still be scented in calm air. See [Perception](02-perception-maps.md) for the exact first-pass range formulas and privacy contract.

Multiple unseen sources in a broad direction combine into one cue. Neither passive cues nor **Smell** reveal names, stable IDs, source count, player/NPC category, exact positions, or speaking colors. Scent cannot reveal speech/actions, enable inspection, expose a wolf token, or create visual map memory. Sneaking does not suppress body odor. An empty result is absence of detection, not proof that no wolf is present.

This first implementation is current body scent within one stored cell. It does not leave tracks, identify individual odor signatures, retain trails after departure, or propagate through inter-cell doors. **Smell** can acknowledge scents from wolves already in view without creating duplicate unknown-source markers.

## Acceptance

- Closed doors stop pathing and require explicit interaction.
- Opening a door or crossing an edge produces the correct stop/transition behavior.
- Hidden and out-of-reach targets fail with useful feedback.
- Height and outdoor rain affect simulation, not only visuals.
- Reduced motion preserves weather information through text/status.
- Scent feedback respects current location, weather and permitted perception.
- Reversing wind reverses the downwind scent advantage; its HUD label cannot be confused with the source-bearing cue.
- Blindness, deafness, and loss of smell remain independent rather than collapsing into a shared detection flag.
- A sealed door blocks scent, connected air can bend around obstacles, and diagonal wall corners do not leak scent.
- Broad scent cues do not grant actor inspection, precise location, or visual memory, and disappear when no longer applicable.
- Base wind, sensory stats, and gust continuity survive restart; stale scent cues are recomputed rather than persisted.
- Day/night illumination, weather, and client conditions agree with authoritative perception; shelter excludes outdoor penalties.
- Clock and weather survive restart; malformed persisted values reject the complete record rather than silently changing conditions.
- Warm tavern day/night, window-only daylight and sealed darkness have distinct visuals and matching authoritative sight; lighting survives a separate-process restart.
- Lighting authoring survives cut/split/export; merging or recutting conflicting profiles fails rather than silently choosing one.

## Provisional tuning

Use forgiving interaction reach and slow deliberate walking. Exact speeds, weather penalties, slope limits, scent reach, wind variation, and cue visibility should be tuned during play. Current scent uses a bounded air-path/range approximation rather than biological or fluid-dynamics fidelity. These tunable rules do not require changing the authority or privacy architecture.
