# Social participation and progression

Status: thin session-based implementation; final reward governance is deferred.
The supplied `SOCIAL_PROGRESSION_ROLEPLAY_TRACKING.md` is the source model.

## Boundaries

Ordinary chat, emotes, typing, proximity, OOC and NPC conversation award no XP.
Accepted human roleplay can provide metadata evidence for a scene. XP is committed
only when an eligible scene ends. AI neither judges the prose nor chooses an XP
amount. The ledger contains actor/partner IDs, event/session IDs, time, reason and
amount; it never contains the roleplay text.

The local MVP has one character per development identity, so its social identity
maps one-to-one onto that character. Production accounts must own progression
across multiple characters; this prototype does not claim to implement that account
service. Displayed social level is `1 + floor(XP / 100)`. Normal characters remain
Normal regardless of this number: Gifted/Quickened eligibility and governance
require later systems.

## Evidence and scenes

The runtime counts speech and ordinary narration as roleplay evidence. Slash
actions, including `/action` and `/pose`, persistent `/me` state/posture changes
and OOC cannot inflate that counter, matching the supplied prototype's conservative
slash-command policy. Whether action-only roleplay should qualify is a morning
design question. The server supplies actual perceived human
listeners. The ledger accepts at most one evidence event per actor per two seconds,
suppresses a repeated normalized-content hash for 60 seconds, rejects duplicate
event IDs, and accepts 1–500 words of evidence per event. Longer valid prose is
preserved in full; its contribution count is capped at 500 rather than losing
eligibility or awarding more for extra length. Content hashes are temporary evidence metadata,
not retained transcripts.

An alternating A–B–A exchange within 30 seconds creates an automatic current-cell
scene. Candidate turns retain their original recipients; later visibility never
retroactively makes a past turn heard. A reply requires its author to have actually
perceived the previous turn, with another current human participant present.
This thin slice groups one automatic scene per cell; separating concurrent
conversations within a cell is a later scene-membership refinement. Passive
listeners do not become participants. A meaningful turn has at
least five words; a reply requires another participating human's meaningful turn
within 180 seconds. Counters cap at 200 turns and 10,000 words per participant.
Short accepted lines can keep a scene active without becoming meaningful turns.

The slice supports a scene-end action and automatic settlement after 30 minutes
without accepted activity. Advanced manual/private scene membership, pause,
resume, invitations, audience revisions and reconnect reservations remain outside
this thin implementation.

## Settlement

At least two participants must each have two meaningful turns, 35 meaningful words
and one valid reply. Each qualified participant starts at 20 XP for the public
scene. Contributor positions 5–8 use 60% of that amount and positions 9+ use 25%.
The most repeated qualified partner over the previous rolling 24 hours applies
successive multipliers of 1, 1/2, 1/4, 1/8 and then zero. No more than eight positive
session rewards or 100 XP can be paid per actor in a rolling 24-hour window.

Ending a scene marks it ended before producing receipts, including zero payouts.
Repeating that end request cannot settle it again. Scene state, ledger receipts
and materialized totals are persisted in the same SQLite world transaction.
The client renders only confirmed totals from snapshots.

Gold Stars, Stories, Story Stars, consent-reviewed recaps, tier diversity gates and
the shadow evaluator are explicitly deferred. They must become separate typed
metadata paths rather than generic “give social XP” commands.

## Verification and unresolved choices

Tests prove that direct speech gives zero XP, A–B–A creates a scene without an
award, two qualified contributors receive a settled receipt, settlement replay
cannot duplicate XP, repeated pairs decay, OOC is excluded, duplicates are
suppressed and solo activity cannot qualify.

Open: explicit UI for scenes and quiet warnings, multi-scene selection, public
versus private speech weighting, account migration, moderation and appeal tooling,
and owner-approved final tier thresholds. Reward numbers here implement the source
prototype baseline for evaluation, not final balance.
