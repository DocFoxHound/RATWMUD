# Atlas faction territory and Chapter sites

This extends the existing `ratw-atlas` JSON version 1 and `RATW_WORLD 1` manifest without changing terrain, portals, player visibility, or cell traversal. It is political **authoring metadata**, not a live DM control channel or a grant of building permission. Runtime settlement migration, diplomatic opinions, and campaign events belong to their own authoritative systems.

## JSON contract

Optional top-level catalogs:

```json
{
  "factions": [
    {"id": "north", "name": "North Wardens", "color": "#6688AA"},
    {"id": "south", "name": "South Wardens", "color": "#CC9988"}
  ],
  "chapters": [{"id": "hearth", "name": "Hearth Chapter"}]
}
```

Each world cell or detached room may contain:

```json
"territory": {
  "region": "north_reach",
  "claims": ["north", "south"],
  "chapter": "hearth"
}
```

The names above are examples, not additions to world canon.

- Missing catalogs mean empty catalogs. Missing territory means exactly `{region:"unassigned", claims:[], chapter:""}`. A provided territory object must include all three keys; `null` is not shorthand for the default.
- IDs, including region IDs, match `^[a-z][a-z0-9_-]{0,47}$`: 1–48 ASCII characters. Region is a grouping key, not a reference to a separate catalog. `unassigned` is the conventional neutral region.
- Factions: at most 64, unique IDs within the faction catalog, exactly `id`, `name`, and `color`. Color is exactly six hexadecimal digits following `#`; upper- and lowercase are accepted.
- Chapters: at most 128, unique IDs within the Chapter catalog, exactly `id` and `name`.
- Names are non-whitespace Unicode text, up to 120 Unicode code points; control codes and unpaired surrogates are rejected. Quotes and backslashes are safely escaped on manifest export.
- Territory objects accept exactly `region`, `claims`, and `chapter`. No unknown fields are silently dropped.
- `claims` is an array of up to 64 distinct faction IDs, all present in `factions`. Order has no political meaning and is normalized lexicographically.
- `chapter` is either an existing Chapter ID or the empty string. One site association per cell; a Chapter can have sites in many cells, including detached interiors.
- Catalog namespaces are independent: a faction, Chapter, and cell may share an ID. References resolve against the appropriate namespace.
- Overlapping claims are intentional. Assigning a Chapter site does **not** delete faction claims, establish faction membership, resolve contested ownership, or imply permission to build.

The JavaScript model produces independent copies with explicit defaults. The Python exporter preserves absent legacy properties in `atlas.json` and sorts provided claim arrays in its validated copy; neither mutates its input. Saving in Atlas may therefore add empty catalogs and neutral territory objects to old JSON, while its runtime export stays equivalent.

## Partition behavior

Splitting a cell or cutting it into smaller cells copies its region, complete claim set, and Chapter site independently into each child. Changing one child cannot mutate a sibling through a shared reference. Terrain, spawn, and link anchors use the existing remapping rules.

Merges and re-cuts that combine cells require identical normalized territory metadata, in addition to existing lighting, elevation, exposure, and weather compatibility. Conflicting region IDs, claims, or Chapter sites reject the whole operation atomically. Different claim ordering is not a conflict. A cut may not extend assigned political metadata into previously uncovered canvas: cut smaller neutral cells first, then explicitly assign their territory. Nothing silently chooses a winner or drops a claim.

Catalog deletion is refused while **any** world cell or detached room references the entry. An editor must explicitly remove/reassign those references first. Updating a catalog label or color retains its stable ID; IDs cannot be renamed through the UI.

## Runtime manifest contract

After the unchanged `RATW_WORLD 1` header, export all faction records, then all Chapter records:

```text
faction "north" "North Wardens" "#6688AA"
faction "south" "South Wardens" "#CC9988"
chapter "hearth" "Hearth Chapter"
```

Each existing `cell` record is immediately followed by its territory record, **if non-default**:

```text
cell "field_1" "cells/field_1.cell"
territory "field_1" "north_reach" "hearth" 2 "north" "south"
cell "field_2" "cells/field_2.cell"
territory "field_2" "north_reach" "-" 0
```

The grammar is:

```text
faction   QUOTED_ID QUOTED_NAME QUOTED_HEX_COLOR
chapter   QUOTED_ID QUOTED_NAME
territory QUOTED_CELL_ID QUOTED_REGION_ID QUOTED_CHAPTER_OR_DASH CLAIM_COUNT [QUOTED_FACTION_ID ...]
```

`"-"` denotes no Chapter. `CLAIM_COUNT` is an integer 0–64 and must match the number of following IDs exactly. Claims are sorted. Detached rooms use the same `cell`/`territory` records. Spawn and reciprocal door records retain their existing format and follow cell records. Unchanged terrain and lighting remain in `.cell` files.

Empty catalogs produce no records. A default unassigned cell produces no territory record. Thus old projects without political settings retain their previous manifest and cell-file bytes. A territory-enabled export requires a runtime supporting these additional record types; an older strict importer will reject them rather than misinterpret them.

## Editor interaction

1. Open **Territory & Chapter sites → Catalogs** to create/edit factions and Chapters.
2. Select a world cell or detached room. Enter a region ID, check any number of faction claims, and optionally assign a Chapter site.
3. Choose **Apply territory to selection**. Shift-select multiple cells for bulk application; replacing a multiple-cell selection requires confirmation. Mixed selections explicitly say which displayed values will replace all selected settings.
4. **Faction claims & Chapter sites** toggles the authoring overlay. Faction colors stripe contested cells; the bottom color strip and legend show each claim. Chapter sites receive a separate diamond/name label when zoom permits. Cell detail supports the same overlay.
5. Undo/redo, local draft recovery, Save JSON, reopen, and export retain political metadata. Nothing in Atlas contacts the live game or alters player fog.

## Verification

Run:

```sh
node --test Editor/src/model/*.test.mjs
python3 -m unittest discover -s tools -p test_map_editor.py
npm --prefix Editor run typecheck
python3 -m py_compile tools/map_editor.py tools/test_map_editor.py
```

Current authoring coverage: **49 JavaScript tests** and **30 Python tests**, all passing. New cases cover legacy compatibility, exact catalog bounds, duplicate and dangling references, hostile input, immutable copies, detached-room references, delete guards, atomic bulk edits, split/re-cut inheritance, every political merge conflict, claim-order equivalence, uncovered-canvas expansion, escaping, export round trips, and unchanged terrain/portal/spawn geometry. These are model/export/HTTP tests and syntax checks; they are not a claim of browser visual verification or live diplomatic simulation.
