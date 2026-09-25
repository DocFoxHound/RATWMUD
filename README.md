# Runs Against the World

A native Unreal prototype of a persistent, text-first roleplaying world inhabited by quadrupedal wolves. The terrain is a logical glyph tilemap; wolves move freely over it as an upright `W` with an orbiting `>` facing marker. Roleplay prose stays in the narrative pane.

## Requirements

- Unreal Engine **5.8.2** (Linux x64 is the verified development environment).
- CMake 3.16+ and a C++17 compiler for portable simulation tests.
- Python 3 for the local Atlas/Storykeeper services and verification scripts.
- A modern browser for Atlas Workshop and Storykeeper; Node.js is needed only for their model tests.

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

The default launch now opens **login → character selection → character creator**.
Register a local test account with a unique test password (at least 12 bytes),
create up to six characters, then select one to enter. The creator has five wolf
species, four age stages, three statures, sex, natural coat gradients and markings.
The same portrait appears on the character sheet and visible-character inspection;
map actors stay `W>` glyphs. Appearance and ownership are saved on the authority.

These are **trusted-local test accounts**, not public authentication. Native
credential exchange is permitted only in standalone play or over loopback on this
computer; it is blocked for remote peers until encrypted transport is implemented.
There is no password recovery or character deletion yet.

For two players, start the headless development server in one terminal, then two clients:

```bash
bash tools/server.sh
bash tools/connect.sh 127.0.0.1:7787
bash tools/connect.sh 127.0.0.1:7787
```

Sign in separately and select different characters. Saves are local SQLite files
under `Saved/`; pass `-RatwSave=/absolute/path.sqlite` to the server or standalone
game to isolate a session. Automated smoke tests use disposable saves. Legacy
development identities are available only with explicit `-RatwDevIdentity` on
both authority and clients; this bypass is for controlled tests, not real accounts.

The installed engine can host a headless authoritative server through its Editor
executable. The package also supports a separate headless **listen host** with no
local player character:

```bash
bash tools/run-packaged.sh server
bash tools/run-packaged.sh connect 127.0.0.1:7787
bash tools/run-packaged.sh connect 127.0.0.1:7787
```

Launch each command in its own terminal. The server scripts bind only to
127.0.0.1 by default. `RATW_BIND` can select another interface for controlled
development-identity tests, but remote account login remains deliberately blocked.
Do not publicly expose this development server.

`Source/RATWMUDServer.Target.cs` defines a stripped dedicated-server target, but
this installed engine explicitly rejects Server targets. That distribution
requires a compatible source engine. The tested packaged listen host is a useful
local development option, not a production dedicated-server release.

## Atlas Workshop — separate map editor

```bash
python3 tools/map_editor.py serve
```

Open the printed local URL. Paint terrain across the continuous world canvas,
then cut it into standard **32×24** cells or a custom grid. Merge rectangular
groups, split individual cells, and drill down for detailed terrain/elevation
work. Detached rooms can be connected through reciprocal doors, passages, or
stairs. The authoring overview does not change the player's separate-cell map,
movement, door actions, or perception rules.

**Territory & Chapter sites** adds faction/Chapter catalogs, a region ID, any
number of faction claims, and an optional Chapter site per cell or detached
room. The overlay distinguishes contested claims from Chapter sites; neither
means legal ownership, building permission, or effective control. Overlapping
claims are a provisional representation, not a settled diplomacy system.
Splits inherit metadata; incompatible merges/recuts are rejected. See the
[territory contract](Docs/TERRITORY_AUTHORING_CONTRACT.md).

Save editable JSON for a portable backup. **Export world** downloads a ZIP with
the source atlas, separate `.cell` files, and `world.ratw`. Extract each version
into a new directory, then play it explicitly:

```bash
bash tools/play.sh -RatwWorld=/absolute/path/to/new-export/world.ratw
```

The default custom save is keyed by the manifest **path**, not its contents.
Changing geometry at the same path does not create a fresh save: use a new export
directory or an explicit fresh `-RatwSave` path. Editing/exporting never hot-reloads
the game or migrates an existing save. Custom worlds start without the demo NPCs.

