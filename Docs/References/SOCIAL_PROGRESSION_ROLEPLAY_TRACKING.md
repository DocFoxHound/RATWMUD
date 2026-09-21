# RATW Social Progression and Roleplay Tracking

Status: implemented local-prototype behavior and adjacent experimental design, as
of 2026-09-20.

This document is a portable technical and game-design handoff for RATW's social
progression, social-session tracking, Stories, endorsements, authored roleplay,
NPC relationships, and experimental roleplay rewards. It deliberately separates
systems that award live Social XP from systems that only preserve narrative or
evaluate a possible future reward model.

## 1. The Short Version

RATW has five related but distinct social data lanes:

1. **Live account Social XP and Social Level.** Qualified multiplayer scenes,
   Gold Stars, Story closure bonuses, and Story Stars can increase Social XP.
   Social Level is a simple display value: `1 + floor(Social XP / 100)`.
2. **Earned social tier.** Regular, Gifted, and Quickened are multi-factor account
   tiers. XP alone is insufficient. The player must also have enough distinct
   rewarded partners, rewarded sessions, and rewarded completed Stories.
3. **NPC relationship state.** Trust, respect, annoyance, hostility, familiarity,
   and conversation count are tracked per character/NPC pair. These values do not
   currently award Social XP or determine the account social tier.
4. **Authored roleplay continuity.** Profiles, journals, consent-reviewed recaps,
   scene listings, obligations, and cooperative projects are stored separately
   from the reward ledger. They currently award no XP, tier credit, items, or
   global canon status.
5. **Shadow reward evaluation.** A pure metadata evaluator can score reciprocal
   participation, milestones, continuity, and recognition. The server advertises
   this mode as `OFF`; its output is always labeled as a candidate and never
   mutates live XP, tiers, rewards, or entitlements.

The core design rule is:

> Track roleplay broadly, but let only narrow, server-validated, replay-safe
> metadata affect live progression. Do not put chat prose into the progression
> ledger and do not let AI grade writing quality or directly grant rewards.

## 2. Terminology and Scope

| Term | Scope | Meaning |
| --- | --- | --- |
| Account | Player-wide | Owns Social XP, Social Level, earned tier flags, rolling caps, rewarded-session count, Story count, and partner diversity |
| Character | In-world identity | Owns character ID, profile, journals, recaps, obligations, project participation, physical/magic XP, and NPC relationships |
| Social XP | Account-wide | Materialized integer total increased only by committed social rewards |
| Social Level | Account-wide display | `1 + Social XP // 100`; every 100 XP advances one displayed level |
| Social Tier | Account-wide unlock | `REGULAR`, `GIFTED`, or `QUICKENED`; requires all configured XP, diversity, session, and Story thresholds |
| Session / Scene | Multiplayer interaction | One authoritative membership and audience container. In current code, `scene_id == session_id` conceptually |
| Qualified contribution | Session-local | At least two meaningful turns, at least 35 meaningful words, and at least one valid recent reply |
| Eligible session | Session-local | At least two participants meet the contribution shape when the session ends |
| Rewarded session | Account progression | The player receives positive session XP after crowd, repetition, daily, and total-cap rules |
| Story | Reward continuity | A participant-approved chain of ended qualified sessions with clan/event gates |
| Authored recap | Narrative continuity | Player-written record with exact-revision unanimous publication consent; not a rewarded Story |
| Gold Star | Session endorsement | Binary recognition after an ended qualified session; may award up to 2 XP before decay/caps |
| Story Star | Story endorsement | Binary recognition after a rewarded Story closes; may award up to 4 XP before decay/caps |
| Shadow points | Experimental only | Candidate comparison output from the roleplay evaluator; never live XP |

### Social Level versus Social Tier

These are intentionally different:

- Social Level rises automatically from Social XP at 100-XP intervals.
- Social Tier is an earned identity/unlock classification and needs multiple
  independent signals.
- A player can have a higher Social Level and remain `REGULAR` if they have not
  met diversity, rewarded-session, or completed-Story requirements.
- Tier flags are sticky once earned. Later threshold/config changes do not remove
  a previously persisted unlock.
- The prototype character is always an adult Quickened Fire Warden for testing.
  `prototype_character_unlock_bypass: true` explains why the playable character
  may be Quickened while the account's earned social tier is still Regular.

## 3. What Currently Raises Social XP

