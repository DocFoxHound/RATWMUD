# Verification report

Run: September 20–21, 2026. Host: Linux x64, Unreal Engine 5.8.2,
bundled clang 20.1.8; GCC and Clang used for independent simulation tests.

## Executed results

**Movement smoothness follow-up:** [correction and verification](MOVEMENT_SMOOTHNESS_TEST_REPORT.md)
records the dedicated 20 Hz pose stream, buffered frame interpolation, 36 native
Automation tests, 12 portable suites and real-process movement/network checks.

**Character creator follow-up:** [local accounts, live portraits and verification](CHARACTER_TEST_REPORT.md)
records 33 native Automation tests, 12 portable suites, and the real-process
account/creator/restart/other-player inspection checks. Local-only credentials
are not a production internet authentication claim.

**Policy confirmation follow-up, September 21:** the user settled lunar phases
on game days and aging while logged out. The existing implementation already
matches both. Re-ran `calendar_behaviors` and `aging_behaviors` (2/2 pass), then
`python3 tools/aging_smoke.py --packaged`: register, advance with the character
absent, and return all passed, including exactly one birthday notice/reward and
no repeated welcome grant. Evidence: `artifacts/logs/aging-confirmed-policy.log`
and the refreshed packaged `age-register-ash.json`, `age-advance-birch.json`,
`age-return-ash.json` scenario reports. This follow-up changes design documents,
not runtime code. Natural-death rules and dual map/log combat presentation are
specified but **not implemented or tested as gameplay**; see designs 14 and 18.

**Latest follow-up:** [territory and Storykeeper verification](DM_TEST_REPORT.md)
adds the political authoring model, separate operator application and private
native execution bridge: 11/11 portable and sanitizer suites, 23/23 Unreal
Automation suites, and 23 native end-to-end checks including a real player and
server restart. See that report for current browser/package evidence and limits.

**Previous follow-up:** the four-hour calendar, seasons, lunar light, aging and
deterministic finite economy are covered by `SOCIETY_TEST_REPORT.md` (1,400
portable checks and 22 native test cases). The table below records the preceding
lighting baseline; it is retained as history, not the current feature ceiling.

Follow-up: the user authorized the existing RATW Game provider for NPC testing.
[Live provider setup and report](LIVE_NPC_TEST_REPORT.md) records six successful
bounded synthetic OpenAI requests, including native delivery and recall across
restart and long-term consolidation. The bridge's 35 offline tests, all ten
native automation tests and both portable suites passed after integration.

| Check | Result | Evidence |
| --- | --- | --- |
| Portable CMake/CTest | Pass: 383 world assertions, 188 authoring checks, 151 pace assertions, 77 travel checks, 127 weather checks, 99 lighting checks and 42 runtime checks; 7/7 executables, 1,067 checks | `artifacts/logs/lighting-core-ctest.log` |
| AddressSanitizer + UndefinedBehaviorSanitizer | Pass: all seven portable suites, no reported violations | `artifacts/logs/lighting-core-asan.log` |
| Unreal module | Builds successfully with installed toolchain | `artifacts/logs/lighting-build.log` |
| Unreal Automation | 20/20 tests pass, zero test warnings, exit 0 | `artifacts/automation/index.json`, `artifacts/logs/automation-lighting.log` |
| Separate editor-hosted server + two clients | Both moved and received the other character's IC, local OOC and 16,000-character post | `artifacts/screenshots/network-ash.json`, `network-bracken.json` |
| Packaged Linux Game | Latest interior-lighting/atmosphere build, cook, stage and archive succeed, 99 seconds | `artifacts/logs/package-lighting.log`, `artifacts/package/Linux/` |
| Native and packaged weather/daylight | Five outdoor conditions change factors/visibility; explicit door enters sheltered room; clock/weather survive separate process restart | `weather-ash.json` and `weather-restore-ash.json` in both evidence directories; `artifacts/logs/weather-*-result.log` |
| Native and packaged interior lighting + restart | Lit/daylit/unlit/warm/cool conditions, actual sight change and saved lighting pass; six fresh screenshots in each build; native gallery and packaged warm-night render visually inspected | `artifacts/logs/lighting-native-result.log`, `lighting-packaged-result.log`, `Docs/LIGHTING_TEST_REPORT.md` |
| Native and packaged remembered travel | Sprint drains stamina; three-cell journey survives typing/speech; arrival recovers; Stop cancels; closed door requires Open | `travel-ash.json` in screenshot/packaged-evidence directories, `artifacts/logs/travel-*-result.log` |
| Editor-free packaged multiplayer | Two packaged graphical clients pass against a separate headless listen host | `artifacts/packaged-evidence/network-ash.json`, `network-bracken.json` |
| Native and packaged persistence scenarios | Character position/color/state and active NPC conversation survive process restart; aged inactivity consolidates to one permanent summary | `persist-write-ash.json`, `persist-read-ash.json`, `persist-aged-ash.json` in each evidence directory |
| Native and packaged walkthrough | Path to pantry; explicit open; vertical overview; enter/leave loft; enter rainy yard | `walkthrough-ash.json` in each evidence directory |
| Screenshot gallery | Real viewport captures of local/world maps, sheet, inventory, settings and text-first layout | `Docs/SCREENSHOTS.md` |
| Optimized Server target | Not supported by installed engine distribution; explicit build failure confirmed | `artifacts/logs/build-server-target.log` |

