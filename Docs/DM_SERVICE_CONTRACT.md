# Storykeeper local service contract

This first slice is a trusted-local operator tool, not a player endpoint or a
public administration server. It stores planning and political records in its
own SQLite database. It **never opens or writes the game's SQLite database**.
Native execution uses the opt-in channel in `DM_BRIDGE_CONTRACT.md`.

## Launch, authentication, and ownership

```sh
python3 tools/dm_service.py --exchange /absolute/private/bridge --state-dir /absolute/private/storykeeper --port 8780
```

Both directories must belong to the running OS user and have mode `0700`.
New directories are created with that mode; existing overly permissive folders
are rejected rather than silently changing unrelated permissions. The native
authority must use the identical bridge directory with `-RatwDMDirectory`.

The service binds only `127.0.0.1`. Each process generates a fresh random
256-bit bearer capability and atomically writes a mode-`0600` `session.json` in
the private state directory. Its `url` contains `#token=...`, never a query
parameter. The token is not printed to stdout or request logs. Open that URL in
a local browser; the frontend reads the fragment and sends
`Authorization: Bearer <token>` on every API request. Restarting the service
invalidates the prior capability. Anyone who can read this OS user's private
files is inside this trust boundary. There are no public accounts, roles,
multi-admin coordination, or remote deployment guarantees.

Requests require the exact `127.0.0.1:<port>` or `localhost:<port>` Host. If an
Origin header exists, it must exactly match the request's local HTTP origin.
Cross-origin preflights are not enabled. All API routes require authentication,
including unknown API paths. Query parameters, duplicate authorization/Host
headers, duplicate JSON keys, NaN/Infinity, unknown command fields, unsafe IDs,
oversized bodies, non-JSON POSTs, and streamed request bodies are rejected.
POST bodies are limited to 64 KiB, authority snapshots to 8 MiB.

Only the frontend's named HTML/CSS/JavaScript modules are served as static
files. The database, session capability, bridge files, repository sources, and
parent directories are never static routes. Responses use no-store, nosniff,
no-referrer, and a self-only content security policy without inline script or
style permission. The service has no CORS, arbitrary filesystem, shell, plugin,
or LLM-execution API.

## HTTP API

### `GET /api/state`

```text
{
  version: 1,
  serverTime: UTC ISO timestamp,
  bridge: {live, stale, ageSeconds, worldId, detail},
  snapshot: normalized authority snapshot | null,
  campaigns: [], beats: [], events: [], chapters: [], factions: [], opinions: [],
  activity: [], routes: [], migrationPreviews: [], audit: []
}
```

`snapshot` follows the native contract and is omniscient by design. Offline
saved characters retain `online:false`; they are not presented as live bodies.
The backend explicitly copies permitted cell, actor, account, and bounded
economic-ledger fields. It discards arbitrary extra fields, including chat,
drafts, memory text, and provider credentials. Character `recruited` is retained
and prevents migration approval. A last-good snapshot is cached persistently.
Missing, malformed, future-dated, backward-sequence, or older-than-ten-second
snapshots disable live operations while keeping last-known information visible.

The first accepted `worldId` pins the service database to that world. A changed
world identity never merges records or permits effects. Use a separate state
directory for another authority world; there is no implicit reset button.

All operator mutations, including planning edits, become read-only during an
outage. Event approvals, migration previews/approvals, and new outbox writes
require fresh authority data. Cached records and the observation-only
`chapter.peak` query remain readable. Repeating an already accepted identical
command may return its persisted response without reapplying a mutation.
Already accepted native effects are not assumed to have been cancelled merely
because snapshots stop arriving.

`activity` rows are `{chapter,weekday,hour,activeMinutes,memberMinutes,samples}`.
`routes` rows are `{chapter,source,destination,transitions}`. `audit` contains the
latest 200 `{sequence,at,action,target,detail}` records; `at` is Unix UTC seconds.
The SQLite audit itself is not trimmed automatically. Event internal envelopes
and command-receipt storage are not exposed as editable state.

### `POST /api/command`

```json
{"id":"a-new-uuid-for-this-operation","action":"campaign.upsert","payload":{"name":"The winter road","description":"An operator-authored campaign."}}
```

Success is `{ok:true,id,result}`. Rejections are `{ok:false,error}` with an
appropriate 4xx status. An accepted command ID is persisted with a canonical
request fingerprint and its response. Retrying the identical accepted request
returns that response; reusing its ID for different content is rejected.
Rejected validation attempts have no domain mutation and may be corrected with
a new ID. Domain approvals and rejections are audited without bearer tokens.

