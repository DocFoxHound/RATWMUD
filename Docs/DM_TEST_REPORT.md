# Territory and Storykeeper verification

September 21, 2026. Linux, Unreal Engine 5.8.2. All interventions used isolated
test saves and operator databases. No provider API calls or production game
state were involved. This is a first-slice verification, not a public-hosting,
security-audit or world-scale performance certification.

## Automated checks

| Layer | Result | Evidence |
| --- | --- | --- |
| Portable C++ simulation | 11/11 suites pass | `artifacts/logs/dm-core-tests.log` |
| Clang AddressSanitizer + UndefinedBehaviorSanitizer | 11/11 suites pass; leak detection enabled | `artifacts/logs/dm-sanitizer-tests.log` |
| Atlas editing model | 49 tests pass | `Editor/model.test.mjs` |
| Atlas Python validator/exporter | 30 tests pass | `artifacts/logs/dm-editor-tests.log` |
| Storykeeper frontend model | 12 tests pass; JavaScript syntax check passes | `artifacts/logs/dm-browser-model-tests.log` |
| Storykeeper Python service | 36 tests pass | `artifacts/logs/dm-service-tests.log` |
| Unreal Automation | 23/23 suites pass, including private operator contract | `artifacts/logs/dm-native-tests.log` |
| Native server + real player client + HTTP service | 23 end-to-end checks pass, including restart | `artifacts/logs/dm-native-smoke-result.json` |
| Updated Linux package | Build/cook/stage/archive succeeds in 69.95 seconds | `artifacts/logs/dm-package-build.log` |
| Packaged listen authority + real packaged client + HTTP service | 23 end-to-end checks pass in 74.453 seconds, including restart | `artifacts/logs/dm-packaged-smoke-result.json` |

The new portable director suite covers 158 checks: strict UTF-8/catalog imports,
atomic rejection, overlapping claims, finite transfers, role/recruitment guards,
physical relocation through doors, mid-journey persistence and new-home rest.
Unreal checks additionally cover receipt replay, malformed/cross-world/expired
requests, persistence corruption, connected-recipient notices, omniscient export
privacy, and full-state rollback if the authoritative checkpoint fails.

The integration harness launches a fresh game authority, a separate Python
service and a real Unreal player client. It verifies:

- Missing/bad bearer and cross-origin requests cannot operate the service.
- A player-command attempt cannot invoke the private economic executor or read
  the director snapshot.
- Campaign/beat/Chapter/faction records and both Chapter/player opinions work
  through HTTP, while an army remains an explicitly blocked narrative plan.
- Fog changes the actual game environment, not merely an event card.
- Three pennies leave the finite treasury and enter Ash's purse. Repeated HTTP
  and byte-identical native requests do not duplicate the ledger transfer.
- A scheduled Chapter notice does not fire early, reaches a real client, and
  observed activity produces a peak-window suggestion.
- A finite resident moves from a different cell and becomes settled only after
  the authoritative home record confirms physical arrival.
- Restart preserves world identity, offline-player cash, relocated home and
  weather. Replaying the original native request after restart still produces
  only the original transfer.

## Browser exercise

Atlas was served on an isolated local port, with a new browser-local test draft.
Created a test faction and Chapter, assigned both to a cell, split that cell and
merged its matching parts. The metadata survived and the UI showed the claim
overlay and Chapter site without redrawing terrain. The automated suites cover
conflicting merges and unsafe exports beyond this visual exercise.

Storykeeper used an independently running game server with a fresh SQLite save,
six real demo NPCs and three cells. The browser exercised session connection,
empty/stale state, Chapter capacity review, migration preview and explicit
approval, confirmed arrival, and weather draft/approval. Test names are fixture
data, not canonical factions or Chapters. Screenshots are actual application
captures in `artifacts/director/`.

Also created a private campaign and faction, recorded an opinion with an explicit
reason, verified it on the Chapter dashboard, and saved a brigand encounter as a
planning-only story beat. No encounter executor was invoked. Reload required
reconnecting the session, as intended; the capability is not browser-stored.
After the live-render polish, cell selection retained keyboard focus across
several polls, and the default browser viewport had no horizontal overflow.
Browser console inspection found no warning/error entries. After stopping the
test authority, the browser visibly changed to `Read-only · stale`, kept prior
results as history, and disabled `Create event draft`. The test service was then
stopped, invalidating its temporary operator capability.

## Findings and boundaries

- Fixed the observer test's malformed public `look` command; it now sends the
  normal `action/look` envelope, so activity is genuine rather than fabricated.
- Hardened corrupt receipt restoration and scope-specific notice validation.
  Checkpoint rollback now retains unrelated in-flight movement and typing.
- An oversized queue file is rejected rather than repeatedly consuming the
  queue budget. Queue I/O failure disables the channel instead of presenting
  stale information as healthy.
- Live UI lists now keep unchanged nodes and restore keyed focus, expanded
  details and scroll when content changes. Event feedback follows the associated
  queued/applied/failed state; political sites are labeled separately from claims.
- Chapter sites are separate from faction claims. Capacity remains declared,
  not measured construction or funded employment.
- Migration is operator-approved, not autonomous; existing jobs and meals remain
  demo commutes. Custom Atlas worlds still lack NPC/job authoring.
- Source-faction resentment is tested in isolated service tests with authored
  claims. The default live demo has no canonical faction claims, so its real
  migration creates no invented political penalty.
- Opinions are political service records; native hostility, trade restrictions,
  armies, assassinations, brigands and faction destruction do not consume them.
- The bridge is opt-in, same-OS-user and loopback only. There is no public staff
  authentication, multi-admin authorization, durable offline notice inbox,
  distributed exactly-once guarantee or reversible world transaction system.

## Reproduce

```sh
cmake -S . -B build-dm -DCMAKE_BUILD_TYPE=Release
cmake --build build-dm -j 6
ctest --test-dir build-dm --output-on-failure
node --test Editor/model.test.mjs DM/model.test.mjs
python3 tools/test_map_editor.py
python3 tools/test_dm_service.py
bash tools/build.sh
bash tools/test-engine.sh
python3 tools/dm_smoke.py
```

Run Unreal Automation without other Unreal instances, since engine asset caches
are shared. The smoke script isolates ports, saves, private capability files and
operator state; it never prints session capabilities. Pass `--packaged` after
rebuilding the Linux package to test the packaged listen authority/client path.
