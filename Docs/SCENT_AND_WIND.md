# See, hear and smell

Implemented September 21, 2026 as an extension of the native Unreal slice.

## Player experience

The three senses can disagree. A crouching wolf can be too far away to see, too
still to hear, yet detectable by scent carried toward the observer. Scent does not
make the hidden `W` appear or make that wolf inspectable. Existing speech/action
filtering remains independent: smell alone never reveals words or poses.

An unseen scent produces a lavender broad arc and `~~` around the observing wolf,
in one of eight directions. Its fixed screen distance is presentation, not source
distance. Multiple sources in a direction merge into one cue. Strength is a
coarse faint/distinct/strong category, not a count. The general **Smell** action
describes the current cue in the narrative pane. The SIGHT / HEARING / SCENT
status strip keeps the channels distinct; the hearing status tracks unseen
moving players, while speech continues through its existing prose channel.

The wind label describes **where air flows**, not meteorological wind-from
notation: W→E means a source west of you can be smelled farther away. The marker
therefore points generally upwind toward incoming scent. Nearby scent in still
air is also possible. Rain and snow drift with wind visually; reduced-motion
mode freezes that animation.

## Server rules and tunable assumptions

- Per-cell base wind is direction in radians (east zero, south positive) and
  normalized strength 0–1. Outdoors defaults to an eastward 0.5 breeze with smooth
  deterministic variation. Indoors is sheltered and calm. Base wind and world
  time persist, preserving the variation phase after restart.
- Smell sensitivity is `smell × noseHealth × (1 + 0.75 × scentSkill / 100)`.
  Nose health is 0–1 and skill is 0–100. These authoritative values persist; a
  complete injury/training system is not yet implemented.
- Downwind alignment increases the full-clarity and maximum detection radii;
  crosswind/upwind detection stays short. At healthy untrained baseline, clear
  weather and strength 0.5, directly downwind scent is full within 6 tiles and
  fades to zero at 18. Calm/crosswind baselines are 1 and 3 tiles. These are game
  tuning values, not biological claims or a final physical tile scale.
- Rain multiplies outdoor range by 0.65; snow by 0.8; fog has a provisional 1.05 bonus. Darkness alone does not reduce scent. Indoor shelter ignores
  outdoor precipitation. Direction/strength can vary without changing weather.
- Walls and closed doors block airflow. Low furniture does not. A bounded air
  path search can carry scent around an opening/corner, with no diagonal leaks
  through sealed corners. The coarse direction follows incoming air near the
  observer when a direct route is blocked. Search is capped at 64 tiles.
- Sneaking reduces visibility and movement noise but does not erase body odor.
  Scent detections are recalculated from current positions, not accumulated tracks.

## Privacy contract

Each observer receives at most eight anonymous scent entries:
`{sector: 0..7, strength: 1..3, windborne: boolean}`. No source IDs, names,
coordinates, colors, counts, equipment or NPC/player discriminator are included.
Scent never grants inspection, map knowledge, adjacent-cell discovery or dialogue
content. Visible sources use the ordinary visible map token; redundant scent
arcs are reserved for unseen wolves. The UI defensively validates and aggregates
the bounded cue records.

## Deliberate first-pass limits

This is live-body scent **within one cell**. It does not yet include deposited
trails, a delay while a scent plume travels, regional wind fronts, indoor drafts,
cross-cell ventilation, distinctive individual scent recognition or masking
items. Barriers are deliberately airtight; this is not a fluid simulation.
An empty cue means no distinguishable scent reaches you, not proof you are alone.

The open design choices are whether familiarity should reveal identity and
whether trails or cross-cell airflow should be the next milestone. Anonymous
awareness is the provisional default until those are decided.

## Reproduce

`python3 tools/smoke.py scent` launches a real server and two clients in a fresh
test database. Bracken crouches twelve tiles west of Ash in clear weather. Ash
detects scent without receiving Bracken's token, cannot inspect a forged hidden
target ID, loses that directional cue when the wind reverses, then detects it
again when airflow returns. The graphical run captures
`artifacts/screenshots/12-upwind-scent.png` from the actual viewport.

Both editor and packaged two-client runs passed. The complete portable suite
passes 383 world assertions plus 42 runtime checks, including Clang ASan/UBSan;
all 14 Unreal automation tests pass. See `TEST_REPORT.md` for evidence and limits.

Add `--packaged --headless` to check the archived Linux Game. Development-only
wind presets require `-RatwDevTools`; normal clients cannot change the weather
server through these commands. No model API calls are needed for these tests.
