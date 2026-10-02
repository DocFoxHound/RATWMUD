# 28. Cheaper NPC voices: the game answers what it can, a small model the rest

Planned 2026-09-30; built 2026-10-01 (see "Built", at the end). The aim: much less spent on language models, with NPCs that still sound like
themselves. The principle is the one the rest of the game follows: **rules decide, language performs**, and a model is
used only where something new is being said.

## Where the money goes today

- **Player conversations** (`/dialogue`): every line a player says to an NPC is one call to the configured model
  (`gpt-5.6-luna`). The live smoke measured about 450–520 input tokens and 30–50 output tokens a reply.
- **Overheard exchanges** (`/exchange`, Phase 10): up to 30 calls an hour across the world.
- **Conversation summaries** (`/summarize`): one when a conversation closes after an hour's quiet.
- **Life stories** (Dungeon Master): rare, written once and kept.
- **Already cheap:** written lines when the Mind is off or over budget; stories kept; persona first in every request,
  so the provider can cache it.

## The plan, in five steps

Each step is useful by itself, and each is measured before the next.

### 1. Measure: a cost ledger (first, so every later step shows its saving)

- The Mind already logs each call's outcome and tokens (no dialogue). Add the **route** that served each NPC line (the
  game itself, a written line, the small model, the main model) and the **kind** (dialogue, exchange, summary, story).
- Prices per model in the Mind's config (input, cached input and output, per million tokens), entered by the operator.
  They change too often to put in code.
- `python3 tools/ai_cost.py` reads the Mind's log: calls, tokens and money by day, kind, route and model; the share of
  lines the game answered itself; cached-token share. The Dungeon Master can show the same later.

### 2. Two models: the main voice and a small one

- The config names two: `"models": {"voice": "gpt-5.6-luna", "light": "<the provider's small tier>"}`. The light one
  takes:
  - overheard exchanges (short, low stakes);
  - conversation summaries;
  - polishing the game's own answers into the NPC's voice (step 3);
  - the exchange library (step 4), written in bulk.
- The main voice keeps conversations with players, where quality shows most, and life stories (rare, kept).
- Fallback order for every kind: light model, then written lines (for player dialogue: main, then light, then written
  lines). A model that fails or is over budget never stops an answer.
- Which small model: chosen by the operator in the config. The plan doesn't depend on one; step 5's review shows
  whether it holds up.

### 3. The game answers what it knows (a speech router)

Before a player's words go to a model, the game checks whether it can answer them itself, truthfully, from the world:

| They ask about | Answered from |
|---|---|
| greetings, thanks, goodbyes | the NPC's greeting, mood, and how well they know the player |
| prices, stock, "what do you sell" | the merchant's real stock and prices (`Society::quote`) |
| opening hours, the day | the merchant's hours, Marketday and Restday, festivals |
| where someone or somewhere is | the NPC's home, work and known places; a direction from where they stand |
| who they are, what they do | name, job, apprenticeship, and their own chronicle's milestones |
| the watch: am I wanted, what for | warrants and charges (guards only) |
| something just asked | the NPC's last answer to this player, if nothing has changed since |

- **Only clear cases.** The router answers only when the whole of what was said is one recognised request (short, a
  single intent, nothing else in it). Anything else, anything emotional or odd, any roleplay, goes to the model as
  now. When unsure, it passes it on.
- **Many ways of saying each answer.** Written lines in several tones (warm, gruff, formal, sly, shy), the tone taken
  from the NPC's personality; their mood and how well they know the player choose among them; never the same line
  twice in a row to the same player.
- **Polished when it's cheap.** Optionally the small model rewrites the game's answer in the NPC's voice: a few output
  tokens, the facts fixed and checked afterwards (prices and names must survive).
- **Rules are data.** The patterns and lines live in a data file with a test corpus of player lines, each with the
  route it must take, so a change that sends too much to the game (or too little) fails a test.

### 4. An exchange library: the small model writes lines once, the game fills them in

