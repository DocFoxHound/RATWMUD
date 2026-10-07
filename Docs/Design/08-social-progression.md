# Social participation and progression

Status: thin session-based implementation; final reward governance is deferred.
The supplied `SOCIAL_PROGRESSION_ROLEPLAY_TRACKING.md` is the source model.

## Boundaries

Ordinary chat, emotes, typing, proximity, OOC and NPC conversation award no XP.
Accepted human roleplay can provide metadata evidence for a scene. XP is committed
only when an eligible scene ends. AI neither judges the prose nor chooses an XP
amount. The ledger contains actor/partner IDs, event/session IDs, time, reason and
amount; it never contains the roleplay text.

*(Since doc 49, 2026-10-06: accounts own up to six characters, and social standing is the account's: the social XP of
all its characters, from scenes, stars and Stories alone. Only a development identity's wolf stands on its own. Doc
44's other XP became practice for skills, and Gifted and Quickened are earned per account (doc 49, Phase 5).)* The
local MVP had one character per development identity, so its social identity
mapped one-to-one onto that character. Displayed level follows doc 44's curve (level L to L + 1 costs 100 + 50 × (L − 1) XP); doc 44 also adds XP from work, practice, places and contracts, a daily cap of 150 and rested XP. Normal characters remain
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

**Fights are scenes** (doc 33). Each fight has its own scene from its start, with every player fighting in it a member
whether they talk or not; fighters' words go to it (tagged as a party's are), not to the cell's scene, and leaving with
the scene-end action doesn't end it: the fight's end does. Settling it pays each player who took two turns or more 10 XP
for the fight, and those who talked it through (the qualifying shape above, with at least one other) twice the scene
pay on top (40 for contributors 1–4). Repeated partners decay it as above (counted over scenes both were paid in
today), and the 8-scene and 100 XP daily limits apply. The receipt is the ordinary session receipt, so stars and Stories
treat a fight as any scene.

**Gold Stars: one to each.** A participant may give a Gold Star to each other paid participant of an ended scene, one
per recipient (as the source model has it), not one per scene. A fight's result card offers them as its roleplay
review, as does the bar above the composer for an hour after any scene.

Gold Stars, Stories, Story Stars, consent-reviewed recaps, tier diversity gates and
the shadow evaluator are explicitly deferred. They must become separate typed
metadata paths rather than generic “give social XP” commands.

## The scene line (built 2026-10-04)

The story column shows every scene the wolf is in (`Client/src/ui/hud/story.ts`; the snapshot's `self.social.scenes`,
from `Game::refreshSocialViews`), a party's or a fight's beside the room's:

- **Who and how far:** "PARTY SCENE with Bo · 2 turns".
- **What pay still needs**, from the server's own counts, never guessed by the page: "To be paid: 1 more line of five
  words or more · 12 more words · answer someone"; then "On track to be paid", or "You've said enough: waiting for
  another to say as much" while no one else has. A fight's scene says "To be paid twice" (talking it through).
- **Where one's next words count**, with two scenes: "← YOUR WORDS GO HERE", by the server's own rule (a fight's scene
  while fighting; the party's while a party mate is in the place; else the room's). There is no choosing: the rule
  decides, and the line says which.
- **Quiet warnings:** after 15 minutes without a line the scene turns amber: "Quiet · ends in 14 min unless someone
  speaks" (it ends at 30, pacing v2 of the source model). With five minutes left, one toast says so. Nothing else
  interrupts.
- **LEAVE** steps out of a scene (social verb `leave`, `SocialLedger::leave`), after a confirm. The leaver is settled
  at once, paid if they have the shape and another member has it too; the others carry on. Their part still counts
  toward the scene's two qualified members, they aren't paid again when it ends, and their later words there count
  for nothing (`Contribution::left`, saved). A fight's scene can't be left: it ends with the fight. The old
  `session_end` action ends the scene for everyone; its "End scene" button left the actions row (2026-10-04), so one
  player can't cut a scene short for the rest. It stays for scripts.

Tests: `social_game_tests` `leavingAScene` and the snapshot's scenes; `checkpoint_tests` (who left survives a
restart); `social.test.ts` (the words); `tools/client/scenes.mjs` (the real page: needs, on track, two scenes and a quiet
one, LEAVE; screenshots in `artifacts/screenshots/scenes/`).

## Verification and unresolved choices

Tests prove that direct speech gives zero XP, A–B–A creates a scene without an
award, two qualified contributors receive a settled receipt, settlement replay
cannot duplicate XP, repeated pairs decay, OOC is excluded, duplicates are
suppressed and solo activity cannot qualify.

Open (the scene line, quiet warnings and showing which of two scenes a line counts toward were built 2026-10-04, above): public
versus private speech weighting, account migration, moderation and appeal tooling,
and owner-approved final tier thresholds. Reward numbers here implement the source
prototype baseline for evaluation, not final balance.
