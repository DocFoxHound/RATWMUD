# Weather and daylight verification

September 21, 2026 · Linux x64 · Unreal Engine 5.8.2.

The design and provisional tuning are in
[environment design](Design/10-interactions-environment.md). These tests use
synthetic characters and disposable saves, not the ordinary player world.

This records the initial weather milestone. The later [interior lighting and
cell-atmosphere report](LIGHTING_TEST_REPORT.md) supersedes its constant-interior
lighting limitation and aggregate counts, and refreshes the weather screenshots.
`artifacts/automation/index.json` always represents the latest run, not this
historical milestone.

## Executed results

| Check | Result |
| --- | --- |
| Normal portable / Clang ASan+UBSan | All six suites pass, 968 checks, no sanitizer findings |
| Final Unreal module / automation | Build succeeds; 18/18 tests pass, zero warnings, exit 0 |
| Final native graphical weather + restart | Both pass; all six fresh screenshots visually inspected |
| Final Linux package | Build/cook/stage/archive pass, 101 seconds |
| Packaged headless weather + restart | Both pass |
| Packaged remembered travel | Pass |
| Existing native regressions | Travel, two-client scent/privacy, networking, movement/posture, and three-stage NPC persistence pass |
| Offline NPC bridge | 35 tests pass; no paid provider calls |

Final native evidence: `artifacts/logs/automation-weather.log` and
`artifacts/automation/index.json`. Package evidence:
`artifacts/logs/package-weather.log`, `weather-packaged-result.log`, and
`weather-packaged-travel-regression.log`. Existing regression results are
`artifacts/logs/weather-*-regression.log`. All test game processes were stopped.

## Portable simulation

All six portable suites pass normally and with Clang AddressSanitizer and
UndefinedBehaviorSanitizer: **968 assertions/checks**, including 127 new weather
checks. Normal runtime: 50.36 seconds; sanitizer runtime: 179.22 seconds.

Coverage includes smooth phase boundaries, midnight and full-day wrap, strict
setters and atomic clock restore, legacy noon defaults, persisted clock/gust
agreement, actual server filtering and dim remembered terrain, actual speech,
pawstep and scent changes, NPC/player perception parity, and every weather ×
shelter combination applied to real movement and stamina. Rain/night penalties
stack for vision; darkness does not silently weaken hearing or smell. Fog's
small scent increase is tested as a game rule, not asserted as physical accuracy.

Evidence: `artifacts/logs/weather-core-regression.log` and
`artifacts/logs/weather-core-sanitizers.log`.

## Native tests

`RATW.Environment.ClockWeatherAndWire` covers authoritative environment JSON,
strict enum/clock persistence and malformed-data rejection, safe legacy values,
development-only clock/weather authority, shelter, and condition-aware NPC
fallback dialogue. Invalid weather values cannot silently become clear weather
or overflow an enum conversion.

`RATW.UI.EnvironmentalPresentation` covers clock/phase and numeric sanitization,
wind reversal, distinct rain/snow/fog geometry, reduced-motion invariance,
indoor/world-map suppression, gradient-axis conventions, preserved hidden-entity
and hit-target privacy, and development controls. The indoor label names shelter
without treating unused room-weather metadata as knowledge of adjacent weather.

Executed native outcomes and the current automation report are recorded in
[the main test report](TEST_REPORT.md).

## Real game scenario

```bash
python3 tools/weather_smoke.py
python3 tools/weather_smoke.py --packaged --headless
```

The harness uses the real Atlas JS model → Python exporter → C++ importer to
create a 32×24 glade with brush, a path, pond, elevation and occluding stone walls,
plus a detached shelter. The same stationary observer experiences clear day,
rain, snow, fog, and clear night. Assertions verify all appropriate environment
factors, reduced authoritative visible-tile counts, independent nighttime
hearing/scent, and protected shelter after an explicit door Open.

Graphical mode captures six fresh viewport PNGs; headless mode skips captures.
A second actual game process reopens the same isolated save and verifies shared
nighttime, persisted weather, and neutral indoor factors. Each run requires fresh
scenario JSON and, in graphical mode, fresh nonempty screenshots. See the
[screenshot gallery](SCREENSHOTS.md).

Evidence is under `artifacts/logs/weather-*-result.log`,
`artifacts/logs/weather-*-smoke.log`, and `weather-ash.json` /
`weather-restore-ash.json` in each evidence directory.

## Review-driven corrections

- Replaced sparse precipitation and a flat fog tint with layered map-only effects.
- Corrected Slate's gradient orientation convention after a capture exposed hard
  fog bands. Checked the next rendered capture, not just the helper's math.
- Removed an indoor header that misleadingly described unused room metadata as
  outside weather. Rebuilt affected UI objects and tests after a concurrent edit
  during an earlier build left stale objects; final evidence requires a fresh run.
- NPC fallback dialogue now follows supplied local conditions instead of always
  describing rain. Generated dialogue receives those same bounded scene facts.
- Hardened persisted weather enums and clock fields against coerced booleans,
  strings, fractions, extreme values and invalid types.

## Limits

This is a bounded developer playtest, not human visual/balance approval,
population-scale profiling, internet-latency testing or a full accessibility
audit. Linux is the only verified platform. The current weather is authored per
cell, not a regional storm simulation; there is no accumulation, exposure damage,
sound playback, local lamp/shadow field, moon cycle, dark adaptation or lingering
trail simulation. Indoor cells currently have steady light. NPC demo schedules
are not yet synchronized with dawn/dusk. No model-provider calls were needed;
the 35 offline bridge tests remain the provider regression gate for this change.
