# 76. The desktop client (2026-11-07)

Drafted 2026-11-07, revised the same day after three reviews (see "Review" at the end). **Agreed 2026-11-07** with its recommended defaults, as amended by doc 77.

**Changed by doc 77 (2026-11-07):** the user then asked for central accounts, community-hosted worlds and a world list
in the client. A host's machine can't be trusted to serve the page, so **section 4.2 is reversed: the page is bundled
in the client**, with a protocol number checked against each world. The client opens on sign-in, wolves and the world
list, not on one server's page. Phases 0-2 here (window, keys, close question, Reconnect) still stand and are folded
into doc 77's Phase 3. See doc 77, section 9.

The user, 2026-11-07 (verbatim):

> Okay, one thing I do want to spend some time doing right now is making a client for the end user. Up until now I've
> been using the browser but I think I would like to make a client. It'd be super simple, and it'd probably just be its
> own type of browser that just directly connects to the game.

This doc is a plan only. No code was changed, nothing was built or installed, and no server was started. The code is
`ridgemere2` at 0af9bc0. It builds on doc 27 (the browser client), doc 31 (responsiveness and the wire; it already said
"a desktop wrapper (Tauri or Electron) is possible later, with the same code", 31-responsiveness.md:103) and doc 19 (the
rule that passwords are taken only from this computer).

Three words, used the same way throughout:
- **the client**: the app a player installs;
- **the shell**: the client's own native part (the window, a small waiting page, keys, later updates);
- **the page**: the game itself, `Client/`, the same web page the browser opens today.

Standing rules, kept: characters are male or female only; never the word for the domestic animal in anything players
read (wolves, pups); nothing writes PROD.

## Headline

- **The client is a window onto the game's own address.** A small Tauri 2 app opens the page ratw_server already
  serves. It gives what a browser tab can't: its own icon and window, no reload or close shortcuts that drop a player
  out of the world, fullscreen where Escape stays with the game, a remembered window, a question before closing while
  in the world, and a waiting page while the server is down.
- **A thin shell.** The page keeps coming from the server, so client and server always match, the Origin check passes
  unchanged, and game releases never need a client release.
- **No custom bridge.** The page uses only Tauri's own window calls (fullscreen, and "the window is about to close"),
  granted to the game's address alone. There are no shell commands of ours, so no shell-versus-page version check.
- **v1 is local.** Remote players can't sign in today, in a browser or in any client: passwords are taken only from
  loopback (Server/ratw_server.cpp:422, Core/RatwGame.cpp:2343-2346), by design. So v1 is the client on this computer,
  and on the user's own other computers through an SSH tunnel. Installers, updates, signing and the hosting box are a
  later "release" step, planned with doc 77, "Going online", which is the real blocker.
- **Runner-up: Electron**, if WebKit on Linux or macOS misbehaves (decided after Phase 0), or for Steam later.
- **Time: v1 is about 5-7 hours** in three phases, all on this laptop. The release step later is about 4-6 more, plus
  the user's machines and accounts.

## 1. What it is, and why

A player installs "Run Against the World" like any game and opens it from the Start menu, the Dock or the app menu. It
opens on the front door. There is no address bar, no tabs, no bookmarks and no browser menu.

Why not stay in the browser:
- **Keys.** In a tab, F5 or Ctrl+R reloads the page. Reloading disconnects, and disconnecting takes the wolf out of the
  world and signs the account out (Core/RatwGame.cpp:1991-2006). Ctrl+W closes the tab, and a page can't stop it (doc
  27, "Keyboard play in a browser"); at most it can make the browser ask first (`beforeunload`, which the page doesn't
  use today). The game's key handler ignores Ctrl (`keyOf` passes it, Client/src/game/view.ts:69, but KeyC toggles the
  character sheet with or without it, Client/src/game/state.ts:1900-1903), so Ctrl+C on the map can't copy.
- **Fullscreen.** A page can only ask for its own fullscreen (`requestFullscreen`, unused today), and that one ends on
  Escape, which the game needs for its panels. A window's own fullscreen doesn't.
- **It feels like a game:** its own icon, taskbar entry and remembered window; one known engine per OS.

Part of the key trouble can be fixed in the page alone, and v1 does that too, so browser players gain (Phase 2). What a
page can't fix is Ctrl+W, a closing window, and fullscreen that keeps Escape.

What it is not: a new client. The game stays the page in `Client/`, one codebase. The browser keeps working as now.

## 2. What the plan rests on

Today's code, read for this plan:

- **The page finds its server from its own address.** `gameUrl()` builds `ws(s)://<page host>/ws`
  (Client/src/net/connection.ts:28-30). The `?server=host:port` override always uses `ws://` (Client/src/main.ts:17-18).
- **The server checks the page's origin.** The upgrade is accepted only when `Origin` is `http(s)://` plus the `Host`
  header, or absent (Core/RatwWeb.cpp:121-133); otherwise 403 (Server/ratw_server.cpp:279-280; tested in
  Tests/server_parts_tests.cpp:746-752). A page loaded from the server passes. A page bundled into an app (origin
  `tauri://localhost`, `http://tauri.localhost`) would be refused.
