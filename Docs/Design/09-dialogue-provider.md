# Dialogue provider and authored fallback

Status: working offline dialogue; optional local HTTP adapter. No external model
service or paid account was used for this build.

## Provider contract

`FRatwDialogueProvider` accepts an NPC's identity, description, current activity,
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

The adapter sends a JSON POST with `npc`, `description`, `activity`, `heard`,
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
providers. No remote provider or redirect was used in this build; it does not
select/download a model or add credentials.
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

The provider label distinguishes authored and configured local generation. The
player is never told that a scripted response is model-generated. NPC reply and
memory persistence are server operations regardless of provider choice.

## Validation and future work

Offline authored responses are exercised by the native NPC interaction scenario.
The adapter compiles with Unreal HTTP support. Native automation includes a
loopback fixture for success, HTTP failure, malformed/empty/oversized responses,
and an eight-second timeout fallback, plus URL-validation regressions. The fixture
never redirects or contacts another service. A real local model must be chosen
and evaluated before claims about generative quality can be made; consult the
root test report for the executed automation results.

Open: preferred local or hosted model, operating cost ceiling, response latency,
memory-context limits, prose constraints and evaluation set, NPC voice authoring,
and whether generated dialogue should visibly carry a provider indicator.
