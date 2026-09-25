# Atlas Workshop verification

Run: September 21, 2026. Linux x64; Python standard library, Node's built-in test
runner, GCC/Clang portable builds, Unreal Engine 5.8.2 and packaged Linux Game.

This is the initial editor milestone. The [lighting follow-up](LIGHTING_TEST_REPORT.md)
adds whole-cell light authoring and raises editor coverage to 37 JS and 23 Python
tests. Shared automation/build artifacts reflect the latest run; the named logs
below preserve the earlier milestone's evidence.

## Executed checks

| Layer | Result | Evidence / command |
| --- | --- | --- |
| Pure editing model | 34/34 tests pass | `node --test Editor/model.test.mjs` |
| Exporter and local HTTP service | 21/21 tests pass | `python3 tools/test_map_editor.py` |
| Portable runtime | 383 existing world assertions, 188 authoring checks, 42 runtime checks; 3/3 executables pass | `ctest --test-dir build-core --output-on-failure` |
| Memory/undefined-behavior checks | All three suites pass, no sanitizer findings | `build-core-clang-sanitize/Testing/Temporary/LastTest.log` |
| Native Unreal automation | 14/14 pass, no test warnings | `artifacts/automation/index.json`, `artifacts/logs/automation-atlas.log` |
| Actual export-to-native playtest | Pass, fresh screenshots verified and visually inspected | `python3 tools/editor_smoke.py --engine`, `artifacts/logs/editor-native-result.log` |
| Rebuilt Linux package | Build/cook/stage/archive succeed; exported-world headless playtest passes | `artifacts/logs/package-atlas.log`, `artifacts/logs/editor-packaged-result.log` |
| Existing gameplay regressions | Native movement, two-client networking, two-client scent, three-stage persistence all pass | `artifacts/logs/atlas-regression-*-run.log` and associated process logs / JSON |

### Editing and export coverage

Tests cover continuous terrain across cuts; standard/custom grids and small
remainder rejection; horizontal/vertical split; rectangular merge; metadata
conflicts; exact global link/spawn remapping; independent room terrain; quarter-unit
heights; solid/steep/occupied arrivals; reciprocal links; transactional rejection;
normalization; deep cloning; and deterministic randomized repartition checks.

The exporter is tested independently for full coverage, bounds, unique IDs,
limits, valid Unicode and invalid surrogates, strict JSON, automatic reciprocal
seams, explicit endpoint priority, Z/height separation, preserved glyphs, valid
ZIP entries, and refusal to overwrite an existing export directory. HTTP tests
exercise actual loopback requests: fixed static allowlist, Host/Origin checks,
session requirement, body/content-type limits and readable malformed-input errors.

The C++ importer rejects invalid manifests atomically, validates cell references
and reciprocal doors, constrains file paths, rejects symlink escapes and oversized
content, and checks safe traversal. Existing world behavior remains covered.

### Browser interaction checks

Used the actual editor at `http://127.0.0.1:8765`, with synthetic demonstration data:

- Merged two adjacent cells through the confirmation dialog, split the result,
  and undid both operations to recover the original four cells and names.
- Painted a terrain stroke across the boundary and undid the complete stroke.
- Drilled into a detached tavern, inspected its map/metadata, and canceled a
  connection-removal confirmation without removing the connection.
- Created a second detached room and a reciprocal stair link to the tavern.
  Preflight changed from unreachable-room warning to ready-to-export.
- Requested a world ZIP through the real validating export service; the editor
  reported success. Download completion is not treated as a confirmed disk save.
- Reloaded and explicitly restored the locally saved draft. Draft recovery stores
  the project, not the undo history. Captured both actual editor views.
- Rechecked final save protection by requesting Save JSON, choosing another atlas
  through the file picker, and verifying that replacement still required explicit
  confirmation. Cancel retained the existing atlas. Stairs visibly showed the
  checked, disabled **Always open** control; final snapshot export succeeded.

Screenshots are in `artifacts/editor/`; the [gallery](SCREENSHOTS.md) includes
both authoring and runtime evidence. No browser console errors were observed
during the inspected workflows. This is a desktop-browser check, not a complete
accessibility, mobile, or cross-browser certification.
The embedded browser's screenshot endpoint stopped responding after the final
reload, although its interactive controls remained available. The retained editor
captures were taken successfully earlier in this same session; runtime captures
were refreshed after the final Unreal UI change. No substitute mockups were used.

### End-to-end gameplay evidence

`tools/editor_smoke.py` calls the actual JavaScript model to paint across a future
cut, add a height override, cut four world cells, create an interior door, merge
and split again. It asserts terrain continuity and unchanged global doorway
coordinates, then sends that document through the real Python exporter and C++
importer. The fixture has five cells including its interior and 226 directional
fixtures including invisible automatic seams.

The Unreal scenario crosses a boundary, checks the 32×24 destination local cell
and matching arrival point, waits to verify movement stopped, then walks to a
closed door and opens it explicitly. It checks the separate 16×12 interior and
its arrival point and again verifies the stop. Native graphical and packaged
headless runs both pass. Native captures are `13-authored-cell-runtime.png` and
`14-authored-interior-runtime.png`.

Regression runs use disposable saves. Network clients each received the other's
IC/OOC and long post. Scent preserved anonymous cues, hidden-target protection
and wind reversal. Persistence retained character state and conversation through
separate process restarts; aging only the synthetic conversation produced one
permanent summary after the inactivity deadline. No regular player save changed.

## Issues found and corrected

1. Automatic edge seams initially made observation/path checks scan too many
   fixtures. Blocking-fixture lookup now indexes only relevant cell/tile entries
   and excludes permanent open seams. The same debug integration probe improved
   from roughly 72 seconds to 1.4 seconds. This is a small fixture comparison,
   not a population-scale performance claim.
2. Palette labels originally implied a tree/road meaning inconsistent with runtime
   glyphs. Labels and demonstration terrain now match actual collision and cost.
3. Browser-native confirmations hung the embedded test browser. Accessible HTML
   dialogs now handle destructive editing confirmations; cancellation was tested.
4. Stairs, height overrides, spawn/arrival reservations, Unicode limits and blank
   names exposed edge cases between model/export/import. Contracts and tests now
   agree; invalid operations retain the original project.
5. An imported room without prose displayed a false connecting message. It now
   reports that no scene description has been authored.
6. Review found download-state and asynchronous export races. JSON download
   requests no longer clear edit protection; export captures one immutable content
   and filename snapshot before awaiting the local service. Recovered drafts retain
   protection against accidental replacement.

## Boundaries of this result

This is a first rectangular-map authoring slice, not production world deployment.
The authoring atlas is capped at 256×256 and 256 total cells/rooms. There is no
legacy `.cell` import, NPC/item placement, collaboration, live world mutation,
save migration, overlapping world layers or irregular cell polygons. Apply cell
metadata before switching cells or using other tools. Browser drafts are a
convenience; save JSON backups and verify the downloads actually completed.

Playtest new topology from a new export path or a fresh explicit `-RatwSave`.
Default custom saves are keyed by manifest path, not content. The existing
demonstration world and normal launch remain available. No paid provider calls,
remote publication, git commit or push were performed for this editor addition.