This first editor uses rectangular canvases/cells, each at most **256×256** tiles,
with at most **256 total cells and detached rooms**. It does not yet author NPCs
or items, import legacy `.cell` files, or provide production-scale world tooling.
See [the editor design and workflow](Docs/Design/12-map-editor.md) and
[the format contract](Docs/EDITOR_CONTRACT.md).

## Storykeeper — separate local DM application

Storykeeper watches and directs a running authority; Atlas authors places.
Launch the native authority with an explicit **absolute private directory**,
then start the service in another terminal using that same directory. Replace
these example paths with private locations owned by your OS user:

```bash
bash tools/server.sh -RatwDMDirectory=/absolute/private/ratw-bridge -RatwSave=/absolute/private/storykeeper-playtest.sqlite
python3 tools/dm_service.py --exchange /absolute/private/ratw-bridge --state-dir /absolute/private/storykeeper --port 8780
```

The bridge/state folders must have mode `0700`; newly created folders receive
that mode, and overly permissive existing folders are rejected. Open the `url`
stored in the private `/absolute/private/storykeeper/session.json` file in a
local browser. It contains a temporary bearer token in the URL fragment, not a
query parameter. Do not share or commit that file/URL. No player login is used;
restarting the service creates a new operator session.

The first slice includes an omniscient map/roster, campaigns and beats, Chapter
profiles/members, faction opinions, observed activity, fixed-UTC event scheduling,
and audited migration previews. Real effects are limited to announcements,
weather, physical relocation of eligible existing residents, and transfers of
existing money/herbs/meals. Armies, brigands, assassinations and faction collapse
are story plans with unavailable executors, not completed game mechanics.

Chapter sites and housing/jobs can be explicitly declared as operator overlays
where Atlas has no conflicting Chapter. These are not constructed buildings or
measured vacancies. Migration requires approval, uses a finite named population,
and applies source-claim resentment only after a fresh snapshot confirms arrival.
Opinions are operator records, not yet native NPC behavior. Automatic migration,
new jobs and self-sufficient player towns remain future work. The demo is grouped
as `demo_reach`; custom Atlas exports still contain no authored NPC population.

All edits become read-only when the native snapshot is stale. This is a
trusted-local, single-operator development tool: loopback HTTP, private files,
no public player RPC, no direct game-database writes, and no public/multi-admin
authentication. Use an isolated save for experiments; dispatched effects cannot
be cancelled as though they were never applied. See the
[Storykeeper design](Docs/Design/17-storykeeper-dm.md),
[service contract](Docs/DM_SERVICE_CONTRACT.md), and
[native bridge contract](Docs/DM_BRIDGE_CONTRACT.md).

The current Linux package includes the private bridge. Add the same
`-RatwDMDirectory` and isolated `-RatwSave` flags to
`bash tools/run-packaged.sh server` to use it without the editor. Run
`python3 tools/dm_smoke.py` or `python3 tools/dm_smoke.py --packaged` for an
isolated end-to-end check. Both paths passed the current 23-check suite;
[verification and limitations](Docs/DM_TEST_REPORT.md) and
[actual screenshots](Docs/SCREENSHOTS.md) are recorded separately.

## Calendar and settlement life

The demonstration now runs four-hour days, 365-day years, seasons and a natural
lunar sequence on accelerated **game days**, now confirmed. Night visibility
combines moon phase and weather. Characters age with the shared running calendar
even while logged out; birthdays notify the player and update the character
sheet on return. Server shutdown time does not currently advance the calendar.

Confirmed next requirements are player-chosen natural death with a persisted
mandatory deadline between ages 100 and 120, and sparse map combat effects plus
one collapsed/latest-action or expanded/full-perceived-log entry per encounter.
**Natural death and combat presentation are not implemented yet.** See
[lifespan design](Docs/Design/14-calendar-aging.md) and
[combat presentation](Docs/Design/18-combat-presentation.md).

