# Verification report

Run: September 20–21, 2026. Host: Linux x64, Unreal Engine 5.8.2,
bundled clang 20.1.8; GCC and Clang used for independent simulation tests.

## Executed results

| Check | Result | Evidence |
| --- | --- | --- |
| Portable CMake/CTest | Pass: 176 world assertions and 42 runtime checks, 2/2 executables | `build-core/Testing/Temporary/LastTest.log` |
| AddressSanitizer + UndefinedBehaviorSanitizer | Pass: both portable suites, no reported violations | `build-core-clang-sanitize/Testing/Temporary/LastTest.log` |
| Unreal module | Builds successfully with installed toolchain | `artifacts/logs/build-editor.log` |
| Unreal Automation | 10/10 tests pass, process exit 0 | `artifacts/automation/index.json` |
| Separate editor-hosted server + two clients | Both moved and received the other character's IC, local OOC and 16,000-character post | `artifacts/screenshots/network-ash.json`, `network-bracken.json` |
| Packaged Linux Game | Build, cook, stage and archive succeed | `artifacts/logs/package-client.log`, `artifacts/package/Linux/` |
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
equipment gameplay. Generated NPC quality and prompt-injection resistance have
not been evaluated with a real model; output has no gameplay mutation authority,
but unsupported prose claims still require evaluation. The configured local HTTP
provider must be trusted because this Unreal transport automatically follows
redirects.

Packaging emitted engine-content/plugin warnings; build and runtime scenarios
passed, but the package is a development playtest artifact, not a clean public
release candidate. A dedicated Server-target build and a smaller distribution
remain separate gates.

## Repeat

Run the commands under README's Verification section. Add `--packaged` to gallery,
walkthrough, persistence or network to exercise the archived Linux Game binary.
Every scenario uses an isolated `Saved/Tests/` database. Logs, raw scenario JSON
and build products are local reproducible outputs ignored by Git; the compact
automation report and screenshots are retained as evidence.