| Source | Can award live Social XP? | Rule |
| --- | --- | --- |
| Qualified session settlement | Yes | Base up to 20 XP, then public/private, crowd, repeated-partner, session-count, daily-XP, and integer-cap reductions |
| Gold Star | Yes | Up to 2 XP, no more than the recipient's paid session XP, then pair/giver/day/cap reductions |
| Story closure | Yes | Incremental continuity bonus only; original session XP is never replayed |
| Story Star | Yes | Up to 4 XP, then pair/giver/day/cap reductions |
| Ordinary `chat` activity endpoint | No | Legacy idempotent acknowledgement, 0 XP |
| Ordinary `emote` activity endpoint | No | Legacy idempotent acknowledgement, 0 XP |
| `/me` action text | No current contribution credit | Runtime word counter treats slash-prefixed text as zero words |
| Typing/composing presence | No | Presentation-only; throttled and transient |
| Pause, resume, join, leave, reconnect | No | Lifecycle and audience operations only |
| Standing near players | No | Proximity alone never creates contribution credit |
| Authored profile, journal, recap, listing, obligation | No | Narrative storage is isolated from reward storage |
| Weather Shelter project work | No | Project-local state only; no XP, items, canon, roles, or world mutation |
| NPC conversation/relationship changes | No | Relationship state is separate from account social progression |
| AI-generated text or AI analysis | No | AI cannot directly grant XP, items, ranks, missions, or canon |
| Shadow reward evaluator | No | Candidate points only; no live mutation adapter |

Physical and magic progression use a separate activity map: bite grants 2 Physical
XP, jump grants 1 Physical XP, and fireball grants 2 Magic XP. Those tracks are
character-scoped. Social XP is account-scoped.

## 4. End-to-End Live Progression Flow

```text
player sends Local speech
        |
        v
authoritative Unreal server validates sender, audience, blocks, scene revision,
message size, rate limit, and delivery
        |
        +--> routes text to authorized recipients (text is not sent to reward DB)
        |
        v
runtime counts words, suppresses recent duplicate content, resolves one credited
session, and submits typed metadata with stable request UUIDs
        |
        v
SQLite backend validates account, membership, pacing, reply shape, and command
idempotency; updates contribution counters in one transaction
        |
        v
session ends -> backend snapshots qualification and computes each reward
        |
        v
social_rewards receipt + positive progression_ledger row + accounts.social_xp +
account revision + counters/tier flags commit atomically
        |
        v
current profile/social state returns to Unreal and owner UI displays confirmed XP,
Social Level, tier, sessions, Stories, and persistence status
```

There is no optimistic XP increment in the client. The HUD retains the last
confirmed totals while requests are pending or the backend is degraded.

## 5. Scene Formation, Audience, and Contribution Capture

### 5.1 Scene formation

Scenes can be manual or automatic.

- A manual scene may begin with one account, but it cannot become reward-eligible
  until at least two participants qualify.
- An automatic scene forms from a mutually heard alternating public exchange,
  effectively A-B-A or B-A-B, within a 30-second candidate window.
- Passive listeners are not automatically added as participants.
- Private speech never creates an automatic scene.
- One account may be in at most eight active memberships. The backend supports up
  to 48 historical members per scene; the current Unreal runtime holds 16 live
  members per scene and 128 scenes per world.
- A player selects one scene as the reward/audience context. One delivered line is
  never submitted as reward-bearing speech to multiple scenes.

### 5.2 Audience authority

The Unreal server, not the client or backend caller, decides the actual recipients.
Each scene has an `audience_revision`. A request captured against an old revision
fails with `AUDIENCE_CHANGED` rather than silently targeting a new roster.

Private Local speech requires a live, unpaused, unambiguous scene membership and is
delivered only to current authorized members. Blocks are mutual delivery denials.
Replies cannot widen the original message audience. World, OOC, and prototype Clan
channels are not private-scene reward containers.

### 5.3 Word and reply tracking

The runtime counts alphanumeric word runs. Apostrophes and hyphens remain inside a
word when surrounded by alphanumeric characters. Slash-prefixed commands count as
zero, which currently excludes `/me` action text from reward evidence.

Before submitting reward metadata, the runtime:

- normalizes the speech to lowercase whitespace-separated tokens;
- stores only a short-lived in-memory hash, never the text, for duplicate checks;
- suppresses matching content for 60 seconds;
- sends `word_count`, `voice_mode`, `session_id`, and an optional recent reply
  target account to the backend;
