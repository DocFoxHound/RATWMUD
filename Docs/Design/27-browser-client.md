# 27. The browser client, and leaving Unreal

Agreed and built 2026-09-28. The game left Unreal Engine entirely. The server is our own (`Server/ratw_server.cpp`
over the portable `ratw::game::Game` in `Core/`); the client is a web page (`Client/`). The NPC phases (doc 26, Phases
7 to 10) waited for this.

## Why this is small enough to do

Unreal does three jobs here, and none of them needs an engine:

- **Drawing.** The Unreal client (about 6,000 lines of Slate) was 2D only: one hand-drawn 1600×1000 canvas
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

## Built

- **The server's web side** (`Core/RatwWeb.h`, `Server/ratw_server.cpp`): HTTP/1.1 for the client's files (hashed
  assets cached for good, the page never stale; nothing outside the folder, no hidden names), and RFC 6455 WebSockets
  at `/ws` (masked client frames, fragments, ping, close; oversized or unknown messages end the connection). The
  upgrade checks `Origin` against `Host`. The plain TCP link the Unreal client used is gone.
- **The client** (`Client/`, 110 KB of JavaScript, 40 KB gzipped):
  - `net/`: messages inflated synchronously (`net/inflate.ts`; the browser's `DecompressionStream` took about 110 ms a
    message in headless Chromium and fell behind), delta sections, binary motion frames, and the session rules.
  - `ui/frontDoor.ts`: sign in, the roster and the creator as HTML forms, scaled like the old 1440×940 layout.
  - `game/state.ts` and `game/paint.ts`: `SRatwGame` ported rule for rule and draw call for draw call, in linear-light
    colours as Slate mixed them, with Roboto and DejaVu Sans Mono bundled (`Data/Fonts`).
  - `ui/portrait.ts` and `game/weatherArt.ts`: the recoloured portraits and the weather sheets, generated as before.
- **Tests:** 27 client tests (the old `RATW.UI.*` and motion engine tests), the front door in headless Chromium
  (`npm --prefix Client run test:browser`), and the server's web side in `server_parts_tests` and `server_smoke`.
  Engine tests of the server's rules were checked against the portable suite and the gaps filled in the C++ tests.
- **Scripted players** (`tools/client/scenario.ts`, driven by `tools/game_run.py`): every smoke runs the server and
  plays in Node (`--headless`) or in the real page in headless Chromium (`tools/client/browser.mjs`), which takes the
  screenshots in `artifacts/screenshots/`. All pass.
- **Launchers:** `server.sh`, `play.sh` (a free port and the browser), `connect.sh` and `live.sh`; Atlas playtests use
  `play.sh`. `tools/convert_saves.py` converts the Unreal server's SQLite saves.
- **Removed:** the Unreal project, its runtime, UI and engine tests, the packaging and engine-test scripts. The
  portable code moved from `Source/RATWMUD/Core` to `Core/`.

## Still to do

- **TLS on the server**, so remote players can sign in (until then passwords are accepted only over loopback).
- Then doc 26's Phase 7.

## Steps (as planned)

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
