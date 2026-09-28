# Calendar, aging and settlement verification

September 21, 2026. Linux x64, Unreal Engine 5.8.2; native C++/Slate client,
independent C++17 simulation, no paid language-model calls for this work.

## Implemented and exercised

Four-hour days, 365-day years, temperate seasons, deterministic forecasts,
Earth-like lunar phases on accelerated game days, moon/weather outdoor light,
annual character/NPC growth and age-65+ effective sense/dexterity penalties.
The old two-hour clock migrates legacy saves without retroactive rewards.

Six NPCs choose needs and work deterministically. They physically move, open
permitted doors, sleep, eat, gather, cook, transport supplies, earn limited wages,
and transact using finite inventories/purses. Player trade, gathering and meal
consumption affect the same authoritative supplies. Dialogue has no economic
write authority. Public snapshots expose only the owner's purse and a nearby,
visible, awake keeper's offers—not every NPC's accounts or hidden locations.

## Executed evidence

| Check | Result | Evidence |
| --- | --- | --- |
| Portable GCC suite | Final frozen logistics policy: 10/10 executables; 1,400 checks | `artifacts/logs/society-recovery-core-tests.log` |
| ASAN + UBSAN | Final frozen core: all 10 suites pass, 1,400 checks | `artifacts/logs/society-recovery-core-sanitize.log` |
| Unreal native automation | Final 22/22 cases pass, including strict text/null JSON and in-reach gathering | `artifacts/logs/calendar-society-native-tests.log`, `artifacts/automation/index.json` |
| Native and packaged graphical client + restart | Buy/sell, finite quantities, junk/range refusal, consumption, annual reward/notice, lunar progress and persisted purse/age pass; four fresh captures in each build | `society-ash.json`, `society-restore-ash.json` in both screenshot/evidence directories; `artifacts/logs/society-*-result.log` |
| Linux Game package | Final build/cook/stage/archive successful, 91.77 seconds | `artifacts/logs/package-calendar-society.log`, `artifacts/package/Linux/` |
| Packaged multiplayer | Two independent clients moved and received IC/OOC posts through a separate headless host | `artifacts/logs/network-calendar-packaged-result.log` |
| Native and packaged remembered travel | Pace/stamina, three-cell travel, writing during travel, recovery, cancellation and explicit closed-door opening pass | `artifacts/logs/travel-calendar-*-result.log` |
| Existing weather regression | Day/rain/snow/fog/night, sheltered interior, saved conditions pass | `artifacts/logs/weather-native-smoke.log`, `weather-restore-native-smoke.log` |
| Existing interior-lighting regression | Warm/daylit/unlit/cool light, outdoor transition and saved dark room pass | `artifacts/logs/lighting-native-smoke.log`, `lighting-restore-native-smoke.log` |
| Packaged weather and lighting | Both complete environment scenarios and separate-process restores pass | `artifacts/logs/weather-calendar-packaged-result.log`, `lighting-calendar-packaged-result.log` |
| Atlas authoring model/exporter | 37 JavaScript and 23 Python tests pass | `artifacts/logs/calendar-society-editor-model.log`; Python console output |
| NPC bridge offline regression | 35 tests pass; zero external provider requests | `python3 tools/test_npc_bridge.py` |
| Live-server offline aging, native and packaged clients | Three sequential client connections pass in each build: Ash absent during Birch's year advance, one reward/notice on return, no second starting grant; explicit command retry preserves its original response | `artifacts/logs/offline-aging-native-result.log`, `offline-aging-packaged-result.log` |

Portable counts: world 383, authoring 188, pace 151, travel 77, weather 130,
lighting 99, calendar 157, society 136, aging 37, runtime 42.

The final logistics/command-receipt revision was rechecked with the entire
portable/sanitizer/native suites, a rebuilt package, both graphical society
restarts, both offline-aging harnesses and packaged multiplayer. The separate
travel/weather/lighting/editor/bridge runs above preceded that final internal
revision and are retained as integration regression evidence.

