# Atlas editor implementation contract (v1)

Separate local-browser authoring tool, no in-game editor and no live-world writes.
32×24 default cuts. Rectangular cells only; merges must form a filled rectangle.

## Authoring JSON

`{format:"ratw-atlas", version:1, name, width, height, terrain:[row strings], heights:{"x,y":number}, cells:[], rooms:[], links:[], spawn:null}`.
World dimensions 4–256 each. Terrain glyphs `.#,\"T=~:^+%` (space unsupported).
Heights are sparse overrides (-16..16, half-tile multiples; legacy quarter values round to the nearest half on load); unspecified glyph heights retain game defaults (`^` .5, others 0). Weather is one of `clear`, `overcast`, `rain`, `storm`, `fog`, `snow`, `sandstorm`.
Atlas, cell, room, and link names must contain non-whitespace text and have at most120 Unicode code points. Names reject control characters. Descriptions may be empty, allow LF newlines, and have at most4096 Unicode code points; other control characters and unpaired UTF-16 surrogates are rejected. Valid Unicode supplementary characters are supported. Export folds description newlines to spaces in runtime headers while preserving source JSON.
World and detached-room positions/Z must be finite and within±1,000,000; world-cell x/y remain integer canvas indices. At most262144 authored tiles are allowed across the shared canvas and all detached rooms, at most2048 explicit links, and at most65536 exported directional fixtures. JSON uploads are bounded to8MiB.

Atlas v3 (current): world cells are `{id,name,description,x,y,width,height,terrain,heights,z,outdoors,weather,lighting,territory}` with x, y in world tiles within ±10⁹; each holds its own ground and the project has no `terrain`, `heights`, `width`, `height` or `origin`. Cells may not overlap; the world has no edge. v1/v2 documents convert on load. The v2 rules follow. World cells (v2): `{id,name,description,x,y,width,height,z:0,outdoors:true,weather:"clear"}`. Nonoverlapping rectangles must cover entire canvas to export; each at least4×4, at most256×256. Maximum256 total world cells plus rooms. World cells must not contain their own terrain/heights fields; those belong exclusively to the shared canvas. A custom grid remainder of1–3 tiles is rejected rather than producing a sliver.
Detached rooms: `{id,name,description,width,height,terrain:[strings],heights:{},worldX:0,worldY:0,z:1,outdoors:false,weather:"clear"}`. They are not required to tile the atlas. IDs `[a-z][a-z0-9_-]{0,47}`, unique across cells/rooms.
Both kinds accept optional `lighting:{artificial:1,daylightAccess:1,tone:"warm"}`. When present, exactly these three fields are required; levels are finite numbers in 0–1 (not strings or booleans), tone is `warm`, `neutral`, or `cool`. Missing legacy profiles use `1/1/warm`. JS normalization materializes the default; Python preserves omitted legacy source fields in `atlas.json`, while runtime export always writes the effective lighting header. Outdoors retains its profile but uses sky lighting in play. Split/cut children inherit independent profiles; merges and recuts reject conflicting effective lighting, including a mismatch against a legacy default. Lighting never changes terrain coordinates, connections or player map knowledge.
Links: `{id,name,kind:"door"|"passage"|"stairs",a:{cell,x,y},b:{cell,x,y},open:false}`. Endpoint x/y integer local tile indices; endpoints in different existing cells, traversable terrain, and no endpoint reused. Door endpoints export as `+`; stairs as `^`; open passages keep terrain. Arrival is a cardinally neighboring passable tile, not occupied by another endpoint, whose elevation differs by at most.55 from the stamped endpoint. Stairs default to.5 unless a sparse override exists; doors retain their authored elevation even when the glyph changes. Both endpoints generate reciprocal fixtures; stairs/passages always open. Terrain and height edits cannot invalidate an existing endpoint's final compatible arrival.
Spawn: `{cell,x,y}` integer tile location on non-solid terrain, not any explicit link endpoint. UI may choose first valid tile after the first cut. Cutting/splitting/merging remaps links and spawn through global coordinates without moving terrain; block a merge that would turn a cross-cell link into a same-cell portal rather than deleting it silently. World cell ID metadata retained where possible; recuts after metadata edits require explicit UI confirmation and warning. Merges reject conflicting Z/outdoors/weather; the first selected name and description survive, with a UI warning before discarding differing names/descriptions.