- **No CORS.** Replies carry only Content-Type, Content-Length, nosniff, Referrer-Policy and `Connection: close`
  (Core/RatwWeb.cpp:166-171), plus Cache-Control (Server/ratw_server.cpp:300-304). So the shell's own waiting page can't
  read anything from the server; the shell checks the server from Rust (3.1).
- **Every launch gets the current page.** `assets/` is cached for good, everything else `no-cache`
  (Server/ratw_server.cpp:300-304). No ETag, so the fonts (673 KB) are downloaded in full each launch.
- **The page uses few browser features.** Canvas 2D (Client/src/game/view.ts:122), its own inflater
  (Client/src/net/inflate.ts), WebAssembly by `fetch` (Client/src/game/walker.ts), a module Worker, synthesised Web
  Audio (Client/src/ui/sound.ts:28-48), localStorage for UI preferences. Also a file picker for portraits
  (Client/src/ui/frontDoor.ts:344) read by `createImageBitmap` (Client/src/ui/artwork.ts:49), and colour pickers
  (`<input type="color">`, frontDoor.ts:523 and 590). `CompressionStream` appears only in a test helper
  (Client/src/net/wire.ts:94). Fedora's WebKitGTK 2.50.5 has all of it. The build sets no `build.target`
  (Client/vite.config.ts:41), so Vite 8.3.1's default applies: Safari 16.4 (Client/node_modules/vite/dist/node/chunks/node.js:699-705).
  That covers syntax only; it adds no APIs.
- **No reconnect.** One connection per page load (Client/src/main.ts:31). When it closes, the front door says "The
  connection to the world server was lost." (Client/src/net/session.ts:140-142). F5 is the only way back.
- **Sign-in is loopback only.** "Local" is decided at accept time from the peer address alone
  (Server/ratw_server.cpp:418-422; Core/RatwAccountsCore.cpp:101-145). The lobby sends `credentialsAllowed`
  (Core/RatwGame.cpp:2055) and the page won't send a password without it (Client/src/net/session.ts:38, 64-68). No TLS.
- **Nothing about a login is stored** (Client/src/ui/frontDoor.ts:1-3).
- **15 native dialogs:** 10 `window.confirm` (lodging.ts:52, stall.ts:68, storyteller.ts:55 and 56, table.ts:71,
  place.ts:71, archive.ts:94, story.ts:190, dialogs.ts:294 and 512) and 5 `window.prompt` (stall.ts:44, state.ts:892,
  dialogs.ts:400, 449 and 504). If a webview doesn't support them, `confirm` returns false and `prompt` null: the
  actions behind them would silently never happen.
- **Keys** are read by `KeyboardEvent.code` from window listeners (Client/src/game/view.ts:164-175). Walking depends on
  keyup: `heldKeys.delete` (state.ts:1921). Held keys are released on blur and when the page is hidden
  (view.ts:177-184). While a panel is open, every key is swallowed (state.ts:1847), F5 and F11 included. Right-click
  is already taken on the map (view.ts:207) and on some rows (Client/src/ui/hud/hud.ts:506).
- **The built page** (`Client/dist`) is 9.6 MB. 8.4 MB of that is `portraits/*.png` (copied by Client/vite.config.ts:10),
  which the page never loads.
- **This laptop:** Intel plus NVIDIA (RTX 4070, driver 580) on Wayland, the hardest case for WebKitGTK. Fedora's Rust
  1.94.0 (no rustup) and every -devel package Tauri needs on Linux are installed (`webkit2gtk4.1-devel` 2.50.5,
  `gtk3-devel`, `libsoup3-devel`, `librsvg2-devel`, `openssl-devel`, plus `fuse-libs` and `rpm-build`). Still to
  fetch, per user, nothing system-wide: the Tauri CLI as an npm dev dependency in `Desktop/`, and a few hundred crates
  on the first `cargo build`. If a Tauri release needs a newer Rust than 1.94, the fallback is rustup in `~/.rustup`
  (a per-user install; the user's call). The repo has no `.github/` yet.

## 3. The shape

```
 Player's computer                                  The server (this laptop now; the box after doc 77)
 +---------------------------------+               +-------------------------------------------+
 | Run Against the World (Tauri)  |   http(s),    | ratw_server: Client/dist and /ws, as now   |
 |  waiting page (bundled, tiny)   |   ws(s)       |   (later: Caddy in front for TLS, doc 77)  |
 |  window -> <address>/           | ------------> |                                           |
 +---------------------------------+               +-------------------------------------------+
```

### 3.1 What happens when a player opens it

1. The window opens on the shell's bundled waiting page: the RATW mark and "Connecting to the world…".
2. Rust (not the page, because of CORS) requests `<address>/` with a 5-second timeout.
   - **A 200:** the window goes to `<address>/`.
   - **A 502 or 503** (later, from the proxy): "The world is restarting."
   - **No answer** (DNS, refused, TLS): "Can't reach the world. Check your internet connection and your computer's
     clock." A wrong clock is a common cause of TLS failures.
   - It tries again every 5 s, with a random ±50% so that many clients don't come back in step after a restart.
     Buttons: Try again, Quit.
3. The page loads as in a browser.
4. **The connection is lost:** the front door's message gets a Reconnect button. It polls `fetch('/')` (same origin, no
   CORS) every 5 s ±50% until it gets a 200, then reloads the page. It works the same in a browser and in the client,
   and needs nothing from the shell.
