# Interactions, height and weather

## Shared interaction rules

Contextual menus and typed actions invoke the same validated server verbs. The interface opens a small anchored menu when a visible entity or fixture is selected. Menus offer only appropriate verbs. General Listen, Smell, Wait and Rest are available without selecting a target.

Inspect requires visibility. Manipulation requires reach and a valid target in the current cell. Pathing toward a closed door approaches its usable side and stops. It never opens the door on behalf of the player. A same-cell Open clears the barrier but leaves movement stopped. An inter-cell Open moves the actor to the authored destination anchor and stops. Open edge crossings transition automatically. The UI suppresses held movement until it is released and reissued after crossing.

An NPC is an interaction target, not an authority bypass. Recruiting a companion, speaking and inspecting use the actor's server identity. Context menus cannot grant items, XP, quests or powers without a corresponding validated rule.

## Height

Each terrain tile carries elevation. Finer navigation geometry limits impassable height changes and permits authored slopes or steps. Height is sampled for movement and sight. The local renderer uses glyph/style cues; world-map vertical stacking uses its authored relative Z. Only a currently visible adjacent stack invokes isometric presentation. A remembered loft alone must not cause the camera to tilt.

## Weather

Rain is the first implemented condition. Outdoor exposure affects movement speed, sight range and scent interpretation. Indoor shelter limits those penalties. The display uses restrained rain strokes over the map and a weather label. Settings can suppress the animation without altering simulation. Narration remains legible. Snow, fog and sunshine are extension points, not assumed complete without implementation and tests.

## Acceptance

- Closed doors stop pathing and require explicit interaction.
- Opening a door or crossing an edge produces the correct stop/transition behavior.
- Hidden and out-of-reach targets fail with useful feedback.
- Height and outdoor rain affect simulation, not only visuals.
- Reduced motion preserves weather information through text/status.
- Scent feedback respects current location, weather and permitted perception.

## Provisional tuning

Use forgiving interaction reach and slow deliberate walking. Exact speeds, rain penalties, slope limits and scent range should be tuned during play. These are configuration changes, not reasons to delay the architecture.
