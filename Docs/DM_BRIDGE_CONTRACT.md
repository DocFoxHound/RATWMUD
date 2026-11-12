# Storykeeper private authority bridge (first slice)

This is a separate, opt-in local operator channel, never a player RPC. The
authority is started with `-RatwDMDirectory=<absolute private directory>`.
Storykeeper uses the same directory. No service opens the game's SQLite file.
The folder and its contents contain omniscient data and require the same trusted
OS user; this is not a public or multi-administrator security boundary.

## Snapshot

The authority atomically replaces `snapshot.json` every two seconds:

```json
{
  "version": 1,
  "worldId": "persistent-uuid",
  "sequence": 123,
  "generatedAtUnix": 1790000000,
  "calendarDays": 0.5,
  "capabilities": ["notice", "weather", "weather_front", "npc_relocate", "economy_transfer", "player_grant", "npc_band"],
  "cells": [{"id":"tavern","name":"Tavern","x":0,"y":0,"z":0,
    "width":32,"height":24,"outdoors":false,"terrain":["..."],
    "territory":{"region":"unassigned","claims":[],"chapter":""}}],
  "factions": [{"id":"example","name":"Example","color":"#71918C"}],
  "chapters": [{"id":"example_chapter","name":"Example Chapter"}],
  "characters": [{"id":"npc_scout","name":"Bracken","npc":true,
    "online":true,"active":false,"cell":"tavern","x":4.5,"y":5.5,
    "age":18,"activity":"resting","role":"resident","cash":80,
    "stock":{"herbs":0,"meal":1},"homeCell":"tavern",
    "homeX":19.5,"homeY":15.5,"relocating":false,
    "relocationTarget":"","lastActiveAtUnix":0,"kind":"staged","band":"poor"}],
  "accounts": [{"id":"treasury","cash":1000,"stock":{"herbs":100,"meal":50},"kind":"staged"},
               {"id":"till:job:npc_cook","cash":40,"stock":{"bread":6},"kind":"staged"}],
  "economy": {"ledger":[]}
}
```

(Since Docs/Design/79-a-staged-economy.md, 13, `economy` holds no `minted` or `sunk`: an older game's are read past.)

Each account and character carries its `kind` (doc 79, 9.1: `real` for players and Chapters, `escrow` for coins held in
trust, `staged` for residents' purses, tills, treasuries and homes, which their bands set each night, `dead` for an old
economy's), and each resident its household's `band` (`destitute`, `poor`, `comfortable`, `rich`; `""` before the bands
are first worked out, and for players). An older game names neither: its accounts are taken as `real`. Account ids carry
`:`, `@` and `.` (`till:job:npc_cook`, `home:<cell>:larder`); every stock good is listed, not only herbs and meals.

Lists above are illustrative shapes, not fabricated live content. Offline saved
characters are included with `online:false`, last-known location, and
`active:false`. `active` means an online player issued a meaningful movement,
interaction or roleplay command within five real minutes; typing/heartbeat
messages alone do not qualify. No roleplay text, provider keys, memory records,
or drafts appear. NPC role and account data are authoritative. A snapshot more
than ten seconds old is stale; future timestamps must not extend its lifetime.
The backend must reject a changed/invalid worldId rather than mixing histories.

## Commands and results

Storykeeper atomically writes `outbox/<id>.json` (safe ID, at most 80 ASCII
letters/digits/hyphens/underscores):

```json
{"version":1,"id":"request-id","worldId":"persistent-uuid",
 "createdAtUnix":1790000000,"expiresAtUnix":1790000060,
 "kind":"weather","payload":{"cell":"exterior","preset":"rain"}}
```

The authority validates identity, expiry, bounds and the action allowlist,
persists the effect and receipt in one checkpoint, then atomically writes
`inbox/<id>.json`:

```json
{"version":1,"id":"request-id","worldId":"persistent-uuid",
 "ok":true,"detail":"Weather updated.","appliedAtUnix":1790000001}
```

IDs are replay-safe within a bounded persisted receipt history. Do not recycle
IDs or present this as an unlimited exactly-once transaction journal. Expired,
malformed or cross-world requests fail closed. Queued is not applied.

When the checkpoint fails, a grant, fine, band or transfer that the world's
journal has written stands: its receipt says so ("Journalled, but the
checkpoint failed: it stands") and the channel halts. If the journal has not
written it either, the request is held: no receipt is written, the request
stays in `outbox/`, and the channel halts. The next server answers it from its
receipt (the journal record carries the receipts) if the record was written,
or applies it then if it was not. A held request is never told as standing.
(Doc 79, Phase 9's second repair.)

Supported payloads:

- `notice`: `{scope:"world"|"cell"|"player"|"players", text, target?, targets?}`.
  `target` is a cell/player ID; `targets` is a bounded explicit player list.
  Chapter targets are resolved to member IDs by Storykeeper. A notice goes only
  to connected matching characters; no offline delivery guarantee is implied.
- `weather`: `{cell,preset:"clear"|"rain"|"snow"|"fog"|"seasonal"}`.
- `npc_relocate`: `{npc,cell,x,y}`. Named existing NPC, finite traversable point.
  The accepted request starts real navigation; it does not teleport or create a
  resident. Arrival is verified later by `homeCell` and `relocating:false`.
  Only non-recruited resident-role NPCs are supported in this first slice;
  essential cook/keeper/forager jobs are protected. New home controls rest/social
  time; existing jobs and food procurement remain the demonstration commute.
- `economy_transfer`: `{from,to,item:""|<a catalogue good>,quantity,coins}`.
  Transfer existing money and optional goods in the same direction from one
  existing real or escrow account to another. A staged end (a resident's purse,
  a till, a treasury) is refused: "staged accounts are set by their bands". No
  minting, negative values, hidden stock, arbitrary recipes, or player-provided
  prices. Whole coins/quantities only.
- `player_grant`: `{player,coins,reason}`. A grant (coins 1 to 2,500: a
  registered faucet "dm grant: <reason>", capped per player a day by
  `status.json` `caps.grantsADay`) or a fine (coins -1 to -2,500: a sink "dm
  fine: <reason>", at most the player's cash) on a player's account; the reason
  is plain words, at most 80 bytes. No funded source (doc 79, 12; D16).
- `npc_band`: `{npc,band,days?}`. The resident's household takes `band`
  (`destitute`, `poor`, `comfortable`, `rich`) for `days` (1 to 112, default 28)
  over its score and its authored band; `band:"clear"` (no `days`) ends the DM's
  band.

Assassinations, brigands, armies, route robbery, faction destruction and combat
are supported as campaign plans, not executable native effects in this slice.
The interface must label unavailable executors and never mark them applied.
