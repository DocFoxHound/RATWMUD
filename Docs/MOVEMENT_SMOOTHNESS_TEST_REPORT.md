# Movement smoothness correction

September 21, 2026 · Linux x64 · Unreal Engine 5.8.2.

## Cause and correction

The simulation ran at 20 Hz, but the client received positions in full snapshots
at 5 Hz. Exponential target chasing (`Vector2DInterpTo`, speed 16) accelerated
after each snapshot and decelerated before the next. Pixel-snapped moving glyphs
added small visible steps.

Observer-filtered pose frames now run at 20 Hz independently of the 5 Hz full
snapshots. Positions and shortest-arc facing interpolate on a 100 ms buffered
timeline every rendered frame. Moving glyphs and their attached markers use
subpixel rendering; static ASCII terrain stays sharp. Movement speed, collision,
pathfinding, stamina, gradual facing and posture timing are unchanged.

Session tokens, revisions and cell generations reject stale packets. Room entry
resets pose history; newly hidden actors disappear immediately. No hidden actor
positions or future paths are included in the light stream. Lost updates can be
bridged by available samples; a longer drought holds the last authoritative pose
instead of extrapolating through walls. Long stalls rebase the presentation clock.
The intentional buffer costs approximately 100 ms plus transport latency; this is
not client prediction and cannot eliminate freezes caused by prolonged packet loss
or a stalled renderer.

## Executed verification

- Editor build succeeded.
- Native Unreal Automation: **36/36 passed**, including three new tests.
- Portable regression suites: **12/12 passed**.
- Deterministic interpolation at 30, 60 and 144 FPS retained constant speed with
  alternating 20 ms arrival jitter and a lost packet. Also tested shortest-arc
  turning, duplicate/reordered samples, stops, no extrapolation, bounded history,
  teleports, stale metadata, same-named room generations and timing-stall recovery.
- Actual Slate tick test verifies steady movement, not just the helper's math.
- Real native two-client networking passed movement, IC/OOC and long-post checks;
  clients received 301/302 motion frames versus 76 full snapshots each.
- Native rendered movement scenario passed gradual facing, sitting/rising,
  lying-to-sneak and slash-command standing.
- Native rendered walkthrough passed click paths, explicit same-cell doors,
  vertical-map visibility, loft return and the outdoor transition.
- Linux package rebuilt successfully. Packaged two-client checks passed with
  322 motion frames per client versus 81/82 full snapshots.
- Packaged rendered movement checks passed, including posture delays and gradual
  facing. Packaged account checks passed registration/creation, owned-character
  entry/leave, private checkpoints, authority restart, persisted appearance,
  other-player inspection and duplicate-active-character rejection.

All smoke tests use disposable `Saved/Tests` saves and terminate their own
processes. The normal player save is not used for verification.

## Evidence and reproduction

`artifacts/logs/movement-build.log`, `movement-automation.log`,
`movement-core-build.log`, and `movement-package.log`; the Automation report is
`artifacts/automation/index.json`. Native scenario evidence is in
`artifacts/screenshots/{network-ash,network-bracken,movement-ash,walkthrough-ash}.json`;
packaged evidence uses `artifacts/packaged-evidence`. Rendered movement captures
10/11 and walkthrough captures 06/07/08 were refreshed by the native runs.

Run `bash tools/build.sh`, then `bash tools/test-engine.sh` with no other Unreal
process running. Run `python3 tools/smoke.py network --headless`,
`python3 tools/smoke.py movement`, and `python3 tools/smoke.py walkthrough`.
Build the distributable with `bash tools/package.sh`; `--packaged` targets it.
Account regression command: `python3 tools/character_smoke.py --packaged --headless`.
