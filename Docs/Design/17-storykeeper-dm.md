# Storykeeper: a separate world-directing application

## Product boundary

Atlas builds places. Storykeeper directs stories in a running world. Neither is
a privileged player character. Storykeeper is a separate local-browser app and
service with its own database, launch command, operator session and private
authority bridge. Ordinary character credentials and commands cannot grant DM
access. Omniscient exports must never enter observer-filtered player snapshots.

This first slice serves a trusted local operator. Public deployment, remote
operators, staff roles, MFA, approvals by a second DM, and revocable production
credentials are separate release gates—not capabilities implied by a bearer
token on loopback. The exchange directory is readable only by its trusted OS
owner. A separate operator machine will need a real authenticated encrypted
control service rather than a shared public folder.

## Workspaces

1. **World watch:** all authored cells, faction claims and Chapter sites, online
   characters/NPC positions, saved offline characters with last-known locations.
   Show the snapshot's age and connection state; never call stale positions live.
2. **Chapters:** members, settlement sites, declared capacities, observed active
   presence, usual play windows, standing, route use, migration previews.
3. **Factions:** catalogs, directed opinions toward Chapters and players, reasons
   and intervention history. Later add diplomacy, armies and lifecycle state.
4. **Campaigns and beats:** persistent story outlines, participants, target scope,
   planned encounters and linked executable events.
5. **Event desk:** validate a draft, review targets/effects, approve now or at a
   fixed UTC time, inspect actual completion/failure, cancel before dispatch.
6. **Audit:** who/what requested a change, why, when, queue state and authority
   result. Do not conflate an operator intention with a successful world effect.

Observed activity is operational metadata, not copied roleplay transcripts.
The bridge excludes messages, NPC memories, drafts and provider credentials.
Counts come from meaningful recent player input, not typing presence alone.
The service aggregates observed minutes in UTC weekday/hour buckets and cell
transitions. A route count is a coarse transition, not exact pawprints, a planned
journey or proof that a caravan carried valuable goods.

## Events as typed operations

Every future executor needs a schema, target resolver, preconditions, authority
validator, resource budget, idempotency rule, expiration/cancellation policy and
completion evidence. UI availability comes from advertised native capabilities.

| Capability | First slice | Needed for the fuller story system |
| --- | --- | --- |
| World/Chapter/cell/individual announcements | Connected-recipient delivery, Chapter resolved to members | Offline inbox, durable delivery acknowledgments |
| Weather intervention | Explicit preset or resume seasonal weather | Duration, regional fronts, conflict policy |
| NPC relocation | Existing residents physically travel and change home | Incentive contracts, autonomous choices, family/profession ties, real local jobs |
| Commerce manipulation | Transfer existing money/goods between real accounts | Demand contracts, tariffs, embargoes, caravans, explicit capped mint policy |
| Assassins | Campaign/beat planning only; executor blocked | Combat, actor templates, pursuit/perception, injury/death and moderation rules |
| Brigands and route pressure | Campaign/beat planning only; executor blocked | Convoys, cargo, bounded encounter placement, loot conservation, counterplay |
| Army mobilization/attack | Campaign/beat planning only; executor blocked | Forces, supply, travel, siege/combat, damage and civilian consequences |
| Faction collapse | Campaign/beat planning only; executor blocked | Territory succession, institutions/assets/debts, displaced residents and historical records |

The blocked rows are deliberate honesty in the application: no fake assassin
tokens, imaginary casualties or successful “army attack” badges. These planned
beats remain useful campaign records, but cannot mutate the world until their
executors exist. Events currently contain one typed effect; conditional chains,
dependency graphs, branching campaigns and reusable encounter templates are
next-stage authoring features, not implied by the campaign list.

### Scheduling and completion

```text
draft → approved scheduled → queued to authority → applied / failed
   └→ cancelled              └→ accepted navigation → verified arrival
unavailable executor → blocked story plan
```

Peak suggestions use actual accumulated activity; absent history produces no
invented “usual playtime.” The DM explicitly chooses a suggested future UTC
window. Approval fixes the timestamp; it does not silently move as newer
observations arrive. A peak is an estimate, not guaranteed attendance. More
advanced minimum attendance, postponement and expiry policies need explicit
controls and maximum waits. These schedules use real UTC, not accelerated game
calendar days. The service must be running for scheduling to execute.

If the world is stale/unavailable, cached planning remains readable but all
mutations and live dispatch stop. Requests are world-bound and short-lived. Replays use the same ID;
changing payload while recycling an ID is invalid. The native authority commits
effects and a bounded receipt to its checkpoint before acknowledging. A
relocation acknowledgment only means travel started: political consequences
wait for arrival. Announcements are queued after checkpoint success but are not
an exactly-once durable inbox across crashes.

Once dispatched, cancellation is not a rollback. An eventual undo system should
use explicit compensating effects—return funds, withdraw a force—rather than
rewinding a shared world over unrelated player actions. Native receipt retention
is bounded to 512; Storykeeper command receipts and audit persist without automatic
trimming. Neither storage is a distributed exactly-once guarantee or a backup.

## Future orchestration examples

**A Chapter drains regional labor:** show source losses and town food/jobs,
offer a labor treaty, let a faction issue a warning, then expose taxation or
sanctions as authored responses. Do not jump automatically from migration to war.

**Brigands target a route:** select coarse observed Chapter routes, reserve an
encounter budget, check terrain and current participants, place real brigands,
and track stolen goods into their inventories. Give warnings, scouts and
alternative paths; neither invent cargo nor teleport attacks onto offline wolves.

**A faction collapses:** mark institutions failing, move/retire forces and
officers, distribute or abandon assets, persist displaced residents, and record
successor claims. Never implement collapse by deleting a faction's history and
every associated NPC.

## Implementation references

- `Docs/DM_BRIDGE_CONTRACT.md`: private native snapshot/request/result schema.
- `Docs/DM_SERVICE_CONTRACT.md`: HTTP API, planner state, scheduling and policy.
- `Docs/Design/16-territory-chapters-migration.md`: migration and territorial rules.
- `Docs/TERRITORY_AUTHORING_CONTRACT.md`: Atlas schema and manifest extensions.

The next implementation gate should be real Chapter membership/build rights and
NPC/job placement in Atlas, followed by funded settlement provisioning. Combat
and encounter executors then become real components to build and test, not a
collection of buttons promising effects the simulation cannot yet support.