- never sends chat text, an XP amount, a client-computed quality score, or a
  client-computed eligibility flag.

A backend speech command accepts 1 to 500 words. A **meaningful turn** has at least
5 words. A reply is valid only when it targets another currently present,
unblocked participant whose last meaningful turn in that scene was no more than
180 seconds ago. Self-replies fail.

The account-wide backend cadence is one accepted speech event every 2 seconds.
Each account/scene counter is bounded to 200 meaningful turns and 10,000 meaningful
words.

### 5.4 Activity versus reward evidence

Scene activity and reward qualification are separate:

- An accepted nonempty, nonduplicate line may reset scene inactivity even if it is
  shorter than five words and therefore adds no qualifying turn or words.
- Presence, an invalid submission, a blocked send, a duplicate, or slash-command
  action text does not create contribution credit.
- This allows short natural replies to keep a scene alive without rewarding volume.

### 5.5 Current pacing

New pacing-policy-v2 scenes use:

- 15 minutes without accepted activity: `COOLDOWN` warning state;
- 30 minutes without accepted activity: `ENDED` and settled once;
- explicit pause: up to 24 hours before ending;
- disconnect reservation: 5 minutes for explicit same-world reconnect.

Historical pre-v2 rows retain the old 3-minute cooldown and 6-minute end behavior.
Some older documentation still describes those legacy timings; current code and
new scenes use 15/30 minutes.

## 6. Qualification and Session XP Formula

### 6.1 Contribution qualification

At settlement, a participant has the required contribution shape when all are true:

- `turns >= 2` meaningful turns;
- `public_words + private_words >= 35`;
- `replies >= 1` valid recent reply.

The scene is eligible only if at least two participants satisfy that shape. A scene
can be eligible even if a daily cap later reduces one or more individual payouts to
zero.

The backend records three different states that should not be conflated:

1. **Qualified shape:** counters meet the turn/word/reply rule.
2. **Qualified member in an eligible scene:** `social_members.qualified = 1` when
   the scene has at least two qualified contributors.
3. **Rewarded contribution:** `base_xp > 0`. Only positive paid session rewards
   increment the account's rewarded-session count and partner diversity. Story
   carryover also requires positive paid XP.

### 6.2 Base formula

For one qualified participant:

```text
words = public_words + private_words
voice_weight = (4 * public_words + 3 * private_words) / (4 * words)
crowd_weight = 1.00 for contributors 1-4
               0.60 for contributors 5-8
               0.25 for contributors 9+

base_xp = floor(20 * voice_weight * crowd_weight)
```

Equivalent integer implementation:

```text
base_xp = floor(
  20 * (4 * public_words + 3 * private_words) * crowd_percent
  / (4 * words * 100)
)
```

Contributors are ordered by join time, then account key. All-public speech starts
at 20 XP; all-private speech starts at 15 XP. A mixed scene is weighted by actual
public/private word counts and rounded down once.

### 6.3 Repeated-partner decay and rolling limits

For every co-qualified peer, the backend counts other qualified sessions shared in
the previous rolling 24 hours. The most repeated peer controls the reduction:

| Previous shared qualified sessions | Session multiplier |
| ---: | ---: |
| 0 | 1.0 |
| 1 | 0.5 |
| 2 | 0.25 |
| 3 | 0.125 |
| 4+ | 0 |

The configured defaults are:

- at most 8 positive rewarded sessions per account per rolling 24 hours;
- at most 100 total Social XP from all social reward kinds per account per rolling
  24 hours;
- total Social XP capped at signed 32-bit maximum.

The final session request is zero when the repeated-pair limit or rewarded-session
limit is reached. Otherwise it is `floor(base_xp / 2^repeat_count)`. The shared
daily XP cap then clamps the result again. A zero award is still written to the
social reward receipt table so the same source cannot later be replayed for value.

## 7. Gold Stars, Stories, and Story Stars

### 7.1 Gold Stars

A Gold Star is binary peer recognition, not a rating scale.

- The scene must be ended and eligible.
- Giver and recipient must both be qualified participants.
- Self-awards fail.
- A giver/recipient pair can create only one Gold Star per scene.
- The base request is `min(2, recipient_session_xp)`.
- The star is recorded even when decay or caps make its XP value zero.