- `python3 tools/ambient_library.py` asks the small model, in bulk (with the provider's batch discount where offered),
  for about twenty exchanges per combination: topic (gossip, news, quarrel, friends, day), how the two get on
  (friends, acquaintances, rivals) and tone. Each has blanks (`{teller}`, `{listener}`, `{subject}`, `{claim}`,
  `{news}`, `{day}`) and no names, places or facts of its own.
- Every one is checked (blanks only from the list, lengths, both speak, nothing invented) and kept in
  `Data/Ambient/library.json`, reviewed like other data.
- The director fills one in for most exchanges, choosing among those not heard lately in that place. It calls a model
  live only when it matters most: gossip about a player who is standing there, or the first talk of a new crime, a
  death or a marriage. At most about 10 live calls an hour.

### 5. Every call cheaper, and a check that it still sounds right

- **Shorter requests.** Send the most relevant memories and the last six dealings, not twelve; relationship and
  rumour lines only when they bear on what was said; tight output limits for each kind.
- **Prompt caching.** Keep each request's opening (rules, persona) unchanged from call to call so the provider
  caches it, and confirm the cached share in the ledger.
- **Budgets that bend.** Per player (say 60 model replies an hour) and for the world. As a budget runs low, the router
  answers more itself and the small model takes more of the dialogue, instead of NPCs falling silent.
- **Cost mode** in the config: *generous*, *balanced* (the default) or *frugal*, setting the budgets and how much goes
  to the small model.
- **A review before switching over.** A fixed set of sample conversations and exchanges, answered three ways (the game,
  the small model, the main model), shown side by side on a page for the operator to judge blind. It's run only when
  asked, since it makes paid calls; the offline tests use the fixture as now.

## What it should save

The saving depends on how players talk, so the ledger (step 1) measures it rather than guessing. The routine share of
MUD conversation (greetings, trade, directions) is large; the small model costs a fraction of the main one; library
exchanges cost nothing to play. Together that should cut the hourly spend substantially at the same player numbers,
with the main model kept for the conversations that are worth it.

## Order

1. The cost ledger (`tools/ai_cost.py`), so there is a baseline.
2. Two models in the config, routed by kind.
3. The speech router, its lines and its test corpus.
4. The exchange library and its writer.
5. Shorter requests, caching checks, budgets that bend, cost mode, and the review page.

## Choices for the operator

- Which small model, and the prices of both (in the config).
- The cost mode, and the per-player and world budgets.
- Whether the game's own answers are polished by the small model (better voice, a little cost) or used as written.

## Built (2026-10-01)

- **The ledger.** The game server writes `Saved/Logs/npc-voices.jsonl` (`--voice-log`): a line per NPC line said, with
  its kind (dialogue, exchange, summary) and route (`game`, `game+polish`, `library`, `written`, `model`), never the
  words. The NPC Mind appends each call to `Saved/Logs/npc-mind-calls.jsonl` (`--ledger`): kind, tier, model,
  outcome, tokens and cached tokens. `tools/ai_cost.py` reports by day: lines and how they were answered, the share
  answered without a model, calls and tokens by kind and model, and money where the config has prices.
- **Two models.** `light_model`, `cost_mode` and `polish` in the Mind's config (`npc_bridge.Config`). Player dialogue
  uses the main model until a speaker has had their hour's share (generous 120, balanced 60, frugal 20) or the world
  has (1200, 600, 200), then the small one; a quick failure of the main model (an HTTP error, a refusal, a reply that
  fails its checks) is answered by the small one. Exchanges, summaries and polishing use the small one; exchanges
  written live are capped by mode (20, 10, 4 an hour) and the game caps them at 10. Without a `light_model`, the main
  model does everything, as before.
- **The speech router** (`Core/RatwVoice.*`, `Core/RatwGameVoice.cpp`, `Data/Voice/router.json`). What a player says
  is normalised (lower case, contractions spelt out, punctuation, the NPC's name and "please" gone) and matched whole
  against each request's patterns, nine words at most. A match is answered from the world: the merchant's real prices
  and stock (`Society::quote`), their hours (with Restday and Marketday), name and work, where a resident works or
  lives (never where a player is) or which way a place lies, a guard's warrants and what settles them, the day.
  Recognised but with nothing true to say (an unknown place, hours for someone without a post), it goes to a model.
  The same thing asked again within ten minutes is answered "as I said". Lines come in six tones (taken from the
  personality's words), for strangers or those known (familiarity 25 and up), never the same line twice running; a
  first meeting may use the NPC's own greeting. The corpus (`Data/Voice/corpus.json`, 50 lines) is checked by
  `Tests/voice_tests.cpp`. With `polish`, the small model rewrites the answer and the Mind checks every number and
  name survived, else the game's words stand; the game stops asking for ten minutes when polishing is off.
- **The exchange library** (`Data/Voice/library.json`: 23 hand-written to start). The director plays one for most
  exchanges, filled in (news in the teller's own words), passing over those heard lately in that place; it writes one
  live only for gossip about a player there, the first talk of an incident, or news of a death or a crime, and falls
  back to the library when that fails. `tools/ambient_library.py` has the small model write more (one call per kind,
  band and tone), each checked: 2 to 4 lines, both speaking, only the listed blanks and those its kind needs, no
  names of its own, no duplicates.
- **Shorter requests.** Memory sent with a conversation is cut to 1600 characters (from 3600); history to the last six
  dealings (from twelve); dialogue output to 260 tokens (from 320).
- **The review.** `tools/ai_review.py` answers `Data/Voice/review_samples.json` three ways (the game via
  `build-core/voice_check`, the small model, the main model) and exchanges three ways (library, small, main), on one
  page in shuffled columns with the key at the end. Paid; run when wanted.
- **Not done:** the provider's batch discount for writing the library (it calls the model one combination at a
  time); a cost view in the Dungeon Master. No small model is chosen yet: the operator names one in the config.

## Choosing the small models (2026-10-01)

Reviewed with `tools/ai_review.py` against `gpt-5.6-luna` (the main voice), on the game's real dialogue context:

- **`gpt-5.4-nano`** spends no reasoning tokens with `reasoning_effort: none` and answers in about two seconds, but in
  conversation it loses track of who is who and invents events; its exchanges ramble. Kept for summaries and overheard
  exchanges (`light_model`).
- **`gpt-5.4-mini`** reads as naturally as the main voice, keeps to the game's ways (the trade menu, debts), and echoes
  the scene least. It invents small details as the main voice does (a dish, a rumour); the main voice was the more
  careful with facts. It answers players once the main voice's share is spent (`fallback_model`, a third setting).
- The rules now tell every model the scene is background, not something to describe; it worked best on the smaller
  models (4 mentions of the hearth and juniper in 17 answers for mini, against 12 for the main voice).
- **Decided 2026-10-02:** `gpt-5.4-mini` is the main voice for now (`model` and `fallback_model`), so it answers
  players, writes the Dungeon Master's life stories and Atlas's generated characters; `gpt-5.4-nano` keeps summaries
  and overheard exchanges. `gpt-5.6-luna` is kept back for other uses later.
