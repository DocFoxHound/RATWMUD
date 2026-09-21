# Narrative, composer, and presence

Status: implemented native Slate client presentation with authoritative server parsing and perception.

## Conversation flow

The narrative side contains the cell's authoritative description and observer-specific posts. Each post stays in one entry even when its content mixes speech and actions. The client receives permitted text only. It never reconstructs missing words, exposes a hidden speaker's identity, or resolves slash commands itself.

The IC channel reveals one post at a time. The next entry waits until the first has finished. Preferences offer 64 characters/second, 120 characters/second, or instant reveal. Reduced motion also makes text instant. These preferences never delay state changes or the map speaking cue. Local OOC is a separate transcript and appears immediately.

The prototype displays the assembled post in its speaker's color, preserving server-produced action and speech text. Segment-level italic styling and independent narration tones are future visual refinements. `...` and `···` remain the server's distinct missing-hearing and missing-action markers.

## Composer state

The multiline Slate editor starts read-only in navigation mode. Enter activates it. Shift-Enter inserts a newline; unmodified Enter submits, clears the live editing area, and returns to navigation. Escape leaves the editor without changing its text. Movement inputs are cleared when editing begins and ignored while editing.

Submitted text is retained in a pending-draft map under a client request ID until a `chatAccepted` event arrives. A matching rejection restores it if the current draft is empty. If the player has already begun another draft, the failed text remains recoverable through a button that appends it, preserving the newer writing. Server validation remains authoritative.

Drafts are local memory only in this slice. They survive mode changes and rejected submissions but not a client-process crash; disk draft recovery and reconnect resubmission require future design. A pending request without an acknowledgment is not automatically retried because that could duplicate a post.

## Typing and speaking

Typing status sends only an active boolean, at most once each second. Activity expires three seconds after the last edit, and stops on send, escape, or channel change. OOC produces no typing presence. A currently visible entity's `typing` or `speaking` snapshot fields drive its colored map indicator. Full text never appears on the map.

The typing mark is `...` in a small outlined bubble. A brief quotation-like speaking mark fades over four seconds. These are independent of the transcript queue. The 32-color palette provides semantic IDs shared with the server, while Self/Player/NPC map-token colors remain separate.

## Test obligations

The Slate automation suite verifies key handling, real multiline newline insertion, draft preservation, rejected-post recovery, movement suppression, and sequential IC reveal. Integration must verify that a second client sees only boolean typing presence and that spatial speech degradation occurs before delivery. Anonymous speech should retain its color but contain no hidden name or stable identity in the payload.

Stress review should include a long multiline post, multiple queued speakers, switching to OOC while IC reveals, a rejected slash command, a typing timeout, and a second draft started before rejection of the first.
