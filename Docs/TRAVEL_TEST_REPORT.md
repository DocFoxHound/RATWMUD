# Pace and world-travel verification

September 21, 2026 · Linux x64 · Unreal Engine 5.8.2.
Design and controls: [pace and remembered travel](Design/13-pace-and-world-travel.md).

## Executed results

| Check | Result |
| --- | --- |
| Final portable CTest | 5/5 executables, 841 assertions/checks, 63.12 seconds |
| Clang ASan/UBSan | All five suites pass; final 77-check travel revision also independently rebuilt and passed |
| Final Unreal build / automation | Build succeeds; 16/16 tests pass, zero test warnings |
| Native graphical travel | Pass; fresh screenshots captured and visually inspected |
| Final Linux package / packaged headless travel | Build, cook, stage, archive and travel scenario pass |
| Existing native regressions | Network, movement, scent, persistence and imported-atlas traversal all pass |

The full sanitizer run contained 74 travel checks; the final 77-check result is
in `artifacts/logs/travel-final-asan.log`. Final normal CTest contains all 77.
Build evidence: `artifacts/logs/pace-travel-build.log` and
`artifacts/logs/package-pace-travel.log`. Existing regression outputs and their
per-process logs are preserved under `artifacts/logs/pace-travel-regression-*`.

## Coverage

The portable pace suite adds 151 assertions covering every pace notch, real
dexterity-dependent displacement, baseline walking, weather/terrain, sustainable
middle pace, constant regeneration, rapid sprint drain, exhaustion/recovery,
crouch caps, posture preparation, wall/slide collision, short-waypoint accounting,
portal-distance exclusion, partial-step arrival, passive bumps, strict restore,
and no offline refill.

The travel suite adds 77 checks: own visited memory, generic unknown/glimpsed
rejection, observed reciprocal endpoints, distant-state privacy, weighted local
navigation, alternate seams/corridor detours, three-cell movement, closed-door
approach and explicit Open, early Open and ordinary internal barriers, exact
arrival stop, cancellations, neutral-input preservation, bounded blocked-route
behavior, rejected-save atomicity and removal of transient journeys on reload.

The two new Unreal automation tests are:

- `RATW.Movement.PacePersistenceAndWire`: strict request validation (including
  booleans, strings, fractions and huge numbers), private owner stats, native
  serialization, exhaustion persistence, safe legacy defaults, malformed-save
  rejection, and absence of private fields/remote terrain in public views.
- `RATW.UI.PaceAndKnownTravel`: wheel/Page controls, map versus prose scrolling,
  multiline/modal guards, pace bounds and lag-tolerant requests, correct gait
  labels, visited-only destination acceptance, route cancellation, neutral input
  while traveling, and retention of overlapping-origin destinations.

All sixteen native automation tests passed after the final UI changes, including
the fourteen existing movement, senses, narrative, networking and persistence tests.
Evidence: `artifacts/automation/index.json` and
`artifacts/logs/automation-pace-travel.log`.

## Real-client scenario

```bash
python3 tools/travel_smoke.py
python3 tools/travel_smoke.py --packaged --headless
```

The harness creates a new Atlas export and SQLite save under `Saved/Tests/`.
Its three outdoor cells and detached shelter have synthetic prior-visit records;
the test does not grant knowledge to an ordinary character or alter a normal save.
This keeps the integration focused on the actual travel execution and UI. The
portable tests independently check discovery eligibility and memory isolation.

The native graphical and packaged headless runs both passed this sequence
(screenshots are captured only in graphical mode):

1. Select full sprint and request the visited eastern destination.
2. Confirm authoritative displacement, higher speed and stamina drain; capture
   the live pace display.
3. Send typing presence and ordinary speech during the journey; still traverse
   the intermediate cell and arrive at the destination.
4. Verify the wolf stops there and recovers stamina; capture Known Routes.
5. Begin another journey, issue Stop, and verify it does not resume.
6. Select a sustainable trot and the visited shelter. Approach its closed door,
   remain outside until explicit Open, then enter the separate interior.

Fresh JSON and screenshot timestamps are required; old evidence cannot satisfy a
new run. Evidence: `artifacts/logs/travel-native-result.log`,
`artifacts/logs/travel-native-smoke.log`, `artifacts/screenshots/travel-ash.json`,
and the two [screenshots](SCREENSHOTS.md). Both captures were visually inspected.
Packaged evidence: `artifacts/logs/travel-packaged-result.log`,
`artifacts/logs/travel-packaged-smoke.log`, and
`artifacts/packaged-evidence/travel-ash.json`.

## Fixes prompted by review and testing

- Replaced recursive path advancement with single-step energy accounting, avoiding
  repeated regeneration on short waypoints and charging only actual movement.
- Kept explicit-door approaches from accidentally requesting a crossing on a
  nearby automatic seam. Early Open on a same-cell blocker now replans onward.
- Aligned UI gait labels with server bands, including notches 1 and 2.
- Grouped overlapping remembered map labels while keeping separate destinations
  in the list.
- Preserved deliberate world journeys during both typing and actual speech;
  ordinary local movement still stops on entering chat. Added live coverage.
- A concurrent packaging attempt hit UnrealBuildTool's single-instance mutex
  (misreported by its wrapper as SDKNotFound). Packaging was rerun separately;
  no SDK installation or engine configuration change was needed.

## Limits

These are bounded developer fixtures, not human balance approval, hostile-load
or internet-latency testing. Only Linux is verified. Dexterity training, combat
fatigue, companion/party pace coordination and offline travel are not implemented.
World travel targets a visited cell's entry point, not a remote individual tile.
Remembered hierarchical routes are not guaranteed globally shortest under changing
terrain. No additional model-provider requests, deployment, commit or push were
needed. Final build/regression outcomes are recorded in the main test report.
