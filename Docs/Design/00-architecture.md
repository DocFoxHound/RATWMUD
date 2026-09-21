# Architecture and delivery baseline

Status: accepted for the first development slice, 2026-09-20.

## Product boundary

Build the three-cell, two-client RATW slice described in PLAN.md. The narrative pane carries performance and imagination. The local glyph map conveys physical position, orientation and interaction. Persistent world rules run on the server. The client submits intentions and displays only its permitted perception.

The first content is the Bent Bough, a tavern designed for quadrupedal wolves: low work surfaces, floor cushions, a mouth-pull door latch, a raised hearth, an adjoining yard and a loft. These are provisional local place names, not additions to the supplied setting's global canon.

## Components and ownership

| Component | Source | Responsibility | Design |
| --- | --- | --- | --- |
| World simulation | Source/RATWMUD/Core | Cells, continuous movement, paths, doors, weather, heights, schedules | 01-world-simulation.md |
| Perception and memory | Source/RATWMUD/Core | Sight, hearing, per-observer map state and permanent exploration | 02-perception-maps.md |
| Slate client | Source/RATWMUD/UI | Layout, glyph rendering, facing marker, input, menus | 03-client-ui-input.md |
| Narrative | UI + Runtime | Composer, ordered posts, sensory filtering, IC/OOC and indicators | 04-narrative-composer.md |
| Character and items | UI + Runtime | Portrait presentation, profile, icons, appearance and item state | 05-character-inventory.md |
| Runtime authority | Source/RATWMUD/Runtime | RPC validation, identity, snapshots, restart and storage | 06-network-persistence.md |
| NPC memory and life | Core + Runtime | Schedules, party participation, active context and durable summaries | 07-npc-memory.md |
| Social progression | Runtime | Validated metadata, capped ledger, separate NPC relationships | 08-social-progression.md |
| Dialogue provider | Runtime | Bounded NPC prose with a working offline fallback | 09-dialogue-provider.md |
| Interaction and environment | Core + UI + Runtime | Verb semantics and sensory effects | 10-interactions-environment.md |
| Verification | Tests + tools | Core behavior, engine integration, two clients, screenshots | 11-verification.md |

## Authority boundary

The engine-independent C++ core has no font, camera, Slate or animation dependency. It can be tested with CMake/CTest without an editor process. Unreal owns network transport, game lifecycle, native desktop presentation and storage integration. The runtime converts core snapshots into an explicit JSON presentation contract sent only to the owning player controller.

Each actor has server-owned identity. A client command names an intention; it cannot submit a new location, XP balance, memory, quest result or NPC relationship. World state is not replicated wholesale. Client UI selection, reveal timing and draft contents are local. Typing communicates only bounded presence.

## ADR-001: engine and toolchain

Use installed Unreal Engine 5.8.2, Linux x64, CL 56702186, with bundled clang 20.1.8. Start with a custom Slate renderer and individually drawn glyphs. UMG/Paper2D are not necessary for the first renderer. Terrain and actors remain separate layers.

The observed installed platform configuration lists Linux Editor and Game targets, not Server. Keep a Server.Target.cs for a compatible source engine but use `UnrealEditor -server -nullrhi` for the development server. This is a separate authoritative server process with real Unreal networking. It does not constitute a packaged dedicated-server release. [Epic's dedicated-server guide](https://dev.epicgames.com/documentation/unreal-engine/setting-up-dedicated-servers-in-unreal-engine) documents the source-build prerequisite.

A Linux Game package is built and tested. It can alternatively run a separate
headless listen host using `?listen -RatwHeadlessHost -nullrhi`; that flag suppresses
the otherwise local host's character. Two packaged clients were verified against
this host. This is a practical editor-free playtest path, not a Server-target
binary or a public deployment recommendation.

## ADR-002: scope and delivery honesty

Implementation status must distinguish implemented behavior, automated evidence, visually inspected behavior, scaffolding and deferred work. Do not mark a milestone complete merely because a screen exists. Source-engine packaging, configured model credentials, public authentication and real human roleplay playtests can remain explicit limitations of this local slice.

No paid model provider is assumed. The offline NPC response path must work and identify itself in the development UI. A later provider uses the same restricted context and cannot author world consequences.

## ADR-003: permanent memory

Exploration memory is character-scoped: direct sight earns a coarse outline, entry permits detail, and earned memory persists permanently. It is shown only when that cell falls within the current adjacent neighborhood. Maps and hearsay do not unlock it. No live state is refreshed through a remembered outline.

NPC memory uses detailed active context followed by a permanent compact summary after exactly 3,600 seconds of inactivity. New relevant interaction resets the deadline. Restart preserves pending context and the deadline. Summary creation uses stable IDs and must be replay-safe. Relevant summaries are retrieved selectively, not all inserted into every prompt.

## Deferred components

Chapter building, full combat, player magic unlocks, large-world authoring, economy, public accounts and uploaded portrait moderation remain outside this MVP. The code must not pretend a menu label supplies these systems. See PLAN.md for the complete boundary.
