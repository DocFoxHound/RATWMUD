# Living NPCs, memory and party conversation

Status: implemented vertical slice with authored offline dialogue fallback.

## Population and movement

The initial population is Rowan (keeper), Bracken (scout), Moss (cook), Flint
(porter), Ash (smith) and Vale (scribe). The world core stores them as simulation
data and advances home/work/social schedule destinations. Movement uses the same
navigation and doors as players. NPCs continue their schedules independently of
the local view. This slice has six residents rather than a production population
LOD scheduler. The original short demonstration schedule is superseded by the
deterministic needs/jobs/economy described in `15-npc-society-economy.md`.
The LLM receives current activity, needs, purse/stock and authoritative age as
context only. Its words cannot transfer goods or decide simulation actions.

Bracken alone can be recruited in the slice. The invitation appears at close
range and is validated by the server. Recruitment suspends his normal schedule;
he follows the leader through local paths and explicit/edge cell transitions.
He answers when named or explicitly invited. His curious personality permits a
road/danger-topic interjection after a 45-second quiet interval. He does not reply
to every party line. Nearby ambient remarks use a separate cooldown.

## Two memory layers

The confirmed consolidation interval is **one hour since the last interaction**.
Each accepted interaction resets the timer. The NPC retains full detail for the
most recent 32 turns of an active NPC/character conversation; older turns are
compressed into attributed short excerpts with their event IDs. Consolidation
produces a permanent extractive summary, removes active detail and keeps all
source-event references. Every turn contributes to the summary; early commitments
must survive a long exchange.

The offline summarizer is deterministic and explicitly described as extractive.
It summarizes what participants said; it does not certify the truth of their
claims. For example, “Ash said they would return the scarf” does not set a quest
completion or inventory transfer. Future authoritative promises, debts and quest
state need dedicated records in addition to conversation summaries.

Active memory stores conversation ID, NPC ID, subject ID, start time, last
activity, bounded detailed turns, compressed older context and source IDs. Long-term
memory stores the same conversation ID, subject links, summary, start and
consolidation timestamps, and source IDs. Duplicate source events within the
active window do not duplicate turns. Consolidation checks the conversation ID
before creating a summary and then removes that active conversation atomically
in the persisted world checkpoint.

Timers use UTC seconds. An hour spent offline therefore counts as inactivity;
startup consolidates any due conversations. Summaries persist permanently and do
not decay, and repeated consolidation/restart cannot create a second summary for
the same conversation. Direct later experiences can create new summaries.

## Perception and retrieval

NPC input passes through the same hearing and visual perception rules as player
input. A distant or obstructed utterance can contain `...`; an unseen action can
contain `···`. The language provider never receives the source's hidden words.
The deterministic recall path selects recent subject-specific active context or
the most recent matching permanent summary; the adapter caps context sent to a
provider. Rich relevance ranking and contradictory-belief handling remain future
work.

NPC replies are normal spatial roleplay posts. A generated answer cannot bypass
hearing, visibility, order, map-marker duration or the narrative queue. A pending
generation request suppresses another overlapping request for that NPC, and the
provider's completion path is guarded against duplicate callbacks.

## Tests and morning questions

Portable tests cover 3,599 versus 3,600 seconds of inactivity, reset by new
activity, consolidation replay, 80-turn source preservation, and an early promise
remaining in the permanent summary. Integration tests exercise memory persistence
through an actual server restart.

Open: how compact long-term summaries should be, whether NPC recollection is
subjective, what explicit promises merit authoritative records, whether a
disconnected leader's companion returns to work, and recruitment release/transfer
rules. The current companion waits for its leader after disconnect.