5. **Closing while in the world:** "Leave the world? Your wolf leaves it with you." Leave, or Stay. On the front door
   or the waiting page the window just closes.

## 4. The decisions

### 4.1 The wrapper: Tauri 2. Runner-up: Electron.

| Option | Engine (Windows / macOS / Linux) | Download | Verdict |
|---|---|---|---|
| **Tauri 2** | WebView2 (Chromium) / WKWebView / WebKitGTK | A few MB (a Linux AppImage 70+ MB) | **Recommended** |
| **Electron** | Its own Chromium on all three | About 100 MB | **Runner-up** |
| Wails 3, Neutralinojs | As Tauri | Small | Smaller communities, fewer guarantees |
| webview/webview, CEF (C++) | As Tauri / Chromium | Tiny / about 110 MB | Installer, updater, window state all ours to write |
| Chrome or Edge `--app=URL` | The player's browser | Nothing | A stopgap only: Ctrl+W still closes it |

**Why Tauri:** smallest download and memory; Windows, where most players will be, gets WebView2, which is the Chromium
we test in now; loading the remote address leaves `gameUrl()` and the Origin check as they are; no Node runtime inside;
an updater built in for later.

**Why not Electron first:** about 20 times the download, a new major version about every 8 weeks. Its strength is one
Chromium everywhere, which matters only if WebKit gives trouble.

**What would make us switch (D1, after Phase 0):** a blank window or a slow canvas on NVIDIA under Wayland that the
safe-graphics start (section 9) doesn't cure, or Tauri's own window calls refused to the remote page. Because the shell
only opens an address, switching costs about Phase 1 again.

**Version:** the current Tauri 2 release, never older than 2.11.1. From then on, calls from a remote origin always
go through the access check (tauri-apps/tauri PR #15266). The doc's design relies on that: only the permissions
granted to the game's address are allowed.

### 4.2 The page comes from the server (a thin shell)

| | Thin shell (page from the server) | Bundled page (page inside the app) |
|---|---|---|
| Origin check | Passes unchanged | Needs an allowlist of app origins |
| A game release | Nothing to ship to players | A signed client release to every player each time |
| walk.wasm, terrain catalogue, packed formats | Always the server's own | Drift when a player hasn't updated |
| First launch | About 1.3 MB of page and fonts | Nothing to download |

**Recommended: thin.** The walking wasm is the server's own movement rules compiled for the page (Core/RatwStep,
tools/build_wasm.sh); a stale copy means refused moves. A thin shell can't have one.

Correction to the first draft: a page and a server can briefly differ. The server reads `dist` from disk on every
request (Server/ratw_server.cpp:297), and a code release needs a server restart. Exit 75 comes only from a new world
build, and only when nobody is connected (Core/RatwGame.cpp:870-888); with players on, a code release goes through
maintenance (60 s warning, exit 76, Core/RatwGame.h:276-277, tools/live.sh:51-70). After it, the Reconnect button loads
the new page. How releases are deployed on a busy box is doc 77's (5).

The costs of thin: macOS and Linux players run the page on WebKit, not Chromium, and the server must be up to play.

### 4.3 Connecting: one address, one override

- **The address is built in.** v1: `http://127.0.0.1:7788/` (this computer). At release: `https://play.<domain>/` (Q1).
- **The override is `--server URL` only** (what `tools/desktop.sh` passes). Nothing is remembered between launches; no
  hidden field, no environment variable. So a player can't be talked into pointing the client at a copy of the game
  that collects passwords by typing an address somewhere.
- **Allowed:** `https://` anywhere; plain `http://` only for loopback (127.x, `localhost`, `::1`). Home-network http
  is not allowed: it can't sign in anyway, and on macOS it would need App Transport Security exceptions and the Local
  Network prompt.
- **When the override is on, it shows:** the title reads "Run Against the World · 127.0.0.1:41235 (not the live
  world)".
- **Scratch servers and DEV.** `bash tools/scratch.sh dev` prints its port; `bash tools/desktop.sh 127.0.0.1:PORT`
  (new) starts the client on it, the built one if there is one, otherwise Tauri's dev mode. Kill the scratch server
  after, as CLAUDE.md says. Query parameters pass through (`?identity=ash&name=Ash` on a `--dev-identity` server,
  Client/src/main.ts:65-68; `?perf`).

### 4.4 Keyboard