Across both Gold and Story Stars, reciprocal pair activity in the prior rolling day
uses the following XP decay: first 1.0, second 0.5, third 0.25, fourth and later 0.
Only the first ten stars given by an account in that rolling day can produce XP.

### 7.2 Rewarded Stories

A rewarded Story chains qualified sessions. It is not the same thing as a personal
journal or shared recap.

- A proposal starts from one ended eligible session.
- At least two positively rewarded contributors from the first eight contributors
  must be carryover-eligible.
- Every carryover participant must have active membership in the Story's required
  provisioned clan scope, event scope, or both.
- The proposer auto-approves. At least two thirds of participants must approve to
  move the Story from `PENDING` to `ACTIVE`.
- An unapproved proposal expires after 24 hours, closes without reward, and releases
  its session back to standalone history.
- An `ACTIVE` Story pauses after 24 hours without continuation.
- A continuation uses another ended eligible session, must share at least one prior
  contributor, and rechecks required scopes.
- A scene can belong to only one Story. A Story supports at most 32 sessions, 48
  members, and each account can belong to at most 8 open Stories.
- Only the Story owner can close it, after at least two linked sessions. Current
  participant voting controls promotion, not closure.

For each member with positive carryover XP in at least two linked sessions:

```text
closure_bonus = floor(sum(paid carryover session XP) / 4)
                + min(number of carryover sessions - 1, 5)
```

The normal daily XP and total caps still apply. Session rewards are never replayed,
and a branch never inherits or replays parent rewards. `completed_stories` increases
only when the closure bonus actually pays positive XP.

### 7.3 Story Stars

- The Story must be closed.
- Giver and recipient must be Story members and must each have positive paid
  continuity rewards for that Story.
- Each giver can award one Story Star total per Story.
- Base request is 4 XP before pair decay, giver/day limit, daily XP cap, and total cap.
- Repeating the same award is an idempotent no-op; trying to switch recipient after
  giving the Story Star is rejected.

## 8. Social Tier Unlocks

Default account requirements are:

| Tier | Social XP | Distinct rewarded partners | Positive rewarded sessions | Positive rewarded Story closures |
| --- | ---: | ---: | ---: | ---: |
| Regular | 0 | 0 | 0 | 0 |
| Gifted | 200 | 3 | 5 | 1 |
| Quickened | 1000 | 8 | 20 | 3 |

All four columns must be satisfied together.

- Diversity is a sparse set of distinct co-contributors from positively rewarded
  sessions, capped at 128 tracked peers.
- Qualified-session and completed-Story counters increase only after positive XP.
- Counters saturate at signed 32-bit maximum.
- `gifted_unlocked` and `quickened_unlocked` persist once true.
- Thresholds and cadence/caps are startup configuration, not client fields.
- Quickened requirements are validated to be at least Gifted requirements.

## 9. Persistence, Ledgers, and Audit Separation

The SQLite transaction is the authority for committed progress. The main tables are:

| Table | Purpose | Contains prose? | Affects live XP? |
| --- | --- | --- | --- |
| `accounts` | Materialized `social_xp`, account revision | No | Yes, current total |
| `characters` | Character identity, physical/magic progression and traits | No | Not Social XP |
| `progression_ledger` | One immutable positive XP row per committed reward event | No | Yes |
| `social_accounts` | Sticky tier flags, rewarded-session count, completed-Story count, cadence times | No | Tier inputs |
| `social_commands` | Stable request UUID, actor, operation, canonical metadata fingerprint, parameters, original result | No chat prose by contract | Indirectly; idempotency/source command |
| `social_sessions` | Owner, stage, Story link, pacing, audience revision, eligibility | No | Settlement source |
| `social_members` | Presence, turns, public/private words, replies, qualification, paid base XP, Story eligibility | No | Settlement evidence |
| `social_rewards` | Stable reward key, recipient, source, kind, paid XP including zero receipts | No | Yes |
| `social_peers` | Distinct positively rewarded co-contributors | No | Tier diversity |
| `social_stars` | Binary giver/recipient/source recognition, including zero-value stars | No | Possible Star source |
| `social_stories` | Rewarded continuity chain, gates, state, owner, timestamps | Story name only | Possible closure source |
| `social_story_members` / `social_story_votes` | Historical roster and promotion consent | No | Story eligibility/state |
| `social_scopes` / `social_scope_members` | Operator-provisioned clan/event gates | No | Story authorization |
| `social_event_log` | Selected lifecycle metadata such as cooldown, end, timeout, and compatibility events | No chat prose | Audit only |