The calendar suite samples 40,000 forecasts and tests every seasonal boundary,
four-hour recurrence, lunar continuity and invalid inputs. Aging/world tests
cover replay-safe rewards, legitimate within-day developer time changes,
corrupt future birthdays, atomic combined restore, nearby/remote trading,
blocked physical production, update-rate consistency and maximum clock bounds.
Native tests additionally exercise strict JSON types, saved accounts, frontend
quote validation and privacy, visible-only resource targets, and developer-only
calendar controls.

## Physical three-day NPC soak

`Tests/society_soak.cpp` is an optional reproducible probe, not a slow default
CTest test. It runs real World navigation without teleporting bodies, jumping
the clock, changing supplies, or invoking chat. The captured 43,200-second run
observed 53 cooked meals, 80 gathered herbs, 21 herb-delivery transactions,
16 meal deliveries, 4 resident food purchases, 27 paid service contracts,
32 capped outside exports and 6 imports. All six followed work/rest routines.

The run starts at noon and spans parts of four calendar budget days, explaining
32 orders under the eight-per-day cap. Money conservation held throughout:
final circulation **1,680 pennies = 1,704 minted − 24 sunk**. See
`artifacts/logs/society-physical-world-probe.log`. This proves physical integration
for a small settlement, not winter equilibrium or production scalability.

An initial 15-day extension exposed a genuine planning failure despite perfect
money conservation: at day 15, five residents were maximally hungry at an empty
counter while the cook still had meals, the porter carried herbs, and the patch
had 60 bundles. Supplier handoff and food-acquisition priorities formed a loop.
`artifacts/logs/society-15-day-soak.log` preserves that failed-health state; its
exit status checks accounting/bounds, **not settlement health**. Subsequent
logistics and recovery verification must be read separately below. No hidden
cash or food refill is accepted as a fix.

## Revised logistics and recovery probes

The revised priority selector, supplier rendezvous, partial affordable batches,
stable eating anchor and explicit consignment policy were then exercised using
real physical navigation. `--require-fed` now also fails a probe if a resident
remains at hunger 90 or greater for two game days; it no longer mistakes bounded
starvation for health. Both runs below passed, with much shorter or no critical
hunger at all:

| Frozen-policy probe | Food-access evidence | Final accounting |
| --- | --- | --- |
| Fresh world, 30 game days | No resident reached hunger 90; all six ended at hunger 47 and fatigue 9 | 2,720 = 3,216 minted − 496 sunk |
| Representative old day-15 collapse, 15 more game days | Longest critical hunger was 180 simulation seconds; all six resumed obtaining food | 2,383 = 2,747 minted − 364 sunk |

Evidence: `artifacts/logs/society-30-day-recovery-policy-soak.log` and
`artifacts/logs/society-collapsed-recovery-soak.log`. The recovery fixture restores
the failed state's existing money, goods and crowd positions once; nothing is
injected after simulation begins. The fresh run cooked 397 meals, gathered 640
herbs, completed 166 meal deliveries, and made 100 ordinary resident purchases.
Its 232 ordinary exports plus 16 consigned exports use exactly 248 orders across
31 touched budget days, within the unchanged eight-order daily cap.

These are Spring logistics/food-access tests, **not long-run equilibrium**. The
fresh final cook/forager balances were 1,016/580 pennies; the keeper had zero,
and the service residents had 50/48/26. Capital distribution and wage solvency
still need balance work. Winter, larger populations and heavy player extraction
remain unproven; the prototype deliberately does not conceal insolvency.

To reproduce the longer probes with an optimized build:

```bash
cmake -S . -B build-soak -DCMAKE_BUILD_TYPE=Release
cmake --build build-soak -j6
c++ -O2 -std=c++17 -ICore Tests/society_soak.cpp build-soak/libratw_core.a -o build-soak/society_soak
build-soak/society_soak 30 --require-fed
build-soak/society_soak 15 --collapsed --require-fed
```