IDs are at most 80 ASCII letters, digits, hyphens, or underscores and start with
a letter/digit. Upserts create a random ID when omitted. Text is bounded,
control characters are rejected, and SQLite always uses bound parameters.

| Action | Payload |
| --- | --- |
| `campaign.upsert` | `{id?,name,description?}` |
| `campaign.delete` | `{campaignId}`; refuses deletion while referenced |
| `beat.upsert` | `{id?,campaignId,title,description?,kind?}` |
| `beat.delete` | `{beatId}`; refuses deletion while referenced by events |
| `chapter.upsert` | `{id?,name,memberIds,cellIds,housing,jobCapacity,attraction,description?,treatyModifier?}` |
| `faction.upsert` | `{id?,name,description?}` |
| `opinion.set` | `{factionId,targetType:"chapter"|"player",targetId,score,reason}` |
| `event.create` | `{title,kind,payload,scheduledAt?,campaignId?,beatId?,chapterId?}` |
| `event.approve` | `{eventId}` |
| `event.cancel` | `{eventId}`; only before dispatch |
| `chapter.peak` | `{chapterId}` |
| `migration.preview` | `{chapterId}` |
| `migration.approve` | `{previewId,npcId}` |

Campaign/beat descriptions permit newlines. Deletion never cascades silently.
Opinions range from −100 to 100 and require known targets. The authored faction
catalog and locally managed faction records both provide known faction IDs.
Political opinions currently live in Storykeeper; they do not automatically
rewrite NPC dialogue, combat AI, or player-facing faction mechanics.

## Events and native effects

Supported event payloads are:

```text
notice: {scope:"world"|"cell"|"player"|"chapter",target?,text}
weather: {cell,preset:"clear"|"rain"|"snow"|"fog"|"seasonal"}
npc_relocate: {npc,cell,x,y}
economy_transfer: {from,to,item:""|"herbs"|"meal",quantity,coins}
```

The service validates existing IDs, advertised native capabilities, resident
role/recruitment state, known traversable destinations, and real balances/stock
before dispatch. The native authority independently revalidates all effects.
Transfers move money and optional goods in the same direction between two
existing accounts. Quantity is 0–99, coins 0–1,000,000, both whole numbers;
empty transfers, unknown goods, invented accounts, negative values, or missing
funds are rejected. No minting or price override is possible through this API.

A Chapter notice resolves known member IDs into one native
`{scope:"players",targets:[...],text}` request. This avoids partially dispatched
multiple requests. The native notice reaches connected matching characters;
including an offline member does not imply an offline message inbox.

```text
draft --explicit approve--> scheduled --due + fresh--> queued
draft/scheduled/blocked --cancel before dispatch--> cancelled
scheduled --missed window or invalidated conditions--> blocked
queued --verified native rejection / unconfirmed timeout--> failed
queued --native success, plus arrival verification for moves--> applied
```

`brigands`, `assassins`, `war`, `faction-collapse`, `combat`, and `route-robbery`
can be stored as narrative plans, but are created **blocked** with an explicit
unimplemented-executor reason. They never become applied through timers or
cosmetic UI feedback.

Dates must be explicit UTC (`Z` or `+00:00`); the scheduler never silently
changes a confirmed time based on later player activity. Missing schedule means
now, but the event still starts as a draft. Schedules over a year ahead are
rejected. A due event can wait briefly for a fresh authority; if its fixed
window is missed by more than 120 seconds, it becomes blocked and requires a
new deliberately approved draft. It does not surprise players hours later.

## Crash/retry boundary

The SQLite transaction commits an immutable outbox envelope and accepted
command receipt **before** publishing the native request file. Files are
written to a private temporary file, fsynced, and atomically replaced. After a
service restart or transient write failure, publishing retries that exact ID
and envelope, never a newly authorized effect. Publication stops while stale.
Native requests expire after 120 seconds.

The native receipt must match version, request ID, world ID, boolean result,
and plausible application time. A reported success after request expiry is
rejected. Native failure is failed; queued is never mislabeled as applied.
No result before expiry becomes failed **with an explicit unconfirmed-outcome
message**, not a claim that a possibly applied operation was rolled back.
Dispatched effects cannot be cancelled safely through this first file protocol.

Native receipts are bounded by the authority, not an unlimited exactly-once
journal. Storykeeper's accepted-command and event records persist; directory
loss, authority save rollback, manual file tampering by the trusted OS user,
and receipt-retention exhaustion are not distributed transaction guarantees.
Back up each world's private operator data deliberately. There is no automatic
pruning, public service hardening, load/performance claim, or recovery UI yet.

## Chapter profiles and observation