The packaged host uses the Game target with `?listen -RatwHeadlessHost -nullrhi`.
It has no local character and accepts remote clients, but is not a Server-target
binary. Both default server launchers bind to loopback only.

## Native automation coverage

- `RATW.Dialogue.AuthoredFallback`: truthful offline label, source-grounded memory
  reply, malformed URL/user-info bypass rejection.
- `RATW.Dialogue.LocalHttpContract`: real loopback HTTP success, 503 failure,
  malformed/empty/oversized output and eight-second timeout; one callback and
  authored fallback on failure. No real model, remote request or paid service.
- `RATW.Narrative.SensoryPrivacy`: mixed speech/action parsing, missing-action
  marker, wholly hidden event omission and deterministic masks.
- `RATW.Network.BoundedSnapshotCodec`: large Unicode payload roundtrip and
  invalid/oversized length rejection before decompression allocation.
- `RATW.NPC.InactivityDeadline`: 3,599/3,600-second boundary, activity resets,
  replay safety, subject isolation and no permanent-summary decay.
- `RATW.Persistence.SQLiteRestart`: exact Unicode roundtrip, atomic replacement,
  store reopen and invalid/unsupported-record detection.
- `RATW.UI.AdjustablePaneBalance`: four bounded presets, draft retained, no server
  command caused by presentation changes.
- `RATW.UI.InputAndDraftRecovery`: navigation/write modes, actual multiline
  editor, newline, preserved draft, send behavior and rejection recovery.
- `RATW.UI.SequentialRoleplayReveal`: ordered non-interleaving posts and composed
  speech/action presentation.
- `RATW.World.EngineIntegration`: core world inside Unreal, invalid actor
  rejection, dimensions, facing and state reload.
- `RATW.UI.StationaryFacingPreview`: Alt preview/commit, priority over entity
  clicks, scaled coordinates, angle interpolation, authoritative self precedence,
  movement/rising/chat/world/modal/focus suppression and Ctrl compatibility.
- `RATW.Movement.PostureSkillsPersistence`: native JSON save/restore of posture
  transitions and skills, safe legacy defaults, malformed-skill rejection, and
  omission of private skill values from public entity rows.
- `RATW.Perception.ScentWindWirePrivacy`: hidden-source detection without source
  identity/coordinates/counts, bounded scent JSON, smell-stat/wind roundtrips and
  malformed wind rejection.
- `RATW.UI.ScentAwareness`: validated sector aggregation, anonymous local cues,
  airflow labels, indoor/calm handling, no scent hit targets, and development-only
  wind commands.
- `RATW.Movement.PacePersistenceAndWire`: strict pace commands, private owner
  statistics, dexterity/stamina/exhaustion save roundtrips, legacy defaults and
  malformed-save rejection, without public stat or remote-terrain leakage.
