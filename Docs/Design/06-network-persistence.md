# Authoritative networking and persistence

Status: implemented vertical-slice design, 2026-09-20. Engine: Unreal 5.8.2.

## Ownership and runtime

`ARatwGameMode` exists on the server and owns one `FRatwRuntime`. That runtime
owns the engine-independent `ratw::World`, memory store, social ledger, dialogue
adapter and SQLite connection. Clients own a Slate view and send intentions through
their own `ARatwPlayerController`; there is no client-writable world actor.

The simulation advances at fixed 0.05-second steps. A client receives five
perception snapshots per second and renders between them. Narrative events are
reliable, ordered RPCs; state snapshots are unreliable and replace older state.
The installed binary engine is exercised using an editor executable in headless
server mode. A distributable dedicated-server target remains a separate build gate.

## Wire protocol

`ServerCommand(FString)` carries a JSON object with a `type` and generated
`commandId`. The controller is the actor authority; supplying someone else's ID
in command data never changes the controlled actor.

| Type | Additional fields | Server action |
| --- | --- | --- |
| hello | id, name | Bind an unused development identity once |
| move | x, y | Continuous input, normalized by the simulation |
| path | x, y | Plan and validate a route |
| face | x, y | Face a point while stationary |
| stop | — | Clear direct input and path |
| typing | active | Set ephemeral presence, with 3.5-second expiry |
| color | index | Select one of 32 server-bounded palette IDs |
| action | target, action | Validate contextual/general action |
| chat | text, channel, volume, requestId | Parse, commit and perceive one post |
| weather | value | Development-only weather testing |

Commands are bounded to 65,536 UTF-16 units before parsing. A post is at most
16,384 UTF-16 units and 32,768 UTF-8 bytes. Initial chat rate limiting is two posts
per second; social evidence has its own slower cadence. The most recent 256
command IDs per character are persisted to suppress duplicate delivery. This is
a bounded development receipt window, not an unlimited production command ledger.

`ClientEvent(FString)` encodes a reliable compressed RPC carrying `roleplay`, `ooc`, `system`, `inspect`,
`chatAccepted`, or `error`. Rejected chat includes `context: chat`, the original
`requestId`, and a reason so the composer can recover its draft. A roleplay event
contains one sequence, one speaker label, one color and ordered segments. It does
not expose an author entity ID. Hidden speakers use “A voice.” The server sends
only listener-specific masked text.

`ClientSnapshot(FString)` encodes an unreliable compressed RPC containing current-cell tiles, visible entities and
fixtures, first-degree visible/remembered world-map neighbors, own character
state, starter inventory presentation, memory counts and server-confirmed social
totals. Unknown tiles are omitted; hidden entities never appear. Remembered cells
carry stale glyph topology and no live inhabitants or fixture state.

Both outgoing message kinds use a bounded zlib UTF-8 envelope: at most 1 MiB
uncompressed and 48 KiB compressed. Decoding checks lengths before allocating.
The first real two-client test found an uncompressed snapshot exceeding Unreal's
64 KiB bunch limit; compression fixes the transport without increasing engine
limits. The same envelope protects long roleplay events, whose prose appears in
both combined text and typed segments. Automated Unicode roundtrip and oversized
length rejection tests cover the codec.

## Development identity

Launch a client with `-RatwIdentity=ash -RatwName=Ash`. IDs are restricted to
2–32 ASCII letters, digits, hyphens and underscores. The same identity cannot
connect twice concurrently. Different identities persist separately. This is a
local prototype identity selector, not authentication; do not expose the server
as a public service until accounts and ownership verification exist.

## Storage transaction

The default database is `Saved/ratw-world.sqlite`; `-RatwSave=/absolute/file.sqlite`
isolates a test world. Unreal's SQLiteCore opens it in WAL mode with synchronous
FULL. A bound prepared statement upserts a versioned JSON world state inside one
`BEGIN IMMEDIATE`/`COMMIT` transaction. Rollback preserves the previous state on
write failure. The database connection closes before destruction.

The transaction includes dormant and connected characters, doors, weather, NPC
positions, permanent map knowledge, party membership, active NPC interactions,
permanent summaries, social sessions and reward receipts. It saves in the
background every 15 seconds; chat, NPC conversation turns, door use and colour
changes ask for a background save within three seconds (`SaveSoon()`) instead of
each making the game wait on a whole-world save. Trades, gathering and eating,
accounts, character creation, login/logout, operator actions, spawns and normal
shutdown still save synchronously before replying.
Typing, current speech markers, active paths and held input are cleared on restore.
The plain core validates saved world geometry before it is accepted. Dormant
characters stay outside the active simulation until they reconnect.

The single-row schema deliberately trades query flexibility for an atomic,
reviewable MVP checkpoint. A production migration should normalize accounts,
characters, sessions, rewards and memories while retaining transaction boundaries
and source identifiers. Schema version 1 has no automatic migration from future
versions. A crash can lose up to 15 seconds of movement and up to three seconds
of chat and conversation (a client retrying a lost chat command may then post it
again); economic and account commits are saved synchronously. The per-NPC
`live.npc_state` rows are rewritten only when a resident's state changed. In the database, saves are built and
written by the persistence worker, usually as deltas (only the rows that changed, `game.save_checkpoint_delta`), and
carry the event log's new entries (`game.events`); see `26-living-npcs.md`, Phase 2.

## Verification and remaining decisions

Automated storage tests exercise commit/reopen and payload restoration. Native
two-client scenarios verify separate identities, authoritative movement, local
speech, perception and restart. Plain-core tests cover invalid world restoration
and forgotten/live-state separation. See the root test report for actual results.

Open: production identity provider, reconnect reservations and expiry, replay
retention beyond 256 commands, incremental snapshot deltas, schema migration
policy, and deployment packaging. None is silently treated as a shipped service.