Six residents follow deterministic needs and jobs independently of NPC chat.
Approach Rowan at the tavern counter and choose **Trade** to buy or sell herbs
and meals. Purse, stock and demand are finite. **Inventory** shows quantities
and lets you eat a meal; a visible herb patch in Juniper Yard supports **Gather**
from within reach. Resources feed the same gathering/cooking/trading chain used
by NPCs. Custom Atlas worlds do not yet author these demo NPCs or resource jobs.

Development settings (`-RatwDevTools`) expose next-day/next-year jumps and
seasonal weather for testing. These mutate the selected development save; use
a fresh `-RatwSave` for experiments. Normal clients cannot change the calendar.

```bash
python3 tools/society_smoke.py
python3 tools/society_smoke.py --packaged
python3 tools/aging_smoke.py
python3 tools/aging_smoke.py --packaged
```

The society check captures the graphical client and restarts its isolated save.
The aging check keeps a separate authority running across three client
connections to exercise logged-out aging and command-retry behavior.

See [calendar design](Docs/Design/14-calendar-aging.md),
[NPC/economy design](Docs/Design/15-npc-society-economy.md), and
[verification](Docs/SOCIETY_TEST_REPORT.md) for formulas, sources and limits.

## Controls

| Input | Action |
| --- | --- |
| WASD | Continuous movement |
| Click terrain | Intelligent path to position |
| Wheel over local map / Page Up / Page Down | Increase or decrease walking-to-sprinting pace |
| Shift-wheel / Ctrl-wheel over local map | Pan vertically / horizontally without changing pace |
| Hold Alt + move mouse | Preview a faded facing `>` while stationary |
| Alt-click (or Ctrl-click) | Turn gradually toward the point without moving |
| Click entity or door | Contextual action menu |
| Enter | Enter chat; while composing, send and return to movement |
| Shift+Enter | New line in a roleplay post |
| Escape | Leave chat while retaining draft; close a panel; otherwise cancel world travel |
| M | Local/world map |
| C / I | Character / inventory |
| L | Listen |

Use quoted speech mixed with `/sigh`, `/action`, `/pose`, `/sit`, `/lay`, `/stand` or `/me`. Unquoted ordinary text is spoken. `/me` declares a current state visible on inspection. Use `//` to write a literal slash word. Local OOC has its own channel.

During ordinary local navigation, opening a same-cell door leaves movement stopped. Opening a transition door crosses into the connected cell and stops at its arrival anchor. Crossing an unobstructed boundary exit also transitions and stops.

For longer journeys, open **World → Known Routes** and select a previously
visited cell. The wolf walks through remembered connections, pauses briefly at
each arrival anchor, and continues at the selected pace. Closed doors still
require **Open**. This explicit journey continues while writing or viewing
panels; ordinary local click movement still stops when entering chat. WASD, a
local destination, Stop/Wait, or navigation-mode Escape cancels the journey.

Dexterity determines top speed. The pace/stamina strip highlights sprinting and
shows recovery or drain: recovery continues during movement, a middle trot is
sustainable, and a full sprint spends stamina quickly. Crouching and exhaustion
limit speed without changing the selected notch. See [pace and world travel](Docs/Design/13-pace-and-world-travel.md)
for controls, tuning, privacy, and the cell-level destination boundary.

Moving from `/sit` first spends 0.65 seconds standing. Moving from `/lay` spends
0.45 seconds rising into a slow sneak (30% walking speed); `/stand` leaves the
crouch. Sneak skill limits how far away others can see you or hear your pawsteps;
their hearing skill and ear health affect detection. Speech still uses the chosen
whisper/speak/yell volume. See [movement and stealth notes](Docs/MOVEMENT_UPDATE.md).

Sight, hearing and smell are independent. Unseen wolves can leave a broad lavender
`~~` scent arc around your token without revealing their name or map position.
The wind label shows airflow (W→E carries western scents eastward); weather, nose
health and scent skill affect detection. **Smell** in the general-action row
describes what currently reaches you. This first pass covers live body scent in
the current cell, not lingering tracks. See [scent and wind](Docs/SCENT_AND_WIND.md).