## Runtime export

ZIP includes `world.ratw`, `cells/<id>.cell`, original `atlas.json`, `README.txt`.
Existing cell headers plus repeated `height: x y value` before grid, optional `wind: direction strength variable` (0/1), and `lighting: artificial daylightAccess tone`. Export indoor base calm/outdoor .5 east variable. Lighting uses roundtrip-safe numeric precision and is validated again by the runtime loader.

Manifest uses whitespace tokens and C++ quoted strings (`std::quoted`), UTF-8, LF:

```text
RATW_WORLD 1
cell "cell_id" "cells/cell_id.cell"
spawn "cell_id" 2.5 3.5
door "fixture_id" "Door name" "cell_id" 3.5 4.5 "target_cell" 1.5 2.5 "paired_fixture_id" 0 0 0 0 "-"
```

Door tail flags: open, locked, boundary, passage (each0/1), then edge `N/E/S/W/-`. Stairs use IDs starting `stairs_` and open1. All links reciprocal. Automatically generated seams are open boundary passages, one per compatible traversable border tile pair, no rendered door glyph/hit target. Explicit link endpoints take precedence. Seams require same Z and traversable elevation difference ≤.55. Arrival at target edge tile center; preserve lateral offset for automatic passage crossings where safe. Crossings still stop movement; explicit closed doors still require open. Ordinary local map remains current-cell-only. World-map knowledge/perception unchanged.

`World::loadWorldFile(path)` validates into a candidate and replaces only on success. Replaces demo cells/NPCs for custom maps; no NPC authoring in this slice. `-RatwWorld=<absolute manifest>` opts into custom content. Custom content uses a separate save default and rejects invalid imports instead of silently using demo. No hot reload/save migration. Every export is a content snapshot; CLI export requires a new output directory and never rewrites an existing one. Recutting can change IDs/topology: use a fresh export directory/save, not an old world's saved state.

## JS module API: Editor/src/model/model.mjs

Pure named exports, mutating project for editing operations; throw Error on invalid operations, unchanged on failure:

- `createProject(width=64,height=48,name="Untitled atlas")`
- `paint(project, x,y,glyph,size=1,roomId=null)`; size is square brush1..8.
- `setHeight(project,x,y,value,roomId=null)`; null removes override.
- `cutGrid(project,width=32,height=24)`; partition only (no terrain edits).
- `splitCell(project,id,axis,offset)`; axis `x/y`, offset local integer; both children ≥4.
- `mergeCells(project,ids)`; filled rectangle, same Z, coherent metadata; reject invalid/self links.
- `addRoom(project,width=16,height=12,name="New interior")` returns room object.
- `addLink(project,link)` returns link; assign ID if omitted.
- `removeLink(project,id)`.
- `getCell(project,id)` returns authored rect/room; `cellTerrain(project,id)` sliced rows.
- `validate(project)` returns `{errors:[string],warnings:[string]}`; draft without cuts allowed but export-invalid.
- `clone(project)` deep clone; `normalizeProject(value)` validate structure and return safe clone (draft coverage errors permitted, invalid shape/types rejected).

UI owns undo/redo transaction snapshots (one drag=one operation), save/open JSON,
local draft recovery, camera/selection and preview rendering. Python exporter is
the final authoritative validator; client checks are convenience only.
Mutators commit replacement nested objects only after validation; UI selection stores IDs and reacquires cells after edits. `splitCell` returns the two committed children, `mergeCells` returns the committed merged cell, and `getCell` returns null for an unknown ID. `normalizeProject` permits incomplete-coverage/spawn-null drafts but rejects malformed data or broken anchors. It strips unknown non-schema fields, while wrongly embedded world-cell terrain/heights are rejected to prevent silent terrain loss.