### 9.1 Atomic reward write

For a positive reward, one `BEGIN IMMEDIATE` transaction:

1. checks/reuses the stable reward key;
2. computes rolling usage and remaining integer capacity;
3. inserts `social_rewards`, including a zero row when capped;
4. derives a deterministic UUID for a positive `progression_ledger` row;
5. increments `accounts.social_xp` and `accounts.revision`;
6. updates counters, peers, and tier flags when applicable;
7. commits the command receipt and returned state.

This prevents a ledger row without its total, a total without its receipt, and
duplicate payout after a lost response.

### 9.2 Idempotency and retries

Every social command has a canonical lowercase UUID and a SHA-256 fingerprint of
the complete normalized request. Exact retries return the original operation result
plus current profile/social state. Reusing the UUID with any changed actor,
operation, or parameter returns a conflict. An uncertain result must be retried with
the same UUID, never a new UUID.

The Unreal backend bridge processes one ordered social command lane, reuses UUIDs
on transient retries, and rejects stale or decreasing profile responses. However,
its pending outbound queue is currently memory-only. A game-server crash can lose
an unsent command; a command already committed in SQLite remains durable even when
its response was lost.

### 9.3 Legacy compatibility lanes

- `/v1/activities` still acknowledges `chat` and `emote` with zero XP. It is not a
  social progression path.
- `/v1/social/session-event` stores older interaction membership/state metadata.
  Those rows do not become reward-bearing sessions.
- All new reward-bearing scenes use `/v1/social/command`.

## 10. Authored Roleplay Tracking, Separate from Rewards

The authored roleplay module exists to preserve identity and continuity without
turning private prose into reward evidence.

### 10.1 Character profiles

Profiles have a revision, published flag, public projection, and private projection.

- Visible fields: apparent name, visible traits, activity, immediate goal, invitation.
- Private fields: motives, biography, aliases, private notes.
- Other characters can inspect only a published public projection.
- The owner sees both projections.
- Profile saves use expected revision and fail on conflict.

### 10.2 Journals

Journals are owner-only records with create, update, read, list, and delete behavior.
The store retains current prose rather than a prose revision history. Delete clears
the text and access metadata while preserving a tombstone/receipt boundary against
replay.

### 10.3 Shared recaps and consent

Recaps start as private `DRAFT` records and contain:

- current text;
- source session IDs;
- every affected character ID from those historical source scenes;
- intended visibility and readers;
- content revision and ACL revision;
- expiry and review expiry.

Source membership proves provenance, not publication consent. Submission creates
revision-scoped review grants for all affected characters. Publication requires the
set of exact-revision approvals to equal the complete affected-character set.
Editing, declining, revoking, deleting, expiry, or ACL change invalidates grants and
approvals. Published access is public or limited to explicitly granted character
IDs and expires as configured.

This unanimous recap consent is deliberately stricter and semantically different
from the two-thirds vote that promotes a rewarded Story.

### 10.4 Scene listings and obligations

- Scene listings are explicit opt-in discovery records tied to a live social
  session, with premise, public location label, revision, and 15-minute expiry.
- Ending the scene or revoking the listing hides it.
- Obligations are explicit offers among named character parties. Each party accepts
  independently; an offer can be withdrawn.
- Obligations currently create no debt score, affinity, penalty, XP, or tier effect.

### 10.5 Narrative storage and receipts

Authored prose is stored only in `roleplay_profiles` and `roleplay_records`, not in
`social_commands`, `progression_ledger`, or generic reward event payloads. Access is
implemented through `roleplay_grants`, `roleplay_approvals`, and `roleplay_parties`.

`roleplay_receipts` stores request identity, operation, keyed request digest, event
ID, timestamp, and a content-free mutation result consisting of IDs/revisions/status.
Read results are never persisted for replay because ACLs may change. Current prose
is retained until update/delete/expiry behavior changes it; metadata receipts and
tombstones are retained indefinitely in this prototype.

None of these narrative records currently awards live progression.

## 11. Cooperative Project Tracking

The Weather Shelter project system is another non-paying roleplay lane:

- A project is bound to an existing social scene and its current membership.
- Contributors explicitly join.
- A contributor proposes an authored method/source against a milestone with a
  unique evidence UUID.