- `RATW.UI.PaceAndKnownTravel`: wheel/Page controls and chat/modal guards,
  lag-tolerant pace selection, gait labels, visited destinations, cancellation,
  neutral-input preservation, and overlapping remembered map origins.
- `RATW.Environment.ClockWeatherAndWire`: strict clock/weather commands and
  serialization, development-only authority, shared environment factors, shelter,
  malformed-save rejection, legacy defaults and weather-aware NPC fallback.
- `RATW.UI.EnvironmentalPresentation`: distinct weather geometry, clock/phase
  sanitization, gradient conventions, reduced-motion invariance, shelter and
  world-map suppression, development guards and hidden-actor privacy.
- `RATW.Environment.IndoorLightingWire`: strict development-only lighting
  commands, unlit-room sight, environment metadata and dialogue context, JSON
  roundtrip, malformed-profile rejection and day/night glow.
- `RATW.UI.CellAtmosphere`: actual small/large/panned cell bounds, warm/cool
  atmosphere, daylight suppression, unlit interior darkness, weather edges,
  numeric sanitization, reduced motion, privacy and development controls.

## Interior lighting and cell-atmosphere follow-up

Seven portable suites pass normally (1,067 checks, 50.45 seconds) and under Clang
ASan/UBSan (193.14 seconds). The 99 new lighting checks cover lit/windowed/sealed
profiles, perception and memory privacy, NPC parity, authored loading, atomic
persistence and invalid values. The editor passes 37 JS model tests and 23
Python export/HTTP tests; all 35 offline NPC bridge tests still pass.

The isolated browser check applies zero and fractional lighting levels, changes
tone, switches cells, requests a validated export, and recovers the fractional
profile after reload. No existing user draft or running world is changed. Native
visual review corrected a halo container assertion, a test-only movement command,
and outline seams/banding before the final captures. A concurrent engine run also
hit an asset-registry cache-file race; final automation is run in isolation.
Current end-to-end outcomes and lighting screenshots are recorded in
[lighting verification](LIGHTING_TEST_REPORT.md). The older sections below retain
their historical test counts.

Final lighting automation passed 20/20 with zero test warnings and exit 0. Native
lighting and weather scenarios plus their separate-process restarts passed after
visual polish. The final Linux package built/cooked/staged/archived in 99 seconds,
then passed graphical lighting/restart and headless weather/restart and travel.
Test processes are stopped; the pre-existing Atlas host was left intact and
requires a restart to load the new exporter (save JSON before reloading).

## Weather and daylight follow-up

All six portable suites pass normally (968 checks, 50.36 seconds) and under Clang
ASan/UBSan (179.22 seconds). The added 127 weather checks exercise actual
perception and movement, smooth daylight boundaries, stacked weather/darkness,
independent senses, NPC parity, shelter, and atomic clock persistence. All
eighteen native automation tests pass with zero test warnings.

The native graphical weather run cycles one authored glade through clear day,
rain, snow, fog and clear night, then explicitly opens a door into a separate
shelter. Six screenshots are captured with freshness checks. A second real game
process confirms that shared nighttime and weather persist while indoor factors
remain neutral. Normal player saves are untouched. Visual QA prompted a fog
gradient-axis correction and an indoor-header correction before final captures.

Existing native multi-cell travel, two-client scent/privacy, two-client IC/OOC
networking, gradual facing/posture movement, and three-stage NPC-memory
persistence regressions pass. The 35 offline NPC-bridge tests also pass; no paid
provider calls were made. Evidence: `artifacts/logs/weather-*-regression.log` and
the native weather scenario logs. See [weather verification](WEATHER_TEST_REPORT.md)
for detailed coverage and limits.

After the final visual fixes, all eighteen native tests passed again with process
exit 0, and the graphical weather/restart scenario was rerun successfully. All
six refreshed screenshots were visually inspected. The final module rebuild
took 18.17 seconds. The Linux package built/cooked/staged/archived successfully in
101 seconds and passed the same weather plus process-restart scenarios
headlessly. Packaged multi-cell travel also passes afterward. Evidence:
`artifacts/logs/package-weather.log`, `weather-packaged-result.log`, and
`weather-packaged-travel-regression.log`. All test game processes were stopped;
the separate Atlas Workshop service was left untouched.

