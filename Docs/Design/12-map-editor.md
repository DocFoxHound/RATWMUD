# Atlas Workshop: separate two-tier map authoring

Status: implemented first authoring slice, 2026-09-21. This document describes the authoring workflow and boundaries; verification results belong in the test report.

## Purpose and gameplay boundary

Authors should draw a continuous landscape before deciding exactly where the game's cells begin and end. Rivers, paths, terrain, and elevation have one source of truth across a large canvas. Cuts organize that geography into playable cells without redrawing it. Drilling into a cell then gives a focused detail view and connections to off-map interiors.

This is a separate local-browser editor, not an in-game construction system. It does not alter Chapter building, player permissions, movement rules, perception, or the roleplay interface. The runtime still loads distinct cells. A player's local map displays only the current cell; the world map continues to obey sight, adjacency, elevation, and permanent exploration memory. Seeing the whole atlas in the authoring tool does not grant that knowledge to a player.

## Workflow

1. Start the local host with `python3 tools/map_editor.py serve` and open the printed loopback URL. The tool requires Python's standard library and a browser, not a running Unreal editor or game server.
2. Create an atlas or open an `.atlas.json` file. Paint continuous terrain before cutting, or leave the default cuts visible while drawing across them. Pan, zoom, and elevation tint help work at different scales.
3. Cut the canvas into the default **32×24** cells or choose another rectangular grid. Exact remainders of at least four tiles are allowed; one-to-three-tile slivers are rejected with an explanation.
4. Select neighboring cells to merge a filled rectangle, or split a cell horizontally or vertically. Terrain stays at its original coordinates. Review the warning when merging different names/descriptions or recutting edited metadata.
5. Double-click a cell, choose **Drill down**, or use the cell selector for a focused local view. Paint terrain and height details without accidentally drawing into a neighboring cell. Change that cell's name, scene description, Z level, shelter, weather, artificial light, daylight access and light tone; apply details before switching cells.
6. Add a detached interior and connect it with a door, passage, or stairs. Pick both local endpoints on their maps or use the connection form; the tool creates the reciprocal link rather than requiring two separately maintained records.
7. Place the player spawn, inspect Preflight, and save the editable JSON. Export a world ZIP when validation succeeds. Extract each exported version into a new directory and launch a separate playtest with its absolute `world.ratw` path.

An initial cut chooses the first traversable spawn if one exists. It is a convenient starting point, not a level-design recommendation; the author can move it. An all-solid or uncut draft can be saved as JSON, but cannot be exported as a playable world until its partition and spawn are valid.

## Data ownership and continuity

The authoring document contains one shared terrain grid and sparse elevation overrides for the world canvas. World cells contain only rectangular bounds and metadata. They may not embed an independent copy of terrain. Detached rooms own their own grids because they are intentionally outside the main canvas.

| Authoring element | Stored coordinates | Effect of cut, split, or merge |
| --- | --- | --- |
| World terrain and elevation | Global canvas tile indices | Unchanged; no copied borders or inserted walls |
| World-cell rectangle | Global origin, local width/height | Repartitioned; IDs retained where possible |
| Link endpoint or spawn in a world cell | Cell ID plus local tile index | Resolved to global coordinates, then reassigned to its new owning cell |
| Detached room and its anchors | Independent local tile indices | Unchanged by world-canvas cuts |

Repartitioning is transactional. A rejected operation leaves terrain, metadata, links, and spawn exactly as they were. Merging requires a filled rectangle, so disconnected selections and L shapes are rejected. Z, outdoors, weather and effective lighting profiles must agree before cells are combined. The first selected cell supplies the surviving name and description; differing text is not silently discarded without a UI warning. Splits copy lighting into independent child profiles; recuts across conflicting profiles are rejected.

Lighting levels are 0–1. Set artificial light and daylight access both to zero for a sealed dark cellar; use 0/1 for a window-lit room, or 1/1 with warm tone for a tavern that remains clear and glows after sunset. Outdoor cells retain these settings but use sky illumination in play. These are whole-cell ambient profiles, not placed lamps or window geometry. The editor does not preview the player's clock/perception: use an exported playtest to judge atmosphere.

A link whose two endpoints would become part of the same merged cell is not deleted or converted implicitly. The author must remove or relocate it first. Likewise, moving a boundary cannot silently strand an anchor or remove its only safe arrival tile.

Cell IDs are useful persistent identifiers, but a topological edit can legitimately replace or reparent them. This is why an authoring operation is not a live save migration.

## Elevation, links, and seams

The glyph map is still a terrain tileset. Wolves move freely over it according to the existing movement simulation. The editor changes content, not that control scheme.

Elevation overrides use quarter-tile increments from −16 to +16. An absent override retains the glyph's game default: `:` is +0.25, `^` is +0.5, and other supported glyphs are level. An explicit zero overrides a raised glyph; choosing **Glyph default** removes the override. Cell Z describes placement in the larger world, separately from local tile elevation.