| Key | In a browser tab today | In the client |
|---|---|---|
| F5, Ctrl+R | Reload: the wolf leaves the world | Nothing while in the world (the page blocks them, in a browser too) |
| Ctrl+W | Closes the tab | Nothing (no tabs). Closing the window asks (4.5) |
| Ctrl+F, Ctrl+P | Find, print | Nothing while in the world (the page blocks them) |
| F12, Ctrl+Shift+I | Developer tools | Nothing (developer tools only in dev builds) |
| Ctrl and +/-, Ctrl+wheel | The browser zooms | The game only (Tauri's zoom keys are off by default) |
| F11, Alt+Enter | Browser fullscreen | The window's fullscreen; Escape stays with the game |
| Alt+F4, Cmd+Q, Cmd+W | Closes the browser | Asks first when in the world |
| Right-click off the map, outside text fields | Back / Reload / Inspect | Nothing; text fields keep their Cut/Copy/Paste menu |
| Ctrl+C (Cmd+C) on the map or a story | Toggles the character sheet | Copies (in the browser too) |

How, all in the page except where noted:
- **Game keys leave Ctrl and Cmd alone on keydown only.** Keyup is always handled, so W pressed alone and released
  with Ctrl held (Ctrl+click to face a tile, state.ts:2113-2118) still stops the wolf. Meta going down or up clears the
  held keys (`focusLost()`), because macOS WebKit often sends no keyup for other keys while Cmd is held. Ctrl-click and
  Ctrl-wheel keep their meaning (view.ts:198, 219).
- **An in-world guard**, a capture-phase listener ahead of the game, so an open panel (state.ts:1847) can't swallow it:
  F11 and Alt+Enter toggle the window's fullscreen in the client; F5, Ctrl+R, Ctrl+F and Ctrl+P are blocked while in
  the world; in a browser only, `beforeunload` makes Ctrl+W and closing the tab ask first while in the world.
- **The context menu:** the page calls `preventDefault` on `contextmenu` outside inputs and textareas. The webview's own
  menu stays on, so text fields keep it. Inspect is absent in release builds because developer tools are off.
- **Windows (WebView2):** whether anything else gets through (its find bar, say) is checked when Windows is first tried
  (Phase 3). If it does, the shell turns off WebView2's browser accelerator keys (`AreBrowserAcceleratorKeysEnabled`,
  a few lines of Rust, or tauri-plugin-prevent-default).
- **macOS (the shell):** a hand-built app menu. Edit keeps Copy, Paste, Select All and Undo (WKWebView needs them for
  Cmd+C and Cmd+V). Quit (Cmd+Q) is our own item, which closes the window and so goes through the close question;
  Tauri's standard Quit would end the app without asking (tauri-apps/tauri issue #5288). Close Window is Cmd+W.

### 4.5 The window

- **Size.** The front door is laid out at 1440×940 and scaled (Client/src/ui/frontDoor.ts:144-146). The window opens at
  that size, or maximised when the screen's work area is smaller (a 1920×1080 screen at 150% is about 1280×672 above
  the taskbar). Minimum 1024×640.
- **Remembered** size, position, maximised and fullscreen, by tauri-plugin-window-state.
- **Fullscreen** is the window's own (Tauri's `setFullscreen`), not the page's `requestFullscreen`, which ends on Escape
  and is unreliable in WKWebView (tauri issue #9557).
- **Closing in the world asks first**, with the page's own dialog. The page registers Tauri's `onCloseRequested`; while
  in the world it stops the close, asks, and on Leave destroys the window. A native dialog from Rust inside the close
  event is avoided: done blocking, it can deadlock on Linux.
- **Minimised**, the page stops drawing but keeps its connection. The 2 s ping (Client/src/net/connection.ts:25) and
  the 30 s network report run on timers, which hidden pages slow down (Chromium hard after about 5 minutes; macOS App
  Nap too). The server has no idle kick, so a disconnect is unlikely, but ping figures may be off. Checked by 10
  minutes minimised on each OS; Tauri's `backgroundThrottling` window option is the fix where it is supported. Also
  checked: `document.hidden` turns true on minimise (view.ts:179-184 depends on it).
- **Sound.** `sound.ts` makes its AudioContext lazily on the first sound cue, which comes from a game event; WebKit may
  refuse audio without a key or click first. If Phase 0 hears silence, the page resumes audio on the first key or click.
- **Dropped files** don't navigate the window (Tauri's default drop handling; no work).

### 4.6 What the page may use of the shell

The page is remote content, so it gets as little as possible:

| Permission | What for |
|---|---|
| `core:window:allow-set-fullscreen`, `core:window:allow-is-fullscreen` | F11 and Alt+Enter |
| `core:window:allow-destroy`, `core:event:allow-listen` (and unlisten) | `onCloseRequested`: asking before closing |

- Granted by one capability with `remote.urls` set to the game's origin only. When `--server` is given, Rust adds the
  same capability for that origin at start (`Manager::add_capability`). The page reaches them through
  `window.__TAURI__` (`withGlobalTauri`). Phase 0 confirms a remote page can call all of them.
