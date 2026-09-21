# Runs Against the World

A native Unreal prototype of a persistent, text-first roleplaying world inhabited by quadrupedal wolves. The terrain is a logical glyph tilemap; wolves move freely over it as an upright `W` with an orbiting `>` facing marker. Roleplay prose stays in the narrative pane.

## Requirements

- Unreal Engine **5.8.2** (Linux x64 is the verified development environment).
- CMake 3.16+ and a C++17 compiler for portable simulation tests.
- Python 3 for the two-process/client verification scripts.

Set `RATW_UNREAL_ROOT` if your engine is installed elsewhere. The local default is `/home/martinb/Applications/UnrealEngine/5.8.2`.

## Build and play

An already-built Linux development package is available locally:

```bash
bash tools/run-packaged.sh play
```

It runs without opening the Unreal editor. To rebuild the package, use
`bash tools/package.sh`. The archive is under `artifacts/package/Linux/` and is
intentionally excluded from Git.

For editor-based development:

```bash
bash tools/build.sh
bash tools/play.sh
```

For two players, start the headless development server in one terminal, then two clients:

```bash
bash tools/server.sh
bash tools/connect.sh 127.0.0.1:7787 -RatwIdentity=ash -RatwName=Ash
bash tools/connect.sh 127.0.0.1:7787 -RatwIdentity=bracken -RatwName=Bracken
```

The development identity selects a persistent character; it is not a public account login. Each concurrent client needs a different identity. Saves are local SQLite files under `Saved/`; pass `-RatwSave=/absolute/path.sqlite` to the server or standalone game to isolate a session. The automated smoke tests do this automatically.

The installed engine can host a headless authoritative server through its Editor
executable. The package also supports a separate headless **listen host** with no
local player character:

```bash
bash tools/run-packaged.sh server
bash tools/run-packaged.sh connect 127.0.0.1:7787 -RatwIdentity=ash -RatwName=Ash
bash tools/run-packaged.sh connect 127.0.0.1:7787 -RatwIdentity=birch -RatwName=Birch
```

Launch each command in its own terminal. The server scripts bind only to
127.0.0.1 by default. `RATW_BIND` can select another interface for a trusted LAN,
but development identities are not safe for public internet hosting.

`Source/RATWMUDServer.Target.cs` defines a stripped dedicated-server target, but
this installed engine explicitly rejects Server targets. That distribution
requires a compatible source engine. The tested packaged listen host is a useful
local development option, not a production dedicated-server release.

## Controls

| Input | Action |
| --- | --- |
| WASD | Continuous movement |
| Click terrain | Intelligent path to position |
| Ctrl-click | Face a point while stationary |
| Click entity or door | Contextual action menu |
| Enter | Enter chat; while composing, send and return to movement |
| Shift+Enter | New line in a roleplay post |
| Escape | Leave chat while retaining draft; close an open panel |
| M | Local/world map |
| C / I | Character / inventory |
| L | Listen |

Use quoted speech mixed with `/sigh`, `/action`, `/pose`, `/sit`, `/lay`, `/stand` or `/me`. Unquoted ordinary text is spoken. `/me` declares a current state visible on inspection. Use `//` to write a literal slash word. Local OOC has its own channel.

Opening a same-cell door leaves movement stopped. Opening a transition door crosses into the connected cell and stops at its arrival anchor. Crossing an unobstructed boundary exit also transitions and stops.

## Verification

```bash
cmake -S . -B build-core -DCMAKE_BUILD_TYPE=Debug
cmake --build build-core -j 6
ctest --test-dir build-core --output-on-failure
bash tools/test-engine.sh
python3 tools/smoke.py network --headless
python3 tools/smoke.py gallery
python3 tools/smoke.py walkthrough
python3 tools/smoke.py persistence
python3 tools/smoke.py network --packaged
```

Use `-DRATW_SANITIZERS=ON` in a separate CMake build directory for AddressSanitizer/UndefinedBehaviorSanitizer checks. Actual executed results and limitations are in [the test report](Docs/TEST_REPORT.md), and captures in `artifacts/screenshots/`.

## Design and status

- [Vision](VISION.md) and [finalized MVP plan](PLAN.md)
- [Component designs and architecture](Docs/Design/00-architecture.md)
- [Implementation status](Docs/IMPLEMENTATION_STATUS.md)
- [Questions for the next session](Docs/MORNING_QUESTIONS.md)
- [Actual game screenshots](Docs/SCREENSHOTS.md)
- Original supplied sources preserved in `Docs/References/`

NPCs use authored offline dialogue by default. Conversation memory becomes
eligible for permanent consolidation one hour after the last interaction; it is
processed on startup or the next five-second checkpoint. Model generation, public
authentication, Chapter construction, full combat, and Gifted/Quickened unlocks
have separate gates; see the status document for what is actually implemented
and tested.
