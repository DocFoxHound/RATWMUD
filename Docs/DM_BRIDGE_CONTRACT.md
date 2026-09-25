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
  "capabilities": ["notice", "weather", "npc_relocate", "economy_transfer"],
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
    "relocationTarget":"","lastActiveAtUnix":0}],
  "accounts": [{"id":"treasury","cash":1000,"stock":{"herbs":100,"meal":50}}],
  "economy": {"minted":1480,"sunk":0,"ledger":[]}
}
```

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
- `economy_transfer`: `{from,to,item:""|"herbs"|"meal",quantity,coins}`.
  Transfer existing money and optional goods in the same direction from one
  existing account to another. No minting, negative values, hidden stock,
  arbitrary recipes, or player-provided prices. Whole coins/quantities only.

Assassinations, brigands, armies, route robbery, faction destruction and combat
are supported as campaign plans, not executable native effects in this slice.
The interface must label unavailable executors and never mark them applied.