## Weather and daylight

Weather is visible on the local map and affects the simulation: rain/snow mask
sound and scent and slow travel, fog conceals the distance, and night reduces
server-visible terrain and wolves. The shared clock runs a four-hour day with
gradual dawn/dusk. Indoors stays sheltered, but light is independent:
warm taverns can glow at night while staying clear, windowed unlit rooms darken
after sunset, and sealed unlit rooms remain dark even at noon. Soft glow and dark
edge fades follow the actual cell boundary without tinting the story pane. Atlas
Workshop's cell details expose artificial light, daylight access, and light tone.
These are whole-cell settings, not individual lamp/shadow simulation. Reduced
motion retains static weather cues without changing perception. For development
playtests, launch with `-RatwDevTools` and use Settings' weather/time/lighting presets;
ordinary players cannot change them. See [weather and daylight](Docs/Design/10-interactions-environment.md).

If Atlas Workshop was already running before this update, save its draft as JSON,
restart `python3 tools/map_editor.py serve`, and reload the editor. The Python
exporter does not hot-reload; a browser refresh alone cannot update an old host.

## Verification

```bash
cmake -S . -B build-core -DCMAKE_BUILD_TYPE=Debug
cmake --build build-core -j 6
ctest --test-dir build-core --output-on-failure
bash tools/test-engine.sh
python3 tools/smoke.py network --headless
python3 tools/character_smoke.py
python3 tools/character_smoke.py --packaged
python3 tools/smoke.py gallery
python3 tools/smoke.py walkthrough
python3 tools/smoke.py persistence
python3 tools/smoke.py movement
python3 tools/smoke.py scent
python3 tools/smoke.py scent --packaged --headless
python3 tools/smoke.py network --packaged
python3 tools/travel_smoke.py
python3 tools/travel_smoke.py --packaged --headless
python3 tools/weather_smoke.py
python3 tools/weather_smoke.py --packaged --headless
python3 tools/lighting_smoke.py
python3 tools/lighting_smoke.py --packaged --headless
python3 tools/test_npc_bridge.py
node --test Editor/model.test.mjs
python3 tools/test_map_editor.py
node --test DM/model.test.mjs
python3 tools/test_dm_service.py
```

Use `-DRATW_SANITIZERS=ON` in a separate CMake build directory for AddressSanitizer/UndefinedBehaviorSanitizer checks. Actual executed results and limitations are in [the test report](Docs/TEST_REPORT.md), and captures in `artifacts/screenshots/`.

## Design and status

- [Vision](VISION.md) and [finalized MVP plan](PLAN.md)
- [Component designs and architecture](Docs/Design/00-architecture.md)
- [Atlas Workshop map editor](Docs/Design/12-map-editor.md)
- [Territory, Chapters and migration](Docs/Design/16-territory-chapters-migration.md)
- [Storykeeper DM application](Docs/Design/17-storykeeper-dm.md)
- [Pace, stamina, and remembered world travel](Docs/Design/13-pace-and-world-travel.md)
- [Implementation status](Docs/IMPLEMENTATION_STATUS.md)
- [Questions for the next session](Docs/MORNING_QUESTIONS.md)
- [Actual game screenshots](Docs/SCREENSHOTS.md)
- Original supplied sources preserved in `Docs/References/`

NPCs use authored offline dialogue by default. An opt-in server-side bridge to
RATW Game's existing OpenAI provider has now passed live dialogue and persistent
recall tests; see [setup and results](Docs/LIVE_NPC_TEST_REPORT.md). Credentials
stay outside this repository and game clients. Conversation memory becomes
eligible for permanent consolidation one hour after the last interaction; it is
processed on startup or the next five-second checkpoint. Production model quality, public
authentication, Chapter construction, full combat, and Gifted/Quickened unlocks
have separate gates; see the status document for what is actually implemented
and tested.