Chapter membership is bounded to 128 IDs, matching the native grouped-notice
limit. Housing and job capacity are **operator-declared** integers 0–10,000. Attraction
is a declared score 0–100; `treatyModifier` is 0–1 and defaults to zero. These
are not measured buildings, jobs, or a completed player construction system.

`cellIds` may provide an explicitly audited local Chapter-site overlay when a
native cell has no authored Chapter. Such profiles may not overlap another
operator-declared Chapter or contradict a nonempty native authored Chapter.
They never edit an Atlas project, its claims, or native cell ownership. The
profile's `siteAuthority` field labels this bootstrap. The default demonstration
region is `demo_reach`; no canon faction or territorial claim is invented.

The service records only observed activity after a Chapter profile exists.
At adjacent fresh snapshots, members active at both ends contribute elapsed
UTC minutes to weekday/hour buckets; member-minutes also count the number of
active members. Intervals over ten seconds are discarded instead of filling
downtime with imaginary presence. Hour boundaries are split. Duplicate polls
and restarts of the same snapshot do not add minutes.

Player cell changes between consecutive fresh snapshots are counted as
observed transitions. Intermediate cells traversed between samples are not
inferred, and this is not a complete journey log. Offline bodies and typing-only
heartbeats do not create active minutes; the native authority defines meaningful
activity. No raw chat or drafts are read or retained.

`chapter.peak` returns `{chapterId,suggestion,reason}`. With no observations,
`suggestion` is null. Otherwise it supplies the most-observed weekday/hour and
the next future occurrence as `{weekday,hourUtc,scheduledAt,observedMinutes,samples}`.
Weekday zero is Monday. The DM must explicitly choose that suggestion for an
event; it never reschedules existing events. Small samples are not a predictive
claim about player availability.

## Migration preview and verified resentment

Only explicit operator approval is implemented. There is no unattended
population drain, newly spawned resident, or hidden quota approval.
The migration workflow attaches the political consequences below; a generic
`npc_relocate` event is an explicit administrative move without inferred
Chapter politics or a migration cooldown award.

Previews expire after five real minutes and contain:

```text
{id,chapterId,worldId,createdAt,expiresAt,snapshotSequence,
 capacity:{housing,jobs,occupied,reserved,available},blockedReason,
 candidates:[{npcId,name,sourceCell,destinationCell,x,y,score,reasons,
              sourceFactions,resentment}]}
```

Candidates are finite named native resident-role NPCs; cook, keeper, forager,
recruited actors, actors already moving homes, and actors with pending moves
are excluded. The existing home and destination must share an explicitly named
region. `unassigned` regions do not establish a migration relation. Destination
points come from known traversable terrain, with final navigation validation
left to the authority. Housing and jobs must both have remaining declared
capacity, minus observed homes and pending approved migration reservations.

Initial deterministic ranking is declared attraction plus up to 12 points for
currently active Chapter members, plus a small need term for an observed purse
below 12 pennies and an empty carried-meal stock. Ties use stable NPC IDs. This
does not claim to measure happiness, employment history, or an actual wage
contract. A new preview with unchanged observations has the same candidate
ordering and terms. Approval rechecks origin, eligibility, capacity, claims,
destination point, and resentment terms; changed terms require another review.

A native relocation receipt only accepts real navigation. The event remains
queued until a **fresh** snapshot shows the requested `homeCell`/home point,
`relocating:false`, and a cleared target. After ten minutes without verified
arrival, its result is unconfirmed/failed; native motion might still be in
progress. No resentment is applied to failed, cancelled, or unverified moves.

On verified arrival, the service records a 24-real-hour provisional migration
cooldown. Source-home territorial claim factions receive a bounded opinion
reduction toward the destination Chapter; the declared treaty modifier reduces
it, down to zero. Opinion values clamp at −100. The transaction stores the
arrival, opinion changes, and applied event together, preventing repeated polls
or restarts from charging resentment twice. Without explicit source claims,
there is no invented resentful faction. This is persistent DM political state,
not yet autonomous hostility or an NPC combat executor.

The native home affects resting/social time. Existing demonstration work and
food routes remain their original commute; this is not a complete resettlement
economy, new job assignment system, or guarantee of a sustainable destination.

## Verification

Run `python3 tools/test_dm_service.py`. Tests use temporary private directories,
a deterministic clock, synthetic authority snapshots/receipts, and a real
loopback HTTP server. They do not launch Unreal or touch production game saves.
Native authority, graphical frontend, real NPC navigation, and end-to-end
bridge verification are separate integration checks and must not be inferred
from this isolated service suite.
