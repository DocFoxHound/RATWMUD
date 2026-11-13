# 80. The whole screen

Built 2026-11-13 on `r2-view`. The user's brief: the screen rendered only what the character could see, which made
the game feel unusually dark; render the whole screen, and preserve sneaking, so that sneaking players, NPCs and
animals aren't rendered until they meet the detection requirements.

## What changed

A player used to be sent only what its wolf could see: tiles along a clear line within its sight range (27 tiles ×
vision × age × eyes × injuries × the light and weather), the rest blank or dim from memory, and the wolves and doors
on those lines. Now it is sent its **whole screen**, and sight decides one thing only: who is spotted hiding.

## The rules

1. **The screen** is a rectangle of tiles round the wolf: 96 each side of its own tile and 56 above and below (193 ×
   113), in `Core/RatwScreen.h` and `Client/src/game/screen.ts` alike (a client test reads the header). At the edge of
   the ground there is (the cell, or the 48-tile band round it near a seam) it stops and reaches further the other way,
   as the map does. The page never draws more: at the widest zoom (14 px a tile) a map up to 2576 × 1456 px fits, a
   larger window gets larger tiles, and looking around (Shift/Ctrl + wheel) stops at it.
2. **Ground:** every tile in the rectangle is sent as it is and drawn as seen, behind walls, at night, in fog, past a
   seam. No rays are cast. Tiles outside it are remembered or unknown as before, and a tile is remembered by having
   been on the screen. The memory's format and saves are unchanged.
3. **Characters:** every player, NPC and animal in the rectangle is sent, except one that is **sneaking**
   (`World::sneaking`): crouched (posture `crouching`; moving while lying becomes a crouch), or in a fight stalking or
   come in unnoticed. A sneak is sent only when today's detection is met (`visionClarity > 0`): inside the observer's
   sight range in this light and weather, along a line nothing thins (`sight::bodySeen`), and for a crouch within
   `(7 − 4·sneakSkill/100) × range/27` tiles. Game outside its hunt is never sent (doc 41). `World::shown` is the one
   rule; snapshots and motion frames both use it, so an undetected sneak is absent from the message, not hidden by the
   page.
4. **Doors and fixtures** in the rectangle are all sent; an open way on the screen glimpses the place beyond.
5. **The page** draws all it is sent at full brightness. The night's blue tint and the weather stay; the pool of
   darkness that closed in round the wolf's own sight at night is gone. Remembered ground shows only off the screen
   (the minimap, the World Map).

## Kept for sneaking, and unchanged

- Doc 40's whole model (`notice()`, awareness, cover, posture, ambushes, fights, hunts), what NPCs and animals see
  (`visionClarity`, as ever), hearing, scent and combat. Night, fog, eyesight and injuries still shorten the range a
  sneak is spotted from. Pawsteps and scent cues now concern only those a player isn't sent.

## What follows from it

- What a player may do to what it sees follows the screen: inspect, talk to, invite, list as known, name the speaker of
  a voice. Using a door still needs a clear line to it; trading and apprenticeship keep their line of sight. What is
  read of a post (emotes by sight, words by ear) is still by the wolf's own senses.
- Lying is not hiding: today's code has no detection rule for it, and sleepers and bodies would vanish.
- Cells beside a player load once its screen reaches them. A blind wolf's player is shown the screen like any other.

## Progress

- 2026-11-13: built. Server: `screenOf`, `screenMask` (replaces `visibleTileMask` and `sightBeyond`), `shown`,
  `sneaking`; views keyed by tile and doors (no sight range), made without helper threads. Page: `screen.ts`, shading
  by the rectangle (the WebAssembly sight rule is no longer asked), no night pool, zoom and pan kept on the screen.
  Tests: `world_tests` `theWholeScreen`, `game_tests` `theWholeScreenOnTheWire`, `hunt_tests`, `screen.test.ts`.
- View cost, `world_check EXPORT --simulate 6 8 --players 20 --no-check`, DEV build 73, one run each, before (a build
  of 2026-11-12, the view code the same) and after: a player's view 0.126 → 0.079 ms (worst 3.10 → 1.32); views inside
  the tick 8,190 → 652 ms over 24,000 ticks; tick mean 1.82 → 1.12 ms, p99 6.16 → 3.80, p99 after the first minute
  5.37 → 3.41, worst after it 19.3 → 17.9 (doc 79's baseline: 1.44, 4.47, 4.20, 16.3); streaming 1,041 → 855 ms.
  Twenty players remembered 472,078 tiles in the two hours (19,661 before).
- Open: In Sight lists everyone in the rectangle (wider than a small window's map); wolves past a seam aren't sent.