- **No commands of our own**, so no `build.rs` AppManifest. If one is ever added, it needs an AppManifest entry and a
  grant in the remote capability (PR #15266), and with it a shell-version check.
- Nothing else: no files, no shell commands, no opening of addresses. A script slipped into the page could at worst
  toggle fullscreen or skip the close question.
- **The navigation rule:** the window shows only the waiting page and the game's origin. Other addresses and new
  windows are refused (no links leave the game today).
- In a browser there is no `window.__TAURI__`, and the page's shell code does nothing.
- **The client is not a trust boundary.** Anyone can open the page in a browser, or write their own client. The server
  stays authoritative for everything.

### 4.7 Signing in, and what is remembered

- As in the browser: username and password over the socket, the password box cleared at once, nothing kept.
- The client has its own web storage, apart from any browser's. Its data folder is roughly
  `%LOCALAPPDATA%\<identifier>\EBWebView`, `~/Library/WebKit/<identifier>` or `~/.local/share/<identifier>`; Phase 0
  writes down the Linux path, Phase 3 the others.
- Later, with doc 77: "Remember me" as a token the server issues, kept in the page's own storage, never a password.

## 5. What the server needs

**For v1: nothing.** The client opens the page the server already serves, on this computer.

For players on the internet, the hosting work belongs in doc 77, "Going online", not here. The reviews found these,
for doc 77 to take up:
- **The same-box proxy trap.** With Caddy (or any proxy) on the box, every player arrives from 127.0.0.1 and would be
  "local" (Server/ratw_server.cpp:418-422): passwords taken, doc 19 sidestepped by accident. The fix must also leave
  the operator's own loopback access local. A second listening port that only the proxy uses (connections on it are
  never local, and take their address from `X-Forwarded-For`) is clearer than judging by headers. Refuse it together
  with `--dev-identity` or `--dev-tools` (`hello` is gated only by its flag, Core/RatwGame.cpp:2623-2629).
- **Caddy** for TLS: from Caddy's own repository (`handle_errors` with status codes needs 2.8+; Debian 12 and Ubuntu
  24.04 ship 2.6). `stream_close_delay` on `reverse_proxy`, or a config reload drops every WebSocket, and so every
  player. Let Caddy serve `dist` with `file_server` (ETags, compression) and proxy only `/ws`, so a launch storm doesn't
  load the game's network thread (Server/ratw_server.cpp:26-28).
- **Deploying on a busy box:** code releases through maintenance (4.2); `dist` built into a versioned folder and
  switched at the restart, old `assets/` kept a day, since today a build in place 404s and removes hashed files open
  pages still need (Client/vite.config.ts:41 empties `dist`).
- **Limits:** `LimitNOFILE` well above 1,000 sockets; today a failing `accept()` just ends the accept loop
  (Server/ratw_server.cpp:411-413) on level-triggered epoll (:221), so running out of descriptors could spin a core.
- **Sign-ins after a restart:** 24 a minute for everyone, successful ones included (Core/RatwAccountsCore.cpp:487-500),
  keyed by connection and forgotten on disconnect (Core/RatwGame.cpp:2389, 1999). 1,000 players would need about 42
  minutes to get back in. Limits by address, session tokens, a reconnect that keeps the wolf, connection caps.
- **Exposure:** the game port bound to 127.0.0.1 (`tools/server.sh:43-44` defaults to 0.0.0.0), the studio on loopback
  (`tools/studio.py:1674` defaults to 0.0.0.0), only 22, 80 and 443 open.
- **Wording:** the lobby's "Local development accounts only…" (Core/RatwGame.cpp:2056) and the front door's "BROWSER
  CLIENT" and "LOCAL TEST ACCOUNTS ONLY" lines (Client/src/ui/frontDoor.ts:122, 124-125).

## 6. Where the code lives

```
Desktop/                          the client's shell (new)
  package.json                    @tauri-apps/cli as a dev dependency; scripts dev, build
  waiting/index.html              the waiting page, bundled (no framework, a few KB)
  src-tauri/
    Cargo.toml
    tauri.conf.json               name, identifier, window, bundle targets
    capabilities/game.json        the game's origin and its window permissions (4.6)
    src/main.rs                   the window, the server check, the navigation rule, the macOS menu
    src/address.rs                the address: built-in, --server, which http is allowed (unit tested)
    icons/
Client/src/shell.ts               the page's side (4.4, 4.5); does nothing in a browser
tools/desktop.sh                  bash tools/desktop.sh [HOST:PORT]: the client on a server
```

- `Desktop/` never builds `Client/`; the page's build stays `npm --prefix Client run build`.
- `.gitignore` gains `/Desktop/node_modules/` and `/Desktop/src-tauri/target/`.

## 7. Phases

Hours of agent work. v1 (Phases 0-2) runs entirely on this laptop, on Linux.

| Phase | Work | Time | Ends with |
|---|---|---|---|
| **0. Spike** | `Desktop/` skeleton: a Tauri window on a scratch server at 127.0.0.1 (stopped after). Both GPUs, Wayland, the safe-graphics start. Check Tauri's Rust version needs against 1.94. Check: a remote page calls all four permissions; confirm and prompt; the file and colour pickers; sound; the window's fullscreen; 10 minutes minimised; the data folder. | 1.5-2 h | **D1**: Tauri, or Electron |
| **1. The shell** | The waiting page and the server check with jittered retries; the address and `--server`; the capability (and its runtime copy for `--server`); the navigation rule; window size and state; the macOS menu with our own Quit (written now, tried in Phase 3); the title; icons; `tools/desktop.sh`; `cargo test` for the address rule. | 2-3 h | The client on this computer |
| **2. The page** | `shell.ts`: F11 and Alt+Enter, the close question. For browsers too: game keys skip Ctrl/Cmd on keydown, Meta clears held keys, the in-world guard and `beforeunload`, the context menu, the polling Reconnect button; sound on the first key if Phase 0 needs it. Client tests. | 1.5-2 h | `npm --prefix Client test`, typecheck and test:browser green; the browser unchanged otherwise |

