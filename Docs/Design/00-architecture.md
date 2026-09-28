# Architecture and delivery baseline

Status: accepted for the first development slice, 2026-09-20; extended through 2026-09-21 with Atlas Workshop, travel, calendar/economy, political metadata and the local Storykeeper first slice. On 2026-09-28 the game left Unreal Engine for its own server and a browser client (ADR-007). This is a current development baseline, not production certification.

## Product boundary

Build the three-cell, two-client RATW slice described in PLAN.md. The narrative pane carries performance and imagination. The local glyph map conveys physical position, orientation and interaction. Persistent world rules run on the server. The client submits intentions and displays only its permitted perception.

The first content is the Bent Bough, a tavern designed for quadrupedal wolves: low work surfaces, floor cushions, a mouth-pull door latch, a raised hearth, an adjoining yard and a loft. These are provisional local place names, not additions to the supplied setting's global canon.

Atlas Workshop is a separate authoring surface. Authors paint continuous geography,
partition it into cells, and refine each cell or detached room. Exported cells
still enter the same observer-filtered simulation; the editor's omniscient canvas
is never a player view or a live-world administration interface.

Storykeeper is that separate operator surface: a local browser app and Python
service using an explicitly enabled private authority bridge. It observes
omniscient world snapshots and stores its own plans/political records, without
granting ordinary players administrative commands or editing the game database.

## Components and ownership

| Component | Source | Responsibility | Design |
| --- | --- | --- | --- |
| World simulation | Core | Cells, continuous movement, paths, doors, weather, heights, schedules | 01-world-simulation.md |
| Perception and memory | Core | Sight, hearing, smell, per-observer map state and permanent exploration | 02-perception-maps.md |
| Browser client | Client | Layout, glyph rendering, facing marker, input, menus, the front door | 03-client-ui-input.md, 27-browser-client.md |
| Narrative | Client + Core/RatwGame.cpp | Composer, ordered posts, sensory filtering, IC/OOC and indicators | 04-narrative-composer.md |
| Character and items | Client + Core | Portrait presentation, profile, icons, appearance and item state | 05-character-inventory.md |
| Game server | Server + Core/RatwGame.cpp + Core/RatwWeb.cpp | Command validation, identity, snapshots, WebSockets, the client's files, restart and storage | 06-network-persistence.md |
| NPC memory and life | Core | Schedules, party participation, active context and durable summaries | 07-npc-memory.md |
| Social progression | Core | Validated metadata, capped ledger, separate NPC relationships | 08-social-progression.md |
| Dialogue provider | Core/RatwMind.cpp + tools/npc_mind.py | Bounded NPC prose with a working offline fallback | 09-dialogue-provider.md |
| Interaction and environment | Core + Client | Verb semantics and sensory effects | 10-interactions-environment.md |
| Verification | Tests + Client tests + tools | Core behavior, the server end to end, the client, scripted players, screenshots | 11-verification.md |
| Atlas Workshop | Editor + tools/map_editor.py + Core/RatwAuthoring.cpp | Continuous authoring, cell partitioning/detail, reciprocal links, safe exports and atomic import | 12-map-editor.md |
| Pace and world travel | Core/RatwWorld.cpp + Core/RatwTravel.cpp + Client | Dexterity-scaled pace, stamina, private remembered routes, local-leg execution and cancellation | 13-pace-and-world-travel.md |
| Calendar and aging | Core/RatwCalendar.cpp + Core/RatwAging.cpp + Client | Four-hour days, 365-day years, seasonal forecasts, lunar lighting, birthdays and age modifiers | 14-calendar-aging.md |
| Resident life and economy | Core/RatwSociety.cpp + Core/RatwWorld.cpp + Client | Deterministic needs/jobs/navigation, finite cash and stock, recipes, wages, trade and bounded external orders | 15-npc-society-economy.md |
| Territory and migration | Editor + Core/RatwAuthoring.cpp + tools/map_editor.py + tools/dm_service.py + the server | Region/claim/site metadata; declared Chapter capacity, finite migration previews, verified arrival and political records | 16-territory-chapters-migration.md |
| Storykeeper | DM + tools/dm_service.py + Core/RatwDirector.cpp | Trusted-local map/roster, campaigns, event approval/scheduling, observed activity, audit and allowlisted effects | 17-storykeeper-dm.md |
| Combat presentation (planned) | Future Core resolver + event projection + Client encounter cards/effects | Observer-filtered battle cues and one expandable log per encounter, independent of roleplay reveal | 18-combat-presentation.md |

## Authority boundary

