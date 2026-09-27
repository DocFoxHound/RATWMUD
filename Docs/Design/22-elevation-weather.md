# 22 — Elevation and Weather Presentation

**Status:** Implemented 2026-09-26. Supersedes the quarter-step elevation rules in `12-map-editor.md` and the particle weather in `03-client-ui-input.md`.

## Decisions

- The map stays a top-down glyph tilemap. We rejected a perspective or true-3D view (lower tiles shrinking, higher tiles growing). It would break square tiles, click targets and panning, and it works against "ASCII as art direction" in `VISION.md`.
- Height is a **heightfield per cell**: one height per tile, in **half-tile steps** from −16 to +16. Quarter steps are retired. Half steps stay available for tables, platforms and future jumping. Caves, bridges and overhangs (two floors over the same ground) remain separate cells, linked through the cell's `world` Z.
- Height is shown mainly through **what a wolf can see**. Relief shading and edge strokes add a light explicit cue on top. Atlas and the Dungeon Master tool show height explicitly, because authors and DMs can't rely on sight.
- Weather uses **stylized art painted in code** (`RatwWeatherArt`). It is not imported from an asset pack, and no Content assets or engine plugins are needed.

## Movement

| Change in height between tiles | Walkable? |
| --- | --- |
| ½ or less | Yes, freely |
| Exactly 1 | Only if either tile is a `:` Slope or `^` Stairs |
| More than 1 | Never: a ledge or drop |
| Into a `%` Cliff tile | Never, whatever its height |

The rule is `stepAllowed` in `RatwWorld.cpp`. `World::passable` takes the tile being left, so a slope works in both directions. Pathfinding, travel and collision all use the same rule. Link arrival tolerance stays at 0.55.

Glyph defaults: `^` Stairs is +½. `:` Slope and `%` Cliff have no height of their own; the author gives them the height they need. A cliff is solid but not opaque: it hides what lies behind it only through its height.

## Sight

`World::lineOfSight` traces from the observer's eye, 0.8 above their tile, to the middle of the target tile, 0.5 above it. Any tile between them that rises above that line blocks sight. The target tile itself is always seen, so the face of a hill or cliff is visible while the ground behind it is not. As a result:

- A rise hides the ground behind it.
- A taller peak behind a lower rise shows above it.
- From higher ground a wolf sees over rises.
- A plateau top is hidden from below its cliff.
- A wolf at the cliff's edge sees the lowland, but stepping back from the edge hides the ground directly below.

Opaque tiles and closed doors still block sight as before. Heights are otherwise unchanged in hearing, where they already added to acoustic distance.

## Game client

- **Relief:** each visible tile's floor is tinted by its height relative to the player's own tile. Higher ground is warmer and lighter; lower ground is darker. A hillshade lit from the north-west is taken from the neighbouring tiles. Glyphs above the wolf lift up to 2 px, and glyphs below it sink by the same.
- **Edges** are drawn by how they can be crossed:
  - a faint contour for a half step;
  - a warm line where a slope or stairs make a full step walkable;
  - a heavy rim plus a cast shadow on the low side for a ledge or cliff.
- **Readout:** "GROUND +1½" under the compass.
- **Weather layers** (`SRatwGame::WeatherLayers`), each a tiled sheet with drift, scale, angle and tint:
  - **Clear/overcast day:** cloud shadows drift with the wind and vanish at night. Overcast adds a second, denser layer and a grey wash.
  - **Rain:** two depths of streaks, slanted by the wind, plus ground splashes.
  - **Storm:** three depths of streaks, a dark grade, and a dim lightning flash (at most 20% brightness, never under reduced motion).
  - **Snow:** three depths of flakes, each swaying on its own phase.
  - **Fog:** two banks of mist drifting on different paths.
  - **Sandstorm:** an ochre grade, a mist of dust, and two layers of sand streaks turned to face the wind.
  - **Night:** beyond the wolf's current sight radius (27 × the server's sight factor), the ground fades into darkness.
- **Reduced motion** freezes every layer in place and disables lightning. Weather never draws indoors, on the world map, outside the cell, or as a click target.

## Server weather

New kinds are added to the end of the enum (it is saved by number): `overcast`, `storm`, `sandstorm`.

| Weather | Sight | Hearing | Scent | Movement |
| --- | --- | --- | --- | --- |
| Overcast | ×0.92 | — | — | — |
| Storm | ×0.60 | 0.50 | 0.55 | 0.75 |
| Sandstorm | ×0.30 | 0.60 | 0.35 | 0.65 |

Seasonal weather still only generates the original four kinds. The new ones are set by authors in Atlas or by the DM, until climates that produce them exist. Migration `0019_cliffs_and_weather.sql` lets Postgres accept `%` and the new weather names.

## Authoring tools

Atlas and the DM map share `Editor/src/lib/elevation.ts`:

- **Off / Shading / Full** switch (key `E`), with a legend.
- Height tint and hillshade.
- Edge strokes by walkability: step, slope, ledge, cliff hatching.
- Height labels when zoomed in.
- A height readout for the hovered tile.
- Atlas also has raise/lower-by-½ height brushes.

## Later work

- Jumping across 1-step ledges (the half-step grid leaves room for it).
- A small sight-range bonus for high ground.
- Climates that bring storms and sandstorms seasonally.
- Mist that settles in low ground.
- A per-tile slope direction, if authors want ramps that only climb one way.
