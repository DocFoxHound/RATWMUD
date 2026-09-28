# Dialogue provider and authored fallback

Status: working offline dialogue and optional local HTTP adapter. A follow-up
authorized test now connects RATW Game's existing OpenAI `gpt-5.6-luna` provider
through a bounded server-side bridge. See [setup and live evidence](../LIVE_NPC_TEST_REPORT.md).
Ordinary launches still make no remote model calls.

## Provider contract

The NPC Mind client (`ratw::mind::Client`, `Source/RATWMUD/Core/RatwMind.h`) accepts an NPC's identity, description, current activity,
the perceived player message, a bounded subject-specific memory excerpt and scene
description. It returns one spoken reply through a callback. The server wraps that
reply as a speech segment and routes it through normal perception. The callback
has no inventory, movement, XP, quest or persistence mutation interface.

Without configuration, the UI reports **Authored offline dialogue**. Deterministic
responses cover greetings, memory recall, promises, rain, Chapters and the
companion's conversational voice. This keeps the entire slice usable without
pretending that a model was called.

An explicitly configured loopback endpoint can enable generation:

```text
-RatwDialogueEndpoint=http://127.0.0.1:8080/dialogue
```

The adapter sends a JSON POST with `npc`, `player`, `description`, `activity`, `heard`,
`memory`, `scene` and `instruction`. A compatible local service returns:

```json
{"text":"A reply that this NPC could say from the supplied knowledge."}
```

Only strictly parsed loopback HTTP endpoints with an explicit numeric port are
accepted as the initial destination. Credentials, backslashes, control characters,
host-prefix tricks and invalid ports are rejected. `localhost` is normalized to
the literal `127.0.0.1` address. The endpoint must be a trusted local service.
The installed Unreal HTTP transport automatically follows redirects and exposes
no supported per-request redirect veto, so this adapter cannot guarantee that a
misbehaving local provider will keep subsequent requests on the machine. Replacing
or restricting that transport is a deployment gate before allowing untrusted
providers. The supplied `tools/npc_bridge.py` never redirects a local request,
and its outbound credential-bearing HTTPS request has a fixed host and cannot
follow redirects. It reads the user-authorized existing config without placing
credentials in Unreal, source, logs or the package. This is an explicit testing
option, not an automatic provider selection or startup service.
There is an eight-second timeout, a 16-KB response bound, a 2,048-character reply
bound, empty/malformed-result fallback and a single-completion guard. A failed
provider returns the authored response without stalling movement or other players.

## Authority and knowledge

The prompt describes the quadrupedal NPC and explicitly treats player dialogue as
untrusted conversational content. The model sees only perceived text and selected
memory. The output is prose; even if it claims to grant an item, it has no route
to do so. Production quality needs evaluation for unsupported narrative claims,
but the deterministic state boundary already prevents a claim from becoming a
gameplay grant.

The provider label distinguishes authored mode from a configured dialogue bridge
with authored fallback. It is not a per-message provenance indicator, and does
not imply that configured generation runs on-device. The `player` field contains
only the identity visible to the NPC (or "traveler"); no account ID is sent. NPC
reply and memory persistence remain server operations regardless of provider.

## Validation and future work

Offline authored responses are exercised by the native NPC interaction scenario.
The client is portable C++ shared by both servers. `Tests/server_parts_tests.cpp`
runs it against a loopback stand-in (a good reply, a malformed one, an unreachable
Mind) and checks the URL-validation regressions. The stand-in never redirects or
contacts another service. Separate live tests verify generated
delivery by matching response hashes and recall across restarts/consolidation.
The bridge answers up to three conversations at once by default (`--concurrency`,
1–4); beyond that a request is refused as busy and the game uses its authored reply.
Its request budget and lifetime limits are unchanged. The bridge has 38 offline
tests for credential boundaries, protocol validation, request budgets, concurrency
and failures. The real-model sample is deliberately
small; broader quality/adversarial evaluation is still needed.

Open: production hosting/model policy, operating cost ceiling, response latency,
memory-context limits, prose constraints and evaluation set, NPC voice authoring,
and whether generated dialogue should visibly carry a provider indicator.

## The NPC Mind (2026-09-27)

`tools/npc_mind.py` is the conversation service for real play. The bridge above remains for provider tests. The Mind:
- takes the same request plus the speaker's ID, the relationship and the NPC's mood
- can read the pair's history from the event log
- returns a structured reply: text, emotion, liking and trust nudges, a note, a promise
- summarises finished conversations

The game server applies all of it on bounded terms. See `26-living-npcs.md`, Phase 3.

**Live by default (2026-09-27).** The launch scripts start the Mind themselves: `tools/server.sh`, `tools/play.sh`,
`tools/live.sh` (through them) and `tools/run-packaged.sh`. The game then uses it (`tools/mind.sh`).
- **Key and model** come from `Saved/Config/RATWNPCAI.local.json`: gitignored (also `*.local.json`) and readable only
  by its owner. `RATW_AI_CONFIG` can point elsewhere.
- **History.** A game on `-RatwDatabase=dev|prod` gives the Mind that database's event log for NPCs' history.
- **Switches.** `RATW_AI=off` plays with authored lines; `RATW_AI=fixture` uses offline replies.
- **Logging.** The Mind's log is `Saved/Logs/npc-mind.log`, with no dialogue in it.
- **Automated tests** (engine tests and smokes) start the engine directly and stay offline and deterministic;
  `tools/live_npc_smoke.py` is the one that uses the paid model.

