# 28. Cheaper NPC voices: the game answers what it can, a small model the rest

Planned 2026-09-30, not built yet. The aim: much less spent on language models, with NPCs that still sound like
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