- A different joined, connected, unpaused account must verify the proposal.
- Milestones are revisioned and can be rolled back with dependent outcomes.
- Close derives `COMPLETED`, `PARTIAL`, or `ABANDONED` from verified local state.
- Stable receipts and evidence tombstones prevent duplicate or resurrected work.

The project module writes only `project_*` tables. It explicitly does not mutate
XP, missions, social sessions, global canon, roles, props, actors, inventories, or
world state. Authored catalog sources indicate what the player selected; they are
not proof that a physical in-world action occurred.

## 12. NPC Relationship and Conversation Tracking

NPC relationship state is character-scoped and separate from account Social XP.
For each character/NPC pair, the backend stores:

- `trust` and `respect` in `[-100, 100]`;
- `annoyance` and `hostility` in `[0, 100]`;
- nonnegative `familiarity`;
- `interaction_count`;
- last conversation ID and current last summary.

After a summarized conversation, server-supplied bounded metrics can change trust,
respect, annoyance, and hostility by at most 5 in either permitted direction.
Familiarity increases by `max(1, floor(turn_count / 2))`, and interaction count
increases once. Summary/conversation UUIDs make submission idempotent.

The backend also retains conversation-summary records and a metadata audit event.
These relationship values can inform deterministic NPC context, but currently:

- they do not add Social XP;
- they do not count as rewarded sessions, diversity, or Stories;
- the language model cannot grant missions, XP, items, powers, titles, or rank;
- deterministic game/backend code owns all consequential actions;
- older summaries are not automatically promoted into the newer authored-memory or
  reward systems.

## 13. Shadow Roleplay Reward Candidate

`backend/roleplay_rewards.py` is a pure, metadata-only evaluator for a possible
future contribution model. It performs no I/O and has no live mutation hook.
Current capabilities report `reward_mode: "OFF"`.

### 13.1 Candidate evidence

Allowed contribution kinds are:

- `speech`;
- `authored_action`;
- `directed_gesture`;
- validated `choice_attempt`.

Additional metadata can record milestone agreement, continuity links, and one of
five recognition categories: hosted opening, welcomed newcomer, offered choice,
mentored, or carried forward.

The evaluator accepts IDs, timestamps, membership/audience snapshots, typed action
IDs, validation results, references, and consent. It explicitly accepts no prose,
content hashes, free-form reasons, or AI quality scores.

### 13.2 Reciprocal baseline

A human account enters the stable reciprocal set only when it has at least two
accepted, cadence-separated contributions and reciprocal reply-reference edges to
another stable human. The default cadence is 2 seconds. Content repeats and invalid
actions are retained as validation outcomes but do not count.

### 13.3 Default candidate points

| Component | Default candidate value |
| --- | ---: |
| Reciprocal scene base | 10 |
| Agreed milestone | 2 |
| Agreed continuation | 2 |
| Valid recognition | 1 |
| Per-scene maximum | 15 |
| Per-account rolling-day maximum | 60 |
| Base-paying scenes per rolling day | 4 |
| Recognition points per rolling day | 2 |
| Pair recognition reuse window | 7 days |

Milestone and continuity bonuses require referenced accepted contributions plus
agreement events from at least two stable affected participants. Recognition must
refer to an earlier accepted contribution by the recipient, cannot be self-given,
and is pair-limited.

The evaluator can preview catalog thresholds at accumulated base totals of 20, 40,
80, and 120, but these are only preview names. It grants no cosmetic, role, item,
or entitlement.

### 13.4 Consent and audit behavior

All human members must consent before any incoming event metadata is examined or
retained. Mixed or withdrawn consent invalidates and purges the whole comparison
run state. Prefixes, event UUIDs, deliveries, evidence sequence, settlement order,
and configuration fingerprint are immutable and replay-checked. Missing dependency
evidence stays pending rather than being guessed.

The evaluator flags suspicious patterns for review, including five scenes within
ten minutes and three-account recognition cycles. Review flags do not retroactively
change points. Every output says `candidate, no XP awarded`.

## 14. Privacy and Data-Minimization Rules

The implemented and intended boundaries are:

- Social progression stores participation metadata, not chat transcripts.
- Reward commands reject extra fields, including text, timestamps supplied by
  clients, quality scores, XP amounts, and eligibility claims.