Explicit links connect distinct cells, including detached interiors. A link endpoint must be traversable, unique, and have a cardinally neighboring free arrival tile whose elevation differs by at most 0.55. Doors export a `+` marker while preserving the authored elevation. Stairs export `^`, defaulting to +0.5 unless explicitly overridden. Stairs and passages are always open. Painting a marker alone does not create a functioning link.

Export automatically generates reciprocal open boundary passages wherever touching world cells have compatible traversable edge tiles on the same Z level. No decorative door or new wall is inserted along a cut. An explicit endpoint takes precedence over an automatic seam. A height discontinuity greater than 0.55, a solid tile, or a different Z prevents an automatic connection at that position.

These generated passages preserve the established runtime behavior: reaching an open edge changes cell automatically, preserves the connection point/lateral offset where safe, and stops movement. Closed explicit doors still require **Open**, and cross-cell door traversal still lands at the authored arrival. Cutting a continuous landscape does not make the local gameplay renderer a continuous-world camera.

## Architecture and validation

- `Editor/model.mjs` is the pure editing model. Its public mutators validate a candidate and commit only on success. Selection uses IDs because successful edits replace nested cell objects.
- `Editor/app.mjs`, `Editor/index.html`, and `Editor/style.css` provide the canvas, inspector, drill-down views, links, undo/redo, JSON downloads, and browser-local recovery.
- `tools/map_editor.py` serves only the fixed editor assets on loopback and performs authoritative export validation. It never opens the game's save database.
- `Source/RATWMUD/Core/RatwAuthoring.cpp` independently validates the exported manifest and its cell files into a candidate world before replacing runtime content.

Client Preflight is useful immediate feedback, not a security or correctness boundary. The Python exporter validates geometry, fields, IDs, coverage, heights, links, arrivals, spawn, and size limits again. Runtime import validates the serialized package independently, including reciprocal fixtures and cell-file containment within the export directory.

The local host restricts accepted Host/Origin values, uses a random session token for export requests, bounds JSON request size, and serves a fixed asset allowlist. No remote service, paid model provider, live player connection, or world-administration credential is needed.

## Saving, recovery, and safe playtests

One completed brush drag is one undo step; the UI retains a bounded editing history. Browser-local drafts provide recovery, but a downloaded JSON file is the portable source of truth and the recommended backup. Clearing browser data can remove drafts. Draft saving does not write to the repository or a running world.

Each ZIP contains `atlas.json`, `world.ratw`, separate `cells/<id>.cell` files, and launch notes. It is a content snapshot, not a patch applied to another package. CLI export requires a new output directory and refuses to overwrite an existing directory. The tool does not hot-reload the game or publish content.

Launch custom content explicitly:

```bash
bash tools/play.sh -RatwWorld=/absolute/path/to/new-export/world.ratw
```

The game chooses a separate default custom save keyed by the manifest path. A fresh export directory therefore also avoids accidentally reusing the preceding version's default save. If deliberately overriding `-RatwSave`, use a fresh path for a changed topology. Reusing old exploration, actors, or fixture state against recut cells is not supported. Invalid custom content fails visibly rather than silently switching to the demonstration world.

## Current limits and deliberate omissions

| Limit | First slice |
| --- | --- |
| Atlas width/height | 4–256 tiles each |
| Individual cell/room dimensions | 4–256 tiles each |
| Combined world cells and detached rooms | 256 |
| Total authored tiles, including interiors | 262,144 |
| Explicit links | 2,048 |
| Exported directional fixtures, including seams | 65,536 |
| JSON upload | 8 MiB |
| Names | Non-whitespace text, at most 120 Unicode code points |
| Scene description | At most 4,096 Unicode code points; LF allowed |
| World positions and Z | Finite values within ±1,000,000 |

Names reject control characters. Descriptions may be empty and retain line breaks in authoring JSON; exported single-line scene headers fold those breaks to spaces. Unpaired UTF-16 surrogates and malformed data are rejected, while valid Unicode characters are supported.

World cells are rectangular. Detached interiors are created independently, not split or merged through the world partition tools. Existing atlas/room resizing, irregular polygons, multiple overlapping full-world canvases, terrain brushes beyond the supported glyph set, prefab placement, NPC/item spawning, procedural generation, collaborative editing, content publishing, and live save migration are not included. Outdoor weather and shelter can be authored; this slice does not supply a wind-field painting tool or a connected weather-front simulation.

## Acceptance criteria

- A path painted across a proposed boundary has identical terrain before and after cutting.
- Splitting, merging, and recutting preserve global spawn/link positions and sparse heights without silently removing links.
- Invalid geometry, metadata conflict, same-cell portals, or blocked arrivals reject without partial changes.
- Fine editing affects only the selected cell; detached rooms remain independent of the atlas partition.
- Door creation produces two reciprocal fixtures with valid arrival positions.
- Saved JSON reopens the editable canvas; exported content loads as separate runtime cells.
- A custom-map playtest preserves local-map isolation, explicit door actions, automatic edge traversal, and stop-after-transition behavior.
- Malformed inputs, unsupported size, path traversal, or broken reciprocal links fail safely.
- Editing and exporting do not mutate the demonstration save, a running game, or a previous export directory.