## Pace and world-travel follow-up

The September 21 extension adds 151 portable pace assertions and 77 travel checks.
All five final portable suites pass (841 assertions/checks total, 63.12 seconds).
All five Clang ASan/UBSan suites passed in 252.76 seconds; that full sanitizer run
contained 74 travel checks. After the final early-Open correction, the complete
77-check travel suite was rebuilt and rerun under ASan/UBSan successfully. All
sixteen final native automation tests pass with zero test warnings.

The final native graphical and editor-free packaged headless travel scenarios
both pass. They use a real Atlas export with isolated synthetic prior-visit
memory, then exercise sprinting, stamina recovery, travel through three local
cells, typing and actual speech during the journey, manual cancellation, and
waiting for an explicit Open before entering a separate shelter. Two fresh
viewport screenshots were captured and visually inspected. The native module
build succeeded in 31.34 seconds; the final Linux package completed in 91 seconds.

Existing native two-client networking, posture/facing movement, two-client scent,
three-stage persistence, and imported-atlas traversal regressions all pass on
this final build. Their logs and preserved per-process outputs are under
`artifacts/logs/pace-travel-regression-*`. No fatal/assertion/sanitizer signatures
were found in those runtime logs. Test processes were stopped, ordinary player
saves were untouched, and no additional paid provider calls were made.

See [the detailed travel report](TRAVEL_TEST_REPORT.md) for commands, test scope,
review fixes and limitations. This does not establish human balance, party pace
coordination, production-scale pathfinding performance or internet-load safety.

## Atlas Workshop follow-up

The separate map editor passes 34 JavaScript model tests, 21 Python exporter/HTTP
tests, and 188 new C++ authoring checks. The actual browser was exercised for
cross-boundary painting, split/merge, undo, detached interiors, reciprocal stairs,
export and draft recovery. The real JS model → Python exporter → C++ loader →
Unreal playtest passes in both the editor runtime and the rebuilt Linux package.
Existing native movement, two-client IC/OOC/long-post networking, two-client scent,
and three-stage persistence regressions all pass afterward. The fourteen native
automation tests remain green. See [the detailed editor report](EDITOR_TEST_REPORT.md)
for commands, evidence, fixes, and limitations.

## Scent and wind follow-up

The September 21 scent extension adds 102 portable assertions, retaining all 281
prior world assertions and 42 runtime checks. Normal and Clang ASan/UBSan suites
pass with no sanitizer findings. Coverage includes independent senses, reversed/
cross/calm wind, weather, injured noses and scent skill, blocked/open air paths,
corner routing without diagonal leaks, no visual-memory changes, aggregation,
source departure, saved gust continuity and atomic invalid-record rejection.

All fourteen native automation tests pass. The editor-runtime two-client scent
scenario also passes: a crouching player twelve tiles west is omitted from the
observer's visible entities but contributes a west scent cue. Actual **Smell**
narration agrees. Inspection of both that hidden player and a nonexistent ID
returns identical denials. Reversing airflow removes the west cue; restoring
airflow restores it. The graphical run captured a fresh screenshot, which was
visually inspected. Evidence: `artifacts/screenshots/scent-ash.json`,
`12-upwind-scent.png`, and `artifacts/logs/scent-smoke.log`.

Review and visual QA prompted fixes to a hidden-player identity-probing response,
misleading unseen-only sense labels, an unsupported wind-arrow glyph, overly
faint scent arcs, a hardcoded tavern reconnect greeting, and rain-specific yard
prose that contradicted clear weather. Smoke evidence now checks timestamps so
old screenshots/JSON cannot satisfy a new scent run. The offline NPC bridge's
35 tests still pass; no additional paid provider requests were made.

The Linux development package rebuilt successfully in 79 seconds and passed the
same two-client scent scenario headlessly. Packaged movement and all three
persistence stages (write, restart, aged-memory restart) also pass. A fresh
editor-hosted two-client IC/OOC/long-post network regression passes. Evidence:
`artifacts/packaged-evidence/scent-ash.json`, `movement-ash.json`, `persist-*.json`,
and `artifacts/logs/scent-*-regression.log`. All test processes were stopped; the
regular player save was untouched. This is not a population-scale scent
performance benchmark or a human stealth playtest.

