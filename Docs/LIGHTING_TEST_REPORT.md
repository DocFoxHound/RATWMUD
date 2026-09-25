# Cell atmosphere and interior lighting verification

Date: 2026-09-21. Linux x64, Unreal Engine 5.8.2. Scope: whole-cell ambient light,
daylight access, cell-edge atmosphere, editor authoring and persistence.

## Portable and authoring evidence

| Check | Executed result | Evidence |
| --- | --- | --- |
| Seven portable suites | 1,067 checks passed, 50.45 seconds | `artifacts/logs/lighting-core-ctest.log` |
| Clang ASan + UBSan | All seven passed, zero findings, 193.14 seconds | `artifacts/logs/lighting-core-asan.log` |
| JS editor model | 37 tests passed | `artifacts/logs/lighting-editor-model.log` |
| Python exporter/HTTP | 23 tests passed | `artifacts/logs/lighting-editor-export.log` |
| Offline NPC provider bridge | 35 tests passed; no external requests | `artifacts/logs/lighting-npc-bridge-regression.log` |
| Final Unreal module | Build succeeded, 21.44 seconds | `artifacts/logs/lighting-build.log` |
| Final isolated native automation | 20/20 passed, zero test warnings, exit 0 | `artifacts/automation/index.json`, `artifacts/logs/automation-lighting.log` |
| Final native lighting + separate-process restart | Passed; six fresh 1600×1000 captures | `artifacts/logs/lighting-native-result.log`, `lighting-native-smoke.log`, `lighting-restore-native-smoke.log` |
| Final native weather + separate-process restart | Passed; six refreshed weather captures | `artifacts/logs/lighting-weather-regression.log`, `weather-native-smoke.log`, `weather-restore-native-smoke.log` |
| Final Linux package | Build/cook/stage/archive succeeded, 99 seconds | `artifacts/logs/package-lighting.log`, `artifacts/package/Linux/` |
| Standalone packaged lighting + restart | Passed graphically; six fresh packaged captures; warm-night render visually checked | `artifacts/logs/lighting-packaged-result.log`, `artifacts/packaged-evidence/lighting-ash.json`, `lighting-restore-ash.json` |
| Standalone packaged weather + restart | Passed headlessly | `artifacts/logs/lighting-packaged-weather-regression.log` |
| Standalone packaged three-cell travel | Passed headlessly | `artifacts/logs/lighting-packaged-travel-regression.log` |
| Atlas import/transition regression | Paint/cut/merge/split retained coordinates; native crossing and detached-room door passed | `artifacts/logs/lighting-atlas-regression.log` |
| Three-cell travel regression | Stamina drain/recovery, route traversal, cancellation and explicit closed-door Open passed | `artifacts/logs/lighting-travel-regression.log` |
| Two-client scent regression | Upwind detection, anonymous sectors, hidden-actor/inspection privacy and wind reversal passed | `artifacts/logs/lighting-scent-regression.log` |

The 99 new core lighting checks cover whole-cell profiles, daylight boundaries,
actual sight/actor/action/inspection behavior, independent hearing/scent/footing,
NPC perception, memory privacy, outdoor invariance, strict authoring, legacy
defaults and atomic save rejection. The other 968 existing checks remain green.

## Browser authoring check

A fresh isolated loopback editor origin was used, leaving existing user tabs and
drafts untouched. In **The Quiet Hearth**, artificial light and daylight access
were set to 0/0 with neutral tone and applied. Switching away and back retained
the profile. The profile was then changed to 0.6/0.2 with cool tone; Preflight
remained export-ready and the UI completed a world-ZIP request. After reload,
**Restore draft** recovered those exact displayed fractional values and tone.
The inspector was visually checked at 1280×720, including its scrollable layout.
The temporary tab and test service were closed. This check confirms the UI/export
request, not the browser's final download location; exported file contents are
independently verified by Python tests and the real runtime fixture.

The pre-existing editor host on port 8765 was intentionally left running with its
user tabs/drafts intact. It must be restarted to load the new Python exporter;
save JSON first, then restart the host and reload the browser. Merely reloading
the old host's page updates frontend files but not its imported exporter code.

## Graphical and restart scenario

`python3 tools/lighting_smoke.py` creates a disposable map through the actual JS
editing model and Python exporter. It starts a real Unreal client in a 20×14
tavern connected to a 32×24 outdoor glade, using a separate test save. Assertions
cover:

- Warm tavern at noon: full illumination, more than 80 visible tiles, no glow.
- Same tavern at night: unchanged visible-tile count, full warm glow.
- Sealed unlit tavern at night and noon: illumination below 0.1, under a quarter
  of the original visible tiles, no glow, unchanged hearing and scent.
- Daylight-only room: clear at noon, dark after sunset.
- Cool artificial light: full vision returns with a different atmosphere.
- Ordinary path movement and explicit door opening into the outdoor night.
- Return to the tavern, set it unlit, close the process, then start a separate
  process and verify the saved dark profile, daylight access and night clock.

Screenshot evidence is freshness-checked rather than accepting previous runs:
`23-tavern-day.png`, `24-tavern-warm-night.png`, `25-tavern-unlit-night.png`,
`26-unlit-cellar-day.png`, `27-tavern-cool-night.png`, and
`28-outdoor-night-edges.png`, under `artifacts/screenshots/`.
See [the gallery](SCREENSHOTS.md) for the comparisons.

Final visual review confirms a smooth warm/cool halo without the earlier seam or
visible contour bands, no daytime interior glow, dark unlit interiors, and dark
outdoor edges. Rain, snow and fog have visibly distinct screen and edge effects.

The weather scenario also revisits clear day, rain, snow, fog, outdoor night and
a sheltered room to exercise weather-edge atmosphere. Normal roleplay text stays
untinted. Its second process checks saved weather/time and shelter.

## Revisions found by verification

The first native render test caught an invalid `TArray` self-append in the halo's
closed outline. Visual inspection then found a seam at polyline end caps and
aliasing in a non-antialiased alternative. The final renderer uses densely
overlapped shader-rounded transparent outlines. A smoke-only command typo was
corrected from `moveTo` to the actual `path` wire command. A concurrent automation
run caught an Unreal asset-registry cache-file race unrelated to dialogue;
automation is rerun alone for final evidence.

## Deliberate limits

This is ambient light for one entire cell, not individual light objects, fuel,
window beams, local shadow casting or dark adaptation. A sealed unlit room keeps
a provisional 8% close-awareness sight floor. Dim remembered terrain is not live
vision; lighting does not expose hidden wolves or remote map state. Small, large
and panned-cell bounds and reduced-motion behavior are checked in native UI
tests; human testing of glow strength, darkness balance and readability under
latency remains open. These runs use fictional content and isolated test saves;
no paid provider call or production save migration is performed.

All test game processes were stopped after completion. The final standalone
graphical lighting pass uses `python3 tools/lighting_smoke.py --packaged`; its
separate-process restore is headless. Full frame-time profiling and human
readability/comfort playtests have not been claimed by these automated results.
