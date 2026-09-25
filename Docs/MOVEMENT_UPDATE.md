# Movement: facing, posture, sneaking, pace and travel

Implemented direction, September 21, 2026 UTC. These are initial tuning values,
not final balance or a physical world scale.

## Smooth rendering follow-up

Movement now has its own observer-filtered 20 Hz stream and 100 ms buffered,
frame-by-frame position/facing interpolation. Full world snapshots remain 5 Hz.
This removes the old repeated catch-up/slowdown rhythm, without changing movement
rules or introducing client-predicted collision. Moving glyphs use subpixel
rendering. See [the correction and verification](MOVEMENT_SMOOTHNESS_TEST_REPORT.md).

## Pace and overland follow-up

Wheel over the local map or Page Up / Page Down selects one of eleven pace
notches. Shift-wheel pans vertically and Ctrl-wheel horizontally. Chat/modals
guard these controls. The pace strip highlights sprint, distinguishes requested
and posture/exhaustion-limited gait, and shows stamina's actual recovery/drain.

Walking is 2.6 tiles/s; dexterity 0–100 sets the sprint cap to 5.2–10.4. Constant
+5/s stamina recovery combines with `15*(pace/10)^2` movement drain. A middle
trot is sustainable; notch-10 sprint nets −10/s. Exhaustion forces walking until
20 stamina. Crouching remains 30% walking speed even with sprint selected.

World → Known Routes lets the player choose a previously visited destination
cell. The server follows observed connections using local paths, stops at each
arrival anchor for 0.25 seconds, then continues. Closed doors require Open;
ordinary local-click transitions still stop until new input. The journey can
continue through writing/panels, but manual WASD, a local destination, Stop/Wait,
navigation Escape, or posture/facing cancels it. Routes never resume after reload.

See [the pace and world-travel design](Design/13-pace-and-world-travel.md) for
formulas, cached-map privacy, partial-frame accounting, and the cell-level
destination boundary. The sections below retain the earlier facing/stealth
handoff; its dated test totals are historical. Current executed evidence is in
[the test report](TEST_REPORT.md).

## Player controls

While stationary in the local map, hold **Alt** and move the cursor to preview a
faded `>` around the `W`. Alt-click commits that direction. The actual marker
turns toward it over time without translating the wolf. Ctrl-click remains an
alternative. Previewing alone does not contact the server or change facing.

The server turns along the shortest arc at 180 degrees/second. A half-turn takes
one second. Movement takes precedence and facing follows actual travel. The
client interpolates only accepted server angles. Chat, modals, the world map,
movement, rising, focus loss and Alt release suppress the preview.

| Starting posture / command | Preparation | Result |
| --- | --- | --- |
| Sitting → WASD or click path | 0.65 seconds, stationary | Standing, ordinary walking |
| Lying → WASD or click path | 0.45 seconds, stationary | Crouching, 30% walking speed |
| Crouching → `/stand` | 0.5 seconds | Standing |
| Lying → `/stand` | 1 second | Standing |

Refreshing held movement does not restart the timer. Releasing movement prevents
later unwanted travel; a physical rise can finish while stationary. Crouching
persists when stopped until another posture is selected. Opening a cross-cell
door still initiates its crossing, but waits for posture preparation; stopping
cancels that pending crossing. Sitting/lying down remain immediate in this slice.

## Detection and skills

Sneak and hearing skills are server-owned values from 0–100, saved with the
character. Existing saves default both to 0; nonnumeric/out-of-range skill values
are rejected. The formulas work now, but earning skill increases through a full
training/practice system is still a later milestone.

- A crouching wolf's visual detection cutoff begins at 7 terrain units with no
  sneak skill, shrinking to 3 at skill 100, then scales with the observer's vision,
  eye health and weather. Line of sight still applies. Outside that cutoff the
  server omits the wolf entirely from entity snapshots and rejects inspect/action
  queries, even if the client guesses an ID.
- Ordinary walking has a six-unit clear footstep radius. Sneaking reduces it to
  2.5 units at skill 0 and 0.8 at skill 100. Clarity falls to zero at twice the
  resulting clear radius. Only actual movement produces pawsteps.
- Hearing sensitivity and ear health scale audible range. Hearing skill adds up
  to 75% range at skill 100. Walls, closed barriers, elevation and weather continue
  to affect the result. Footsteps remain within the current cell.
- A listener may receive an anonymous nearby-pawsteps cue for an unseen moving
  player, no more than once every three seconds. It reveals no name, speaking
  color, actor ID or exact position and does not reveal a map token.

**Provisional interpretation pending user clarification:** sneaking quiets
movement, not intentional speech. Whisper/speak/yell keep their selected volume;
a hidden wolf who speaks can be heard as an anonymous voice. This avoids making
an intentional yell silent simply because the speaker is crouching.

Posture and skill values persist. Transient turn, route, input and queued portal
intentions do not; restarting resolves an interrupted rise to its intended stable
posture without resuming travel. Skill values are sent only to the owning player,
not included in another observer's public entity row.

## Verification

Portable tests cover gradual/shortest-arc turning, canceled turns, exact posture
boundaries, held-input refresh/release, click paths, crouch speed, visibility and
inspection privacy, sound detection, hearing/ear health, speech independence,
delayed portal crossing and restart validation. The suite now has 281 world
assertions plus 42 runtime-core checks; both regular and Clang ASan/UBSan runs
passed without findings.

Native tests add `RATW.UI.StationaryFacingPreview` and
`RATW.Movement.PostureSkillsPersistence`. The `movement` smoke scenario drives
actual player commands, samples replicated intermediate turns, waits through a
sit-rise under refreshed movement, moves from `/lay` at crouch speed, and returns
to standing through `/stand`. No model-provider calls are involved.

```bash
ctest --test-dir build-core --output-on-failure
bash tools/test-engine.sh
python3 tools/smoke.py movement
python3 tools/smoke.py movement --packaged
```

The graphical scenario writes `artifacts/screenshots/10-crouch-movement.png`
(or the packaged evidence directory). Use `--headless` for command/replication
checks without screenshots. Check `Docs/TEST_REPORT.md` for executed native and
package results rather than treating these repeat commands as evidence by themselves.

Executed follow-up: 12/12 Unreal tests passed with zero test warnings; graphical
editor and headless packaged movement scenarios passed; two-client networking and
all three packaged persistence stages passed. Both new captures were visually
inspected, including the Alt preview without a committed turn.