## Movement follow-up

The September 21 movement update adds 105 portable assertions. Normal and
Clang ASan/UBSan suites pass. The actual editor-runtime graphical smoke passes
gradual replicated turning, sitting-rise under refreshed movement input,
lying-to-crouch speed and slash-command return to standing. Screenshots were
captured and visually inspected. See [movement details](MOVEMENT_UPDATE.md).

The first UI build exposed a changed Unreal focus-event constructor signature;
the tests now supply the required user index. A north-angle test then exposed
the engine's float `PI` constant versus double `atan2` precision; the assertion
uses a documented 1e-6 tolerance, without altering the production angle math.
All twelve native tests pass after those corrections. The Linux package rebuild
succeeded; no paid model calls were needed for movement verification.

The updated packaged Game passed the same movement scenario headlessly and all
three persistence stages (write, restart, aged-memory restart). The two-client
authoritative networking regression also passed. Evidence: movement scenario
JSON in both screenshot/packaged-evidence directories, packaged `persist-*.json`,
and `artifacts/logs/automation-movement.log` / `package-movement.log`.

The live restart test ages only a disposable test conversation's last-activity
timestamp. It does not wait an hour or modify the ordinary player save. Exact
timing is independently checked using explicit timestamps in the core tests.

## Revisions driven by testing

1. Fixed runtime initialization ordering so the first client is not discarded
   when BeginPlay follows PostLogin.
2. Closed SQLite explicitly before destruction, resolving an engine shutdown
   assertion.
3. Enabled the required network-driver plugins/configuration and compressed
   outgoing UTF-8 snapshots/events. An initial 105-KB snapshot exceeded Unreal's
   64-KB RPC bunch limit; the bounded envelope fixes this without raising limits.
4. Preserved original social-event audiences and corrected reciprocal hearing;
   later listeners cannot retroactively qualify disconnected conversations.
5. Prevented NPC greetings/companion activity from naming hidden players; NPC
   response triggers now use perceived text and respect the actual voice volume.
6. Preserved early conversation source IDs/commitments, made consolidation
   replay-safe, and retained ordinary character/map state across restart.
7. Validated saved geometry, door pairs, senses, knowledge tiers and observed
   glyph masks atomically; rejected saves cannot silently become blank new worlds.
8. Restored rejected composer drafts, corrected newline/focus handling, refreshed
   typing presence and fixed entity action menus/Ctrl-click priority.
9. Corrected washed-out colors with explicit sRGB conversion, improved the
   prototype profile drawing, and added true reflowing pane proportions.
10. Verified full-resolution captures, removed ordinary prose logging, and
    capped long-post contribution metadata instead of discarding its eligibility.
11. Added independent ephemeral test ports and readiness checks, preventing
    parallel networking tests or buffered packaged logs from confusing startup.

## Limits of this evidence

No human two-player 30-minute roleplay session, public-network latency/loss test,
hostile-load benchmark, complete keyboard/screen-reader audit, fifty-NPC population
test, or production migration/recovery drill has been performed. Linux is the only
tested platform. The portrait/inventory panels do not prove full customization or
equipment gameplay. The follow-up live provider checks are a small synthetic
sample, not comprehensive generated-quality or prompt-injection validation;
output has no gameplay mutation authority, but unsupported prose claims still
require broader evaluation. The configured local HTTP
provider must be trusted because this Unreal transport automatically follows
redirects.

Packaging emitted engine-content/plugin warnings; build and runtime scenarios
passed, but the package is a development playtest artifact, not a clean public
release candidate. A dedicated Server-target build and a smaller distribution
remain separate gates.

## Repeat

Run the commands under README's Verification section. Add `--packaged` to gallery,
walkthrough, persistence, movement, scent or network to exercise the archived Linux Game binary.
Every scenario uses an isolated `Saved/Tests/` database. Logs, raw scenario JSON
and build products are local reproducible outputs ignored by Git; the compact
automation report and screenshots are retained as evidence.