The C++ core (`Core/`) holds every rule and is tested with CMake/CTest. The server (`Server/ratw_server.cpp`) owns the
transport, the game's lifecycle and storage: it serves the browser client's files and speaks WebSockets on one port.
The game converts core state into an explicit JSON presentation contract sent only to the owning connection; the
browser client draws it and sends back intentions.

Each actor has server-owned identity. A client command names an intention; it cannot submit a new location, XP balance, memory, quest result or NPC relationship. World state is not replicated wholesale. Client UI selection, reveal timing and draft contents are local. Typing communicates only bounded presence.

The administrative exception is explicit and out-of-band: a trusted OS owner
starts the server with `--dm-directory /absolute/private/path`. Only that
private exchange carries omniscient data and typed operator requests. It is not
a player RPC or an authentication upgrade for a development character. The
service never opens the game's save; the server still validates
effects and checkpoints results. See ADR-006 below.

## ADR-001: engine and toolchain (replaced by ADR-007)

Superseded 2026-09-28; kept for the record. Use installed Unreal Engine 5.8.2, Linux x64, CL 56702186, with bundled clang 20.1.8. Start with a custom Slate renderer and individually drawn glyphs. UMG/Paper2D are not necessary for the first renderer. Terrain and actors remain separate layers.

The observed installed platform configuration lists Linux Editor and Game targets, not Server. Keep a Server.Target.cs for a compatible source engine but use `UnrealEditor -server -nullrhi` for the development server. This is a separate authoritative server process with real Unreal networking. It does not constitute a packaged dedicated-server release. [Epic's dedicated-server guide](https://dev.epicgames.com/documentation/unreal-engine/setting-up-dedicated-servers-in-unreal-engine) documents the source-build prerequisite.

A Linux Game package is built and tested. It can alternatively run a separate
headless listen host using `?listen -RatwHeadlessHost -nullrhi`; that flag suppresses
the otherwise local host's character. Two packaged clients were verified against
this host. This is a practical editor-free playtest path, not a Server-target
binary or a public deployment recommendation.

## ADR-002: scope and delivery honesty

Implementation status must distinguish implemented behavior, automated evidence, visually inspected behavior, scaffolding and deferred work. Do not mark a milestone complete merely because a screen exists. Source-engine packaging, configured model credentials, public authentication and real human roleplay playtests can remain explicit limitations of this local slice.

No paid model provider is required by ordinary launches. The offline NPC response path must work and identify itself in the development UI. The opt-in tested OpenAI bridge uses the same restricted context and cannot author world consequences; see `../LIVE_NPC_TEST_REPORT.md` for credential isolation and bounded test evidence.

## ADR-003: permanent memory

Exploration memory is character-scoped: direct sight earns a coarse outline, entry permits detail, and earned memory persists permanently. The Nearby view shows only the current adjacent neighborhood. A separate Known Routes index may show cached names, dimensions, and placement for all visited cells, without remote glyphs or live state. Maps and hearsay do not unlock either view. No live state is refreshed through a remembered outline.

NPC memory uses detailed active context followed by a permanent compact summary after exactly 3,600 seconds of inactivity. New relevant interaction resets the deadline. Restart preserves pending context and the deadline. Summary creation uses stable IDs and must be replay-safe. Relevant summaries are retrieved selectively, not all inserted into every prompt.

## ADR-004: author continuously, play separate cells

Use a local-browser editor hosted by Python's standard library, launched with
`python3 tools/map_editor.py serve`. The shared atlas grid owns world terrain and
sparse heights. Rectangular cell bounds own partition metadata, while detached
interiors own separate grids. Cuts, splits, and merges remap links and spawn via
global coordinates without relocating terrain. A focused drill-down supports
fine editing and reciprocal door, passage, or stair connections.

The browser model validates edits transactionally. The exporter independently
validates the authoring data, generates compatible open seams, and creates a
snapshot containing the source atlas and separate runtime cell files. Runtime
`--world` import validates another candidate before replacing demo content.
No stage edits a running game or grants players additional map knowledge.

Optional faction/Chapter catalogs and cell territory metadata follow the same
validation path. Splits inherit region/claims/site; incompatible merges/recuts
reject atomically. Multiple faction claims are provisional contested assertions,
not inferred control or construction permission. Chapter associations do not
erase claims. See `../TERRITORY_AUTHORING_CONTRACT.md`.

The first editor is bounded: world and individual cell/room dimensions are at
most 256×256, with at most 256 total cells/rooms. NPC/item authoring and legacy
`.cell` import are not supplied. This is a development content tool, not evidence
of production-scale authoring or simulation performance.

The custom-save default uses a manifest-path hash. It is not a content hash or a
topology migration scheme. Geometry revisions need a fresh export directory or
fresh explicit save path, even if some stable cell IDs survive the edit.

## ADR-005: deliberate travel, not fast travel

Dexterity sets the top movement speed; the server accepts a bounded pace notch
and computes stamina with continuous recovery. The client displays the requested
and limited gait, sprint emphasis, and actual energy trend, never an accepted
position or a client-supplied statistic.

World journeys use a visited-cell graph with both portal endpoints observed,
then execute ordinary local navigation. Every crossing retains the normal anchor
and stops; only an explicit world journey issues another leg after 0.25 seconds.
Closed doors require Open. Writing and panels do not cancel that journey, while
manual navigation and explicit cancellation do. Routes never resume from a save.
See [the component contract](13-pace-and-world-travel.md) for exact boundaries.

## ADR-006: local operator authority, separate from player transport

The native bridge writes versioned snapshots every two seconds to an absolute,
owner-private directory. Storykeeper reads them, keeps a last-good cache and
becomes read-only when freshness exceeds ten seconds or validation fails. The
snapshot includes world identity, cells, political catalogs, characters and
finite accounts—not chat, drafts, NPC memory text or model credentials.

The service binds loopback port 8780 and owns a separate SQLite planning store.
A random bearer session is saved in a private `session.json` URL fragment;
all APIs require it, with strict Host/Origin and bounded schema validation.
This protects the trusted-local workflow, not a public or multi-admin service.

Approved events commit a world-bound expiring outbox request before atomic
publication. Native authority accepts only notice, weather, resident relocation
and finite asset transfers, checkpoints the effect and a bounded receipt, then
publishes a result. Requests cannot mint money or invent NPCs. Retry uses the
same identity; a timeout can mean an unconfirmed outcome, never proof of rollback.
Queued operations cannot be safely cancelled as though they had not dispatched.

Migration approvals reserve declared capacity, select finite eligible residents
within an explicit region, and wait for a fresh snapshot proving home/arrival
before recording source-claim resentment. Only this workflow adds political
consequences; direct administrative relocation does not. Chapter sites may be
audited operator overlays on otherwise unassigned native cells. Neither overlays
nor housing/job numbers modify Atlas or construct a building. Political opinions
remain service records and do not yet drive native NPC behavior.

Observed Chapter presence and cell transitions are aggregated from actual fresh
samples, not invented history or transcript analysis. Suggested play windows
require DM confirmation; approved UTC times do not automatically shift. Armies,
brigands, assassination and faction collapse are non-executable story plans.
Full schemas/lifecycle limits live in `../DM_BRIDGE_CONTRACT.md` and
`../DM_SERVICE_CONTRACT.md`; component design is `17-storykeeper-dm.md`.

## ADR-007: our own server, a browser client, no engine

Accepted 2026-09-28. Unreal did three jobs here and none needed an engine: it drew a 2D screen (one hand-drawn
1600×1000 canvas, a few forms, a recoloured portrait, procedural weather sheets), carried the network, and hosted the
server. The portable core already held every rule, and its own server (`Server/ratw_server.cpp`) already passed every
smoke. So the client became a web page (`Client/`, TypeScript on a canvas, ported call for call from the Slate
client) served by that server, and the Unreal project was removed. There is no 3D in the game's plans; were there, a
3D client (Unreal or another engine) could speak the same WebSocket protocol.

- **One port.** HTTP for the client's files, a WebSocket at `/ws` for the game (`Core/RatwWeb.h`). Each message is a
  kind byte and the payload the game has always sent: command JSON, snapshot acknowledgements, and zlib-compressed
  events, delta snapshots and binary motion frames.
- **Keys by position.** WASD are read by `KeyboardEvent.code`, released on blur, and resent while held.
- **Credentials** only over loopback until the server speaks TLS; the WebSocket upgrade checks `Origin`.
- **Tests.** The Slate UI's engine tests became client tests (Node's test runner, and headless Chromium for the front
  door); the scripted Unreal players became `tools/client/scenario.ts`, which plays in Node or in the real page.

See [the browser client](27-browser-client.md).

## Deferred components

Chapter building, autonomous population migration, full combat, player magic unlocks, production-scale world authoring, NPC/item/job placement tooling, broader economy/crafting, public accounts/staff roles and uploaded portrait moderation remain outside this MVP. Storykeeper's planned encounter/army/faction executors and native consequences for political opinions also remain unbuilt. The code must not pretend a menu label supplies these systems. See PLAN.md for the complete boundary.