- The aggregate XP totals and generic profile persistence status use ordinary
  PlayerState replication. Detailed scene and Story state, tier flags, and social
  command status are owner-only.
- Composing presence is optional, scene-scoped, sent no more than once every 3
  seconds, and expires after 10 seconds.
- Authored prose has its own ACL and revision store.
- Narrative publication is not proof of knowledge, canon, Story eligibility, or
  reward entitlement.
- AI output is presentation/content, never reward authority.
- Account blocks prevent scene creation/join/reconnect and communication in the
  social lane. Block-aware authored discovery/contact filtering is still incomplete.

## 15. Failure and Consistency Behavior

- Backend social mutations use SQLite `BEGIN IMMEDIATE` transactions.
- Stable UUIDs and payload fingerprints make request retry idempotent.
- A duplicate exact request returns its original operation result and current state.
- A changed payload under the same UUID fails.
- Ended scenes never resurrect.
- Scene settlement and reward writes occur once, including after backend restart.
- Positive XP responses must have nondecreasing revision and totals before Unreal
  accepts them.
- Backend outages preserve confirmed UI totals. Transient errors retry with bounded
  delay; permanent client errors block the affected queue and surface degraded state.
- The runtime queue is ordered but memory-only, so unsent metadata is not guaranteed
  across game-server crash.

## 16. Known Prototype Limits

This is a trusted loopback prototype, not a production social economy.

- Development profile names and the shared game-server token are impersonable.
- There is no production login, principal/character ownership proof, TLS deployment,
  server attestation, or community-host threat model.
- The backend trusts the authoritative game server's word counts, memberships, and
  cooldown validation.
- Word/reply shape resists idle farming but does not measure semantic quality,
  consequence, originality beyond a short duplicate window, or good roleplay.
- There is no moderation freeze/reversal flow, dispute adjustment, or live balance
  dashboard.
- One account currently owns exactly one character. Alternate-character anti-farm
  behavior is therefore not a complete production implementation.
- The social outbound queue is not crash-durable.
- Social Level is arithmetic display; no full level-by-level unlock table exists.
- Earned tiers do not yet enforce character creation or ability entitlement.
- Authored narrative retention, receipt pruning, encrypted backups, deletion-aware
  restore, and shared-author disputes need production policy.
- Projects have no physical-world verification or effects.
- The shadow evaluator is tested but not connected to live evidence collection,
  reports, catalog grants, or payout.
- Rich source-attributed NPC memory, revocation-aware retrieval, and disclosure
  policy remain future work.

## 17. Recommended Porting Contract for Another Project

Preserve these separations even if names, engines, or storage change:

1. **Authoritative evidence:** only a trusted game server can assert actor,
   membership, audience, reply relationship, action validity, and occurrence time.
2. **Eligibility:** derive qualification from immutable metadata. Do not accept a
   client `eligible`, `quality`, `recipients`, or `xp` field.
3. **Payout:** calculate rewards on the backend from versioned configuration and
   rolling history. Clamp and ledger in the same transaction.
4. **Materialized state:** keep a fast current XP/tier snapshot, but make every
   positive mutation traceable to a unique ledger source.
5. **Zero-value receipts:** persist capped/decayed attempts so they cannot be
   replayed after the cap window.
6. **Idempotency:** one immutable request UUID and canonical payload fingerprint;
   exact retry returns the original result, changed retry conflicts.
7. **Narrative separation:** store prose in an ACL/revision service, never a reward
   ledger or generic analytics payload.
8. **Consent separation:** rewarded-Story promotion votes, recap publication
   consent, discovery opt-in, and reward-analysis consent are different permissions.
9. **AI separation:** AI may generate dialogue or summarize within approved policy,
   but deterministic code validates every consequential state change.
10. **Shadow first:** evaluate new signals without mutation, compare fairness and
    abuse patterns, then make a separate explicit decision before live payout.

For a production version, add durable server outbox/acknowledgements, real identity
and character ownership, versioned policy migrations, moderation reversals, finite
retention/deletion jobs, cross-server ordering, and explicit observability for every
eligibility and payout decision.

## 18. API Surface at a Glance

