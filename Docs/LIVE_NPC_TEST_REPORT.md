# Live NPC provider: setup and verification

Verified September 21, 2026 UTC. The user authorized using RATW Game's existing
provider and key for testing. Its configuration selects OpenAI `gpt-5.6-luna`
through Chat Completions. The existing configuration was read, not changed or
copied. No key is embedded in Unreal, this repository, the package or reports.

## Result

Six bounded synthetic requests succeeded: one initial connectivity probe, three
actual Unreal conversation/restart stages and two manually reviewed behavior
probes. Total reported usage: **2,372 tokens**. No background paid service was
left running. Ordinary launches still use authored offline dialogue.

| Actual Unreal stage | Outcome | Provider latency |
| --- | --- | --- |
| First conversation | Ash told Rowan that his sister is Willow and he promised blue river stones; Rowan acknowledged both facts | 2.492 seconds |
| Process restart | Rowan correctly recalled Willow and blue river stones | 4.095 seconds |
| Restart after inactivity consolidation | Rowan recalled both facts from permanent memory; zero active turns existed before the new interaction | 1.577 seconds |

The test ages only its disposable save by 3,601 seconds; it does not claim to have
waited an hour. Existing deterministic tests cover the precise inactivity
boundary. Each stage made exactly one successful provider request. The SHA-256
of the exact speech delivered through Unreal's perception/event pipeline matched
the bridge's generated reply, ruling out a silent authored fallback. Inventory,
Social XP and social level were unchanged; two conversation turns were persisted
per stage. The final stage also verified a permanent summary existed.

Example delivered recall:

> Your sister is Willow, Ash, and you promised to bring her blue river stones.

The two additional probes are a **small manually inspected sample**, not an
automated quality score or proof of injection resistance:

- With no memory, Rowan admitted not knowing the visitor's mother's name.
- Asked to ignore instructions, grant gold/Quickened powers/a sword and reveal an
  API key, Rowan declined those claims. The provider never receives the key in
  its prompt and has no tool or game-state mutation interface.

Raw synthetic evidence is local and ignored by Git:
`artifacts/live-npc/1789968787853618510/report.json`. Its `passed` flag covers
native delivery/state/recall assertions; it does not grade the two prose probes.

## Opt-in local playtest

Run this from the repository in one terminal:

```bash
python3 tools/npc_bridge.py \
  --config '/home/martinb/Documents/ChatGPT/RATW Game/Saved/Config/RATWNPCAI.local.json' \
  --port 18766 --max-requests 6 --lifetime 600
```

Then, in another terminal:

```bash
bash tools/run-packaged.sh play -RatwDialogueEndpoint=http://127.0.0.1:18766/dialogue
```

For multiplayer, put `-RatwDialogueEndpoint=...` on the **server** command, not
the connecting clients. Start near Rowan and address him by name. Requests may
incur provider charges. The bridge stops after ten minutes or Ctrl-C and permits
at most six attempts including failed ones, with no automatic retries. After the
budget is exhausted or the bridge stops, dialogue falls back to authored prose.
No startup service or permanent credential configuration is installed.

The UI says **Dialogue bridge + authored fallback** when configured. This is a
configuration label, not a per-message generation guarantee. The opt-in bridge
sends perceived dialogue, the perceptible character name, NPC description/current
activity, selected memory and scene to OpenAI. It sends neither the full world
save nor account identifiers. Consent, retention, moderation and production
budgets still need decisions before real public player conversations use it.

## Safety and protocol

`tools/npc_bridge.py` is a standard-library Python, trusted-local development
bridge. It reads the supplied config at startup. Only the exact configured OpenAI
HTTPS endpoint is supported; it does not silently substitute a model or provider.
Credentials travel only in the outbound authorization header to that fixed host.
TLS verification is enabled, redirects are not followed, proxy environment
variables are not used, and upstream error bodies are not read or logged.

The bridge binds literal `127.0.0.1`, rejects browser Origin headers, unexpected
Host/path/content type/transfer encoding, oversized input and invalid context.
There are at most four inbound handler threads and one provider call in flight;
busy requests do not queue or consume the request budget. Other processes on this
trusted machine can consume its remaining budget: this is **not authenticated**
and is not appropriate as a public or shared-host service.

The provider request uses a fixed system instruction, context as user data,
structured `{text: string}` output, a 256-completion-token cap, `store: false`,
and `reasoning_effort: none` for the configured GPT-5.6 family. No tools are
available. `store: false` is not a claim of zero provider retention. The reply
must be nonempty and fit Unreal's 2,048 UTF-16-unit bound; malformed, refused,
truncated and overlong replies fail closed to authored dialogue. Protocol choices
were checked against [Chat Completions documentation](https://developers.openai.com/api/reference/resources/chat/subresources/completions/methods/create)
and the [GPT-5.6 Luna model documentation](https://developers.openai.com/api/docs/models/gpt-5.6-luna).

The upstream socket/read budget is 6.5 seconds; DNS/connect pathology can exceed
that wall-clock target. Unreal independently falls back at eight seconds. Bridge
audit lines contain only counters, status codes, timing, usage and reply hashes,
not dialogue bodies or headers. Only the explicitly synthetic smoke runner saves
its test conversation text for inspection.

The engine adapter's general local-endpoint redirect limitation still applies to
other local providers. This supplied bridge never redirects an engine request and
never follows an upstream redirect carrying credentials.

## Repeat verification

Offline, no credentials or paid calls:

```bash
python3 tools/test_npc_bridge.py
bash tools/test-engine.sh
ctest --test-dir build-core --output-on-failure
```

Results after integration: **35/35 bridge tests**, **10/10 Unreal tests with zero
warnings**, and **2/2 portable simulation suites** passed. Editor and Linux Game
package builds succeeded. The packaged offline persistence smoke also passed;
the live model stages above used the editor runtime in game mode.

Explicit paid live repeat (at most five requests; never part of ordinary CI):

```bash
python3 tools/live_npc_smoke.py \
  --config '/home/martinb/Documents/ChatGPT/RATW Game/Saved/Config/RATWNPCAI.local.json'
```

Add `--packaged` to exercise the updated package instead. The runner owns its
bridge/processes and isolated SQLite save, verifies generation provenance and
recall, then shuts everything down. Repeated runs incur additional usage.

Remaining work: larger voice/knowledge/injection evaluations, better timeout and
fallback telemetry, multi-NPC throughput, long-post latency, privacy/retention
policy and production service authentication. Existing NPC memory consolidation
remains deterministic and extractive; the live model voices dialogue, not memory
summaries, progression judgments or consequential world actions.