## Issues found and corrected

- The old half-second schedule loop discarded fractional remainder; needs/work
  now retain cadence across different frame rates.
- Developer calendar changes could save stale seasonal weather; forecasts now
  update synchronously before saving.
- Restoring a missing player account could reissue starting goods; new-format
  checkpoints now require the existing account and reject the entire save.
- Unreal JSON can coerce numbers/booleans into text. Accounting, needs and text
  shape are now explicitly validated; invalid in-memory JSON nodes fail closed.
- Stationary service counters could be reserved forever by a broke NPC. Only
  genuinely exclusive beds/workstations reserve space, allowing other customers.
- Nonfinite body coordinates, unsafe inventory capacity, role substitution,
  future budgets and mismatched money totals now reject or skip unsafe state.
- Eating now clears exhaustion when stamina crosses its recovery threshold.
- New-format NPC life now requires its physical NPC checkpoint record; a missing
  body cannot silently reset a resident's position or age.
- A graphical smoke assertion initially read the previous periodic snapshot
  immediately after an accepted trade while packaging saturated the machine.
  The scenario now waits for the committed inventory update (with a bounded
  timeout), rather than assuming one wall-clock second guarantees a new frame.
  The failed evidence showed an accepted purchase event alongside the old
  snapshot; the production transaction itself had succeeded.
- Hungry NPCs could queue at an empty shop, miss their ingredient supplier, or
  require an unaffordable full batch. Availability-aware work and physical
  supplier meetings, partial batches, and explicitly recorded consignment now
  restore productive circulation without new grants or inventory.
- Tiny crowd bumps could change an eating target every tick and reset its
  timer. The timed meal now retains a stable target until genuinely interrupted.
- Active command context was not cleared after returning, allowing a later
  unsolicited birthday notice to overwrite that command's retry receipt. A
  scope guard now clears it on every exit; the real-client aging scenario
  retries the calendar command and checks one reward/notice plus two correct
  command responses.

## Screenshots and interpretation

The gallery captures the actual rendered client, not a generated mockup.
`29-finite-merchant.png` shows real stock, buy/sell prices and both purses;
`30-birthday-character.png` shows the annual stats;
`31-nightly-rest.png` shows the night calendar and warm tavern as NPCs return to
rest; `32-birthday-notice.png` shows the private birthday message.
All four final native captures were regenerated after visual review. The age
sheet's unsupported arrow was replaced with readable plain-text parentheses;
the corrected native and packaged character-sheet captures were inspected.
The merchant and warm-night views were also visually reviewed. These checks
are not a substitute for a two-person roleplay usability session.

The final logistics/receipt build regenerated all four captures in both native
and packaged runs again. Its packaged merchant and birthday sheet were visually
inspected; screenshots are from the same final binary as the gameplay checks.

## Deliberate limits

- Moon phases provisionally follow accelerated game days, not today's actual
  astronomical date. Shared-calendar offline aging is also awaiting confirmation.
- No automatic death, starvation damage, retirement, time during server downtime,
  regional weather fronts, or astronomical altitude/cloud simulation.
- Two goods and six authored demo residents; no general Atlas NPC/job/resource
  authoring, player crafting, player-to-player trade, debt, taxes or banking.
- Starter allocation is a finite treasury transfer. Export orders are an
  explicit capped source; imports remove currency. No hidden purse refill.
- Companion work pauses while recruited; party food/rest provisioning is not
  implemented. “Socialize” is an activity, not generated background conversation.
- Development identities remain unauthenticated. Bounded command receipts are
  not a production transaction/authentication service or infinite audit history.
- Birthday notices use reliable RPC delivery, not durable client acknowledgment.
  Graceful save/reconnect behavior is covered; crash/disconnect delivery windows
  can still lose or repeat the message, independently of replay-safe stat awards.
- No new paid-provider test was necessary: deterministic NPC decisions never
  call a language model. The earlier provider report remains separate evidence.
