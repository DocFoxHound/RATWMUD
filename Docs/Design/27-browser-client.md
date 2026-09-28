# 27. The browser client, and leaving Unreal

Agreed 2026-09-28. The game leaves Unreal Engine entirely. The server is already our own (`Server/ratw_server.cpp`
over the portable `ratw::game::Game`); the client becomes a web page. The NPC phases (doc 26, Phases 7 to 10) wait
until this is done.

## Why this is small enough to do

Unreal does three jobs here, and none of them needs an engine:

- **Drawing.** The client (`Source/RATWMUD/UI`, about 6,000 lines) is 2D only: one hand-drawn 1600×1000 canvas
  scaled to the window (`SRatwGame`), a few forms (`SRatwFrontDoor`), a recoloured portrait (`SRatwWolfDoll`) and
  procedural weather sheets (`RatwWeatherArt`). There are no meshes, materials, actors or imported assets. Every
  Slate call has a direct HTML canvas equivalent (boxes, lines, text, gradients, clipping, rotated images).
- **Networking.** Already replaced: the standalone server's own wire (`RatwLink.h`).
- **Hosting the server.** Already replaced: the Unreal server is a thin adapter over the portable game.

## Keyboard play in a browser

WASD stays. The page listens to `keydown`/`keyup` on the window and keeps the set of held keys, exactly as
`SRatwGame::HeldKeys` does:

- Keys are read by **physical position** (`KeyboardEvent.code`: `KeyW`, `KeyA`...), so WASD sits in the same
  place on AZERTY or Dvorak keyboards.
- Browser auto-repeat is ignored (`event.repeat`); movement is resent every 75 ms while keys are held, as now.
- Held keys are released when the window loses focus, the tab is hidden, or the composer takes focus, so a wolf
  never keeps walking after an Alt-Tab.
- Keys the game uses (WASD, E, I, C, L, M, 1 to 6, PageUp/PageDown, Enter, Escape) have their browser default
  suppressed while the map has focus; the mouse wheel over the map changes pace instead of scrolling.
- Alt+click turns in place, as now; Ctrl+click does the same, for browsers or desktops that take Alt for themselves.
- A few browser shortcuts can't be overridden (Ctrl+W closes the tab). None of the game's keys use Ctrl.

## The shape of it

- **One server, one port.** `ratw_server` serves the built client over HTTP and the game over a WebSocket on the
  same port (7788): a player opens `http://host:7788/`. The Unreal-only TCP link goes away.
- **The wire, unchanged in meaning.** Each WebSocket message is one frame: a kind byte, then the payload the TCP
  link carries today (command JSON; acknowledgement revision + missing flag; zlib-compressed event, snapshot or
  binary motion frame). The browser inflates with `DecompressionStream('deflate')`. Delta snapshot sections,
  `{"$held": key}` map entries and binary motion frames all carry over.
- **Credentials stay safe.** Passwords are accepted only over loopback, as now, until the server speaks TLS (a
  later step). The WebSocket upgrade checks `Origin` against `Host`, so another website open in the same browser
  can't drive a player's session.
- **The client** (`Client/`, TypeScript and Vite like Atlas, no framework):
  - `net/`: the socket, frame decoding, section filling, motion unpacking, and the session rules the player
    controller applies today (lobby versus world, motion sessions, cell generations, stale snapshots).
  - `ui/frontDoor`: sign-in, the roster and the character creator as ordinary HTML forms.
  - `ui/game`: `SRatwGame` ported draw call for draw call onto a canvas, with the composer as a real `<textarea>`.
  - `ui/portrait`: the wolf portrait recoloured from `Data/Portraits/*.png` in a canvas, as `SRatwWolfDoll` does.
  - `ui/weather`: the weather sheets generated in code, as `RatwWeatherArt` does.
  - The terrain table (glyphs and colours) comes from the same generated catalogue Atlas uses.
- **Saves.** File worlds (the demo, Greyfen, Atlas playtests) are saved by the portable file store. A one-off
  converter (`tools/convert_saves.py`) turns the Unreal server's SQLite saves under `Saved/` into it. The
  database worlds (DEV, PROD) are unaffected.

## Tests

- **Server:** the WebSocket handshake, framing, origin check and credential rule in `Tests/server_smoke.cpp`.
- **Client logic:** Node's test runner, like Atlas (`npm --prefix Client test`): sessions and sections, motion
  unpacking and interpolation, input (held keys, blur release, draft recovery), labels and layout rules. These
  replace the `RATW.UI.*` engine tests.
- **Server rules:** engine tests that check the server's wire (privacy, delta sections, motion frames, lighting,
  weather, calendar) move to the portable suite wherever it doesn't already cover them.
- **Smokes:** a headless client in Node (`tools/client/`) runs the scripted scenarios that `RatwScenario.cpp` runs
  today (network, persistence, movement, travel, weather, lighting, society, aging, character, Dungeon Master,
  Mind, Atlas playtest), and the Python smokes launch it instead of Unreal. Screenshots come from headless Chromium
  driving the real page.

## Steps

1. **Server web transport.** HTTP static files and the WebSocket on `ratw_server`, with tests.
2. **Client foundation.** The package, the connection and session layer, and the front door: sign in, roster,
   creation, entering the world.
3. **The game screen.** The full `SRatwGame` port: story feed and composer, local map with terrain, heights,
   doors, wolves and interpolated motion, pace and stamina, world map and travel atlas, character, inventory,
   trade and settings, portraits, weather, lighting and scent.
4. **Tests and smokes.** The headless client, the scenarios, the smokes switched over, and the engine tests moved.
5. **Leaving Unreal.** The portable code moves to `Core/`; the Unreal project, runtime, UI, engine tests and
   launchers are deleted; `server.sh`, `play.sh`, `live.sh` and Atlas playtests start `ratw_server` and open the
   browser; the save converter; docs.

Afterwards: TLS on the server (so remote players can sign in), and then doc 26's Phase 7.