**v1: about 5-7 hours.** If a phase runs past its estimate, stop and ask (memory `session-budget`).

Later, planned together with doc 77 (not counted in v1):

| Phase | Work | Time | Needs |
|---|---|---|---|
| **3. Windows and macOS** | The repo's first workflow, `.github/workflows/desktop.yml`: tauri-action builds, unsigned, as downloadable artifacts, plus a `cargo test` step. Tried from a browser download on the user's machines: the checklist (8), WebView2's keys, WKWebView's confirm and prompt (in-page dialogs for the 15 calls if they fail there), Cmd+Q. One Linux format chosen: the CI-built AppImage run on this laptop (it carries the runner's WebKitGTK, not Fedora's), or `.rpm`/`.deb` (the distribution's own). | 2-3 h, plus CI rounds of 15-25 min | A Windows PC; a Mac for macOS |
| **4. Release** | The production address built in; `signingIdentity: "-"` and `minimumSystemVersion` for macOS; the updater key and secrets; 0.1.0 then 0.1.1, the update proven from a browser download; the download and how-to-open pages under `/desktop/`. | 2-3 h | The domain and box (doc 77); doc 77's closed beta |

### Decision points

- **D1 (after Phase 0):** Tauri, unless the reasons in 4.1 hold; then Phase 1 is built in Electron, for about the same
  time.
- **D2 (before Phase 4):** has doc 77's closed beta landed? If not, the release waits. Tunnels are not a way to reach
  other players (8).

## 8. Tests and checks

- **Shell:** `cargo test` in `Desktop/src-tauri`: the address rule (https anywhere, http only for loopback, normalising
  slashes and ports) and the navigation rule.
- **Page:** a `shell.test.ts` (no Tauri: nothing happens; keydown with Ctrl skipped, keyup always handled), and the
  existing `npm --prefix Client test`, `npm --prefix Client run typecheck` and `test:browser`.
- **By hand, on each OS** (from Phase 3 on, starting from a browser download, so that Windows' Mark-of-the-Web and
  macOS's quarantine apply):

| Check | Pass |
|---|---|
| Start with the server down, then up | Waits, then loads the front door by itself |
| Sign in on a scratch server at 127.0.0.1 | Works; the title shows the override |
| WASD; Alt-Tab while walking | Walks; stops on Alt-Tab |
| Hold W, press Ctrl (Cmd), release W; Ctrl+click while walking | The wolf stops; faces the tile |
| F5, Ctrl+R, Ctrl+W, F12, Ctrl+F, Ctrl+P in the world | Nothing happens |
| Ctrl+wheel, Shift+wheel | The map; no page zoom |
| Select story text, Ctrl+C (Cmd+C), paste in the composer | Copies; the character sheet stays shut |
| Right-click in the composer; right-click on empty page | Text menu; nothing |
| F11 and Alt+Enter, then Escape with a panel open | Fullscreen; Escape closes the panel, stays fullscreen |
| Restart the client | Same size, place and fullscreen state |
| Close, Alt+F4, Cmd+Q in the world; on the front door | Asks; doesn't ask |
| Server stopped mid-game, then started | Reconnect waits, then the front door |
| A sound cue after the first key | Plays |
| A stall price; giving up a lodging | The dialogs work |
| Portrait upload; a custom colour | The file picker and the colour picker open and work |
| Minimised 10 minutes | Still connected; `document.hidden` was true |
| Linux on this laptop, Intel and NVIDIA | Full speed, or the safe-graphics start fixes it |

- **The user's own other computers before TLS:** `ssh -L 7788:127.0.0.1:7788 <this computer>` on the other machine,
  then `--server http://127.0.0.1:7788/`. The tunnel ends on loopback, so sign-in is allowed, and it is encrypted.
  Only for the user's own machines: a tunnel counts as "this computer", which also lets `hello` in on a `--dev-identity`
  server (Core/RatwGame.cpp:2623-2629), and an SSH account reaches Postgres, the studio and `Saved/Config`. This
  computer needs `sshd` running (the user's call).

## 9. Risks, and out of scope

| Risk | Defused by |
|---|---|
| WebKitGTK gives a blank window or a slow canvas on NVIDIA under Wayland | Phase 0 tests both GPUs here. A documented safe-graphics start (`--safe-graphics`, WebKit's DMA-BUF and compositing workarounds) for players who hit it, off by default. Electron if it can't be cured (D1). The CI-built AppImage is tried here too (Phase 3); bundled-library AppImages are a known cause of blank windows. |
| WKWebView differs: confirm and prompt, sound, keys with Cmd | Phase 3's checks on a real Mac; in-page dialogs if needed; macOS is best-effort until then. |
| Mac and Linux players run WebKit while our tests run Chromium | The by-hand checklist at each shell release; WebKit in `test:browser` later. |
| The remote page misuses the shell | Four window permissions, one origin; the navigation rule; no developer tools in release builds. |
| A player is talked into pointing the client elsewhere | `--server` only, nothing remembered, the title marker. |
| The domain lapses and someone else takes it | Auto-renew, registrar lock, several years paid (Q1). The address is built into every client. |
| A same-box proxy makes everyone "local" | Doc 77, before the box faces the internet (5). |
| The client works, but nobody remote can sign in | Said in the headline; doc 77. |
| Unsigned installers scare players; Smart App Control blocks them | How-to-open pages; Windows signed before open sign-ups (Q4). |
| The updater key is lost | Two offline copies (password manager and a USB stick), never committed: the repo is public. Lost, installs can never update themselves again. |

Not in v1: installers, updates and signing (Phases 3-4); Steam (the overlay doesn't draw over WebView2; Electron
would be the route); a launcher; offline play; a server list; a bundled page; a stored password; a reconnect that keeps
the wolf; tray, notifications, taskbar flashing; deep links; crash reports and telemetry (later and cheap: the page
could name the client in its net report); mobile, gamepads, ARM for Windows or Linux; Flathub, Homebrew, the Microsoft
Store; several windows.

Noticed in passing, each its own small commit, not counted above:
- The half-minute network report's `setInterval` (Client/src/main.ts:49) is started on every world entry and never
  stopped, while `lobby()` drops the view (main.ts:35-36). Each return to the lobby adds another, holding a stale view.
- Fonts into hashed assets (a relative `url()` in Client/src/styles.css:1-3), cached for good instead of 673 KB a launch.
- Portraits out of `dist` (8.4 MB the page never loads).
- `application/wasm` as the wasm type (Core/RatwWeb.cpp:292-308).
- Optional later: a `build` field in the lobby event (Core/RatwGame.cpp:2050-2056).

## 10. Release notes, for Phases 3-4

Kept short; these get decided at release time.
- **One installer per OS:** NSIS `.exe` per user for Windows 10 and 11 (the installer fetches WebView2 where missing),
  a universal `.dmg` for macOS, and one Linux format (Phase 3).
- **Updates:** Tauri's signed updater at launch. Since tauri-plugin-updater 2.10.0 it handles every bundle type,
  `.deb` and `.rpm` too, but the rpm path runs `rpm -U`, which needs root and doesn't suit rpm-ostree systems: test it,
  or leave Linux packages to manual updates. `latest.json` and the bundles on GitHub Releases, as tauri-action writes
  them (the simplest). Putting `latest.json` on the domain only helps if the bundles move there too.
- **Unsigned first steps for players**, on the how-to-open page:
  - Windows: the browser's "isn't commonly downloaded" warning first (Edge hides Keep in a menu), then SmartScreen's
    "Windows protected your PC" (More info, Run anyway). Updates the client downloads itself probably skip SmartScreen
    (no Mark-of-the-Web); Phase 4 checks 0.1.0 to 0.1.1. Smart App Control, where on, blocks unsigned apps outright.
    Antivirus false alarms on NSIS installers have been reported.
  - macOS: drag to Applications first (run from the dmg or Downloads, the updater can't replace it), then open, be
    refused, and allow it in System Settings, Privacy & Security, Open Anyway. Needs the ad-hoc signature
    (`"signingIdentity": "-"`), or Apple Silicon says the app "is damaged".
- **Signing, later:** Windows by Azure Artifact Signing (US$9.99 a month; individuals in the US or Canada only), or an
  OV certificate, whose key must sit in hardware since 2023, so from CI a cloud HSM service (likely a few hundred US$
  a year in all; not checked). Apple: the Developer Program, US$99 a year, for Developer ID and notarisation. The
  Microsoft Store would mean MSIX packaging (Tauri bundles MSI and NSIS only) and the Store's updates in place of
  Tauri's.

### Costs

| What | Cost |
|---|---|
| v1 (Tauri, its plugins, this laptop) | Free |
| GitHub Actions builds and Releases | Free while the repo is public (macOS about US$1 a release if it went private) |
| A domain | Roughly €10-20 a year (not checked); needed only at release |
| Windows signing | Nothing at first; US$9.99 a month (US/Canada individuals) or OV plus cloud HSM (a few hundred US$ a year, not checked) |
| Apple | Nothing at first; US$99 a year |
| Update bandwidth | About 5 MB a player a shell release; nothing |

## 11. Decisions for the user

Answered (user, 2026-11-07):
1. **The name:** "Run Against the World". The identifier and the release domain are still to pick, before Phase 4;
   the identifier comes from the domain (v1 uses a placeholder identifier, installed on our own machines only).
2. **Machines to try it on:** a Windows PC and this Fedora laptop; no Mac. So Linux first, Windows next, and macOS
   is built by CI but untested and unlisted until a Mac has run it.

Defaults, unless the user says otherwise:
3. **Wrapper:** Tauri 2, Electron if Phase 0 fails it (D1).
4. **Signing:** unsigned for the first players; Windows signed before open sign-ups; Apple once Mac players matter.
   Where the user is based decides the Windows route (the Azure route is US and Canada only for individuals).
5. **Where the page comes from:** the server (thin).
6. **Connecting:** one built-in address, `--server` for ours.
7. **Remembered:** nothing in v1 but the window; a "remember me" token with doc 77; never the password.
8. **Closing:** asks only in the world.
9. **Reconnecting:** the button now (sign in again); keeping the wolf with doc 77.
10. **Going online:** doc 77 next, starting with a closed beta; never the same-box proxy treated as loopback.
11. **Updates:** GitHub Releases alone.

## Review (2026-11-07)

Three reviews of the first draft (feasibility, simplicity, operations). Each point was checked against the code at
0af9bc0 or the cited source.

Taken:
- **Simpler v1** (simplicity review): v1 is local, 5-7 h instead of 18-21; installers, updates and signing moved to
  Phases 3-4 with doc 77; hosting work listed for doc 77 (5) rather than designed here. No custom bridge commands, so
  no `version.json`, `shellMin` or "out of date" path; one override (`--server`), nothing remembered; one installer
  per OS; the username, the unrelated fixes and the in-page dialogs deferred or made conditional; two questions asked,
  the rest stated as defaults. Not quite 3-5 h: the waiting page, the close question and tests are kept.
- **Remote page permissions** (PR #15266, read): with no custom commands, only Tauri's own window permissions in a
  `remote.urls` capability; added at runtime for `--server`; Phase 0 checks the remote page can call them.
- **CORS** (Core/RatwWeb.cpp:166-171, read): the server check is in Rust; the Reconnect poll is same-origin.
- **Stuck keys** (state.ts:1921, view.ts:164-175, read): Ctrl/Cmd skipped on keydown only; Meta clears held keys;
  checklist rows added.
- **Cmd+Q**: our own Quit item. **Context menu**: page-side, native menu kept. **Close question**: in-page via
  `onCloseRequested`, no blocking native dialog.
- **The phishing override** (both reviews): `--server` only, https or loopback http, nothing remembered.
- **Home-network http** dropped (sign-in is loopback only).
- **Dialog count**: 15 (10 confirm, 5 prompt), counted; the wording about unsupported dialogs corrected.
- **Missed browser features**: the colour pickers (frontDoor.ts:523, 590) and `createImageBitmap` (artwork.ts:49),
  now in the checklist.
- **Fedora tooling**: crates, linuxdeploy, Rust 1.94 and the rustup fallback stated.
- **AppImage carries the runner's WebKitGTK**: the CI-built one is tried here before choosing a Linux format.
- **Phase 0 on Linux only**; CI is Phase 3, with `cargo test` as its own step; Phase 3's time depends on the user's
  machines.
- **Updater, signing and unsigned-download facts**: the updater handles `.deb`/`.rpm` since 2.10.0 (release notes,
  read); `signingIdentity: "-"` (Tauri's macOS signing page, read); updates probably skip SmartScreen (to check);
  browser download warnings, drag to Applications; OV plus cloud HSM for an EU individual; MSIX is extra work.
- **Releases and the box** (Core/RatwGame.cpp:870-888, Server/ratw_server.cpp:297, 411-413, read): the first draft's
  "releases restart when nobody is connected" was wrong for code releases and corrected (4.2). Maintenance deploys,
  versioned `dist`, `stream_close_delay`, Caddy serving files, `LimitNOFILE`, the sign-in storm and the Caddy version
  went to the list for doc 77. Retry jitter is in v1.
- **The proxy trap's fix** must keep the operator's loopback local: both suggestions noted for doc 77; a separate
  proxy port recommended.
- **Window size** on a 1080p screen at 150%: open maximised there; minimum 1024×640.
- **Throttling when minimised**: checked on each OS; `backgroundThrottling` as the fix.
- **Tunnels for the user's own machines only**; D2 no longer offers them as a release path.
- **The client is not a trust boundary**, said in 4.6. The domain's permanence is in Q1 and the risks.
- **The browser claims**: Escape ends only page-requested fullscreen (corrected); a page can block F5/Ctrl+R and ask
  on close, so the page guard helps browsers too.

Rejected, and why:
- **"Vite's default target is Safari 16.0"**: Vite here is 8.3.1, whose default is Safari 16.4
  (Client/node_modules/vite/dist/node/chunks/node.js:699-705). The first draft was right; no `build.target` needed.
- **A `build.rs` AppManifest and four custom calls**: not needed, as there are no custom commands now.
- **A signed `moved` address in `version.json`**: it needs the bridge and versioning v1 dropped; auto-renew and a
  registrar lock instead.
- **Raising `shellMin` for security fixes**: there is no `shellMin`; at release, the updater offered at every launch
  covers security fixes; revisit if a minimum is ever needed.
- **Zero shell calls** (simplicity review): kept four built-in window permissions, because fullscreen that keeps Escape
  and asking only in the world are two of the reasons for the client.
- **Tagging the User-Agent now**: moved to later; it needs a server change and isn't needed while v1 is local.
- **Unchecked claims** (Wails 3's date, the CVE number): removed rather than cited; the Tauri version rule now cites
  PR #15266.