## Version 2: world content (people, profession slots, routes, economy)

Atlas files may be `version: 1` or `2`; the editor always writes 2. New
optional top-level fields (absent = empty/default):

- `id`: stable world ID (`[a-z][a-z0-9_-]{0,47}`), the key for roster
  assignments. Older atlases without one use a slug of their name.
- `people`: named NPCs, at most 256. Exactly these keys: `id, name, role
  (merchant|guard|civilian), description, greeting, workLabel (1–40), age
  (0–200), appearance {species, sex, stature, pattern, baseColor, gradientColor,
  markingColor (0–7)}, voice (0–31), hours {start, end} (0–23.75, different,
  may wrap midnight), route ('' or a route ID; guards only), paid, purse, herbs,
  meals, home, work, evening, personality (≤2000), backstory (≤6000)`. Places are
  `{cell, x, y}` open tiles. A merchant's `work` is their counter; customers
  stand on its first open neighbour. IDs `treasury`, `wolf-*`, `player-*` are reserved.
- `slots`: profession slots, at most 256. Exactly `id, name, profession (roster
  profession ID), workLabel ('' = the profession's), hours, route, paid, purse,
  herbs, meals, home, work, evening`.
- `routes`: `[{id, name, posts: [place, …1–64]}]`, at most 128.
- `economy`: `{treasury ≤1e6, storeHerbs, storeMeals ≤1e4, dailyHerbs, dailyMeals ≤1e3}`.
- `herbPatch`: a place or null.

Cut, split and merge remap every place like links and spawn. Deleting a room
is refused while people, slots, posts or markers are inside.

Manifest `RATW_WORLD 2` adds (after cells, doors and spawn; all strings `std::quoted`):

```text
economy 1000 100 50 10 12                      treasury storeHerbs storeMeals dailyHerbs dailyMeals
herbs "cell" 4.5 14.5
route "id" 9 "cell" 30.5 37.5 …                 count, then that many places
resident "id" "Name" "role" "work label" "description" "greeting" age "species" "sex" "stature" "pattern" base gradient marking voice paid start end "route|-" purse herbs meals "home" x y "work" x y "evening" x y
story "id" "personality" "backstory"            optional; follows its resident once
```

Filled profession slots export as ordinary `resident` + `story` records whose
ID is the roster character's ID, so the game's memories follow the character.
`RATW_WORLD 1` is still written for worlds without content.

## Character roster (`Data/Characters/roster.json`)

`{format: "ratw-roster", version: 1, professions: [], characters: []}`, shared by
every world and validated by `tools/roster.py`.

- Profession: `{id, name, behavior (merchant|guard|civilian), workLabel,
  description, hours {start, end}, paid}`.
- Character: `{id, name, age, appearance, voice, description, personality,
  traits (≤12 × ≤40 chars), backstory, greeting, preferences {professionId: 0–3},
  status (active|dead|removed), profession ('' or locked ID), assignment (null |
  {world, slot}), origin (manual|llm)}`.

Assignment (`roster.assign`) runs when a world is exported, played or saved as
a bundled world: existing `{world, slot}` holders keep their slot; an empty
slot takes the active, unassigned character with the highest preference (> 0)
for its profession whose `profession` is empty or the same, ties broken by
already-locked characters then ID. The chosen character's `assignment` and
`profession` are written back. A slot's holder is released if the slot is
deleted or its profession changes (keeping their locked profession); dead or
removed characters release their slot and are never drawn again. Editor saves
of the roster cannot change `assignment`/`profession`, and a character who has
ever held a profession is kept (marked removed) instead of deleted.

`POST /api/roster/generate` asks the configured live-NPC model (fixed OpenAI
endpoint, key kept server-side) for 1–8 characters as strict JSON; the result is
clamped and validated before the editor shows it. Nothing is saved until the
author adds and saves.