| Endpoint | Role |
| --- | --- |
| `POST /v1/profiles/init` | Initialize or read the account's one prototype character and progression snapshot |
| `GET /v1/profiles/<account>` | Read XP, computed levels, revision, character data, tier, and unlock flags |
| `POST /v1/social/command` | Current strict reward-bearing scene, speech, Star, Story, voice-mode, pacing, and block command lane |
| `GET /v1/social/state/<account>` | Read bounded owner social sessions, Stories, counters, diversity, blocks, and tier requirements |
| `POST /v1/social/admin/command` | Separate trusted-operator clan/event scope provisioning |
| `POST /v1/activities` | Legacy general activities; chat/emote are acknowledged at zero XP |
| `POST /v1/social/session-event` | Legacy metadata-only interaction compatibility, not a payout lane |
| `POST /v1/npc/conversation-summary` | Idempotently update character/NPC relationship and conversation summary |
| `GET /v1/npc/relationships/<account>/<npc>` | Read one character/NPC relationship projection |
| `GET /v2/capabilities` | Advertise local roleplay storage/projects and `reward_mode: OFF` |
| `POST /v2/roleplay/command` | Authored profile, record, listing, obligation, and project command lane |
| `GET /v2/roleplay/state/<account>` | Read authorized narrative and project projections |

All endpoints currently require the trusted local bearer token. The social admin
route requires a separate provisioning token that the game server and clients do
not receive. This is transport separation, not production authentication.

## 19. Verification Snapshot

The consolidated implementation was last verified with:

- 202 passing Python backend tests across base progression, social sessions,
  Stories, Stars, authored roleplay, projects, blocks, HTTP, concurrency, and the
  pure shadow evaluator;
- 87 passing Unreal native automation tests, including routing, social runtime,
  chat, NPC, and panel behavior;
- a rendered Slate panel test at 320x280, 420x280, and 600x500;
- an actual dedicated-server plus three-client privacy test in which sender and
  intended member received one private action after replay, the outsider received
  none, private NPC presentation stayed sender-only, and XP remained unchanged;
- a live configured-provider NPC smoke test confirming generated text, pending
  typing dots, reply display, and dot cleanup.

These checks establish the local prototype behavior described here. They do not
establish production authentication, public-host privacy, economic balance,
moderation readiness, or a fair live rollout of the shadow candidate.

## 20. Current Source Map

The implementation summarized here is primarily located in:

- `backend/social.py`: reward-bearing sessions, qualification, settlement, Stars,
  Stories, caps, peers, tiers, and social state projection.
- `backend/server.py`: profile/level materialization, progression ledger, legacy
  activity/session compatibility, NPC relationships, HTTP routes, and SQLite store.
- `backend/roleplay.py`: profiles, journals, recaps, listings, obligations, ACLs,
  revisions, receipts, and deletion tombstones.
- `backend/roleplay_projects.py`: cooperative project metadata and evidence.
- `backend/roleplay_rewards.py`: pure OFF/SHADOW candidate evaluator.
- `Source/RATWGame/RATWSocialSessionSubsystem.*`: authoritative runtime scenes,
  routing, word/reply evidence, pacing, presence, blocks, and backend command queueing.
- `Source/RATWGame/RATWBackendSubsystem.*`: ordered HTTP bridge, retry, response
  validation, cached account state, and owner-only social projection.
- `Source/RATWGame/RATWPlayerState.*`: replicated XP, level calculation, tier flags,
  social state/status, and account/character identity.
- `Source/RATWGame/SRATWSocialPanel.*`: Sessions / Stories player interface.
- `Source/RATWGame/SRATWRoleplayPanel.*`: authored profile/journal/recap/listing UI.
- `Source/RATWGame/SRATWProjectPanel.*`: cooperative project UI.
- `backend/test_social.py`, `backend/test_roleplay*.py`, and
  `Source/RATWGame/Tests/RATWSocial*`: executable behavior and boundary fixtures.

## 21. Non-Negotiable Invariants

For reuse, the highest-value invariants are:

- Chat delivery and progression settlement are related but not the same event.
- A successful send does not promise XP.
- A qualified session does not promise positive XP after caps.
- XP alone does not promise an earned social tier.
- A narrative record does not promise Story status, canon, or reward.
- A Story vote does not grant consent to store or publish player prose.
- Recognition is binary and bounded; it is not a popularity score.
- Private speech carries reduced reward weight but never weaker delivery privacy.
- Presence and accessibility features never become reward requirements.
- No AI model is the authority for identity, audience, eligibility, payout, role,
  mission completion, item grants, or canon.
