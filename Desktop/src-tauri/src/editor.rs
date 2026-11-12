// The Editor (Docs/Design/78-hosts-run-their-own-worlds.md, sections 2 and 6.3-6.6; Phase M4): the client's way into a
// world's own tools. The main window's screens (Client/src/ui/frontDoor.ts) ask the shell, through the editor_*
// commands, to connect to a world's Editor port, check its key, sign in and manage Editor accounts. The password is typed
// only in the main window and handed to the shell once; the session token stays here, in memory (I4). The tools open in
// the Editor window, whose every request goes through the `editor` scheme (editor/proxy.rs) with the token added here.
//
//   editor/tls.rs      the pinned TLS 1.3 client (its verifier and record mode, ureq's with_parts)
//   editor/wire.rs     addresses, the hello and Editor codes, checked
//   editor/pins.rs     the pin store (by key, addresses as aliases), signed rotation, the red screen's cases
//   editor/devices.rs  device tokens in the keychain
//   editor/bundle.rs   the signed Editor bundle and its release key
//   editor/proxy.rs    the `editor` scheme's rules
//
// The Editor window (label `editor`) gets no capability at all: Tauri files the scheme's origin under "URL: local", so a
// capability naming it would grant it the shell's commands (M0a). It belongs to one world, one account and one session
// (I10): signing in to another world or as another account destroys it, and it gets a new data directory for every
// window, deleted after it closes (WebKitGTK's incognito windows never reach a custom scheme, M0a). Its navigation is
// kept to the scheme's origin (lib.rs, page_origin_for), it opens no other window, and its downloads need an allowed
// extension and go through the shell's save dialog.
pub mod bundle;
pub mod devices;
pub mod pins;
pub mod proxy;
pub mod tls;
pub mod wire;

use std::collections::HashMap;
use std::path::PathBuf;
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use serde_json::{json, Value};
use tauri::{AppHandle, Emitter, Manager, State};

use bundle::Manifest;
use pins::{PinStore, Verdict};
use proxy::Binding;
use tls::{fingerprint, sha256, Mode, Pin};
use wire::{Address, Code, Hello, Place};

/// Whether this platform has the Editor: not macOS until it has been tried on WKWebView (doc 78, Q26).
pub const AVAILABLE: bool = !cfg!(target_os = "macos");
const KEEPALIVE_EVERY: Duration = Duration::from_secs(5 * 60);
const INPUT_COUNTS_FOR: Duration = Duration::from_secs(30 * 60);
const DOWNLOADS: [&str; 3] = ["json", "png", "csv"];
const CALL_MOST: u64 = 256 * 1024;

/// What the last connection to an address found (until the host trusts it, forgets an old key, or connects again).
struct Connected {
    spki: Vec<u8>,
    hello: Hello,
    verdict: Verdict,
}

#[derive(Clone)]
struct Session {
    generation: u64,
    address: Address,
    place: Place,
    base: String,
    pin: Pin,
    world: String,
    name: String,
    username: String,
    role: String,
    must_change: bool,
    /// None once the world said the session is over: the window stays bound, for the same account to sign in again.
    token: Option<String>,
    /// The one-time password this session signed in with, while it must be changed (screen 4b asks only for the new
    /// one; from an Editor code the page never had it). Dropped once changed.
    one_time: Option<String>,
    agent: ureq::Agent,
    manifest: Option<Arc<Manifest>>,
}

struct Window {
    generation: u64,
    world: String,
    username: String,
}

#[derive(Default)]
struct Inner {
    session: Option<Session>,
    window: Option<Window>,
    connects: HashMap<String, Connected>,
    code: Option<Code>,
    generation: u64,
    keepalive_at: Option<Instant>,
}

pub struct Editor {
    folder: PathBuf,
    pins: PinStore,
    devices: devices::Devices,
    inner: Mutex<Inner>,
    last_input: Mutex<Option<Instant>>,
}

fn refused(error: &str) -> Value {
    json!({"ok": false, "error": error})
}

/// A world's refusal, passed on as its code and the numbers the page words (never the world's own sentence: I12).
fn from_world(status: u16, body: &Value) -> Value {
    let code = body.get("error").and_then(Value::as_str).filter(|c| c.len() <= 40 && c.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'-'))
        .unwrap_or("unknown");
    let mut out = json!({"ok": false, "status": status, "error": code});
    if let Some(m) = body.get("minutes").and_then(Value::as_u64) {
        out["minutes"] = json!(m.min(24 * 60));
    }
    if let Some(r) = body.get("reason").and_then(Value::as_str).filter(|r| r.len() <= 20 && r.bytes().all(|b| b.is_ascii_lowercase())) {
        out["reason"] = json!(r);
    }
    out
}

/// One JSON exchange with a world's Editor routes: (status, body); status 0 when it couldn't be reached.
fn call(agent: &ureq::Agent, base: &str, method: &str, path: &str, token: Option<&str>, body: Option<&Value>) -> (u16, Value) {
    let url = format!("{base}{path}");
    let bearer = token.map(|t| format!("Bearer {t}"));
    let result = match method {
        "GET" => {
            let mut r = agent.get(&url);
            if let Some(b) = &bearer {
                r = r.header("Authorization", b);
            }
            r.call()
        }
        _ => {
            let mut r = agent.post(&url).header("Content-Type", "application/json");
            if let Some(b) = &bearer {
                r = r.header("Authorization", b);
            }
            r.send(serde_json::to_vec(body.unwrap_or(&json!({}))).unwrap_or_default())
        }
    };
    match result {
        Ok(mut response) => {
            let status = response.status().as_u16();
            let value = response.body_mut().with_config().limit(CALL_MOST).read_to_vec().ok()
                .and_then(|b| serde_json::from_slice::<Value>(&b).ok()).filter(Value::is_object).unwrap_or_else(|| json!({}));
            (status, value)
        }
        Err(e) => {
            eprintln!("ratw-desktop: the Editor: {} {path}: {}", method, tls::error_text(&e));
            (0, json!({"error": "unreachable"}))
        }
    }
}

/// The Editor window's origin: `editor://localhost` (WebKitGTK, WKWebView), `http://editor.localhost` (WebView2). Never a
/// port, a user or another host.
pub fn editor_origin_on(url: &tauri::Url, windows: bool) -> bool {
    let (scheme, host) = if windows { ("http", "editor.localhost") } else { ("editor", "localhost") };
    url.scheme() == scheme && url.host_str() == Some(host) && url.port().is_none() && url.username().is_empty() && url.password().is_none()
}

pub fn editor_origin(url: &tauri::Url) -> bool {
    editor_origin_on(url, cfg!(windows))
}

fn editor_url(path: &str) -> tauri::Url {
    let origin = if cfg!(windows) { "http://editor.localhost" } else { "editor://localhost" };
    format!("{origin}{path}").parse().expect("the Editor's own URL")
}

/// A download the Editor window may save (after the save dialog): one of the allowed extensions, by its own name.
pub fn download_allowed(name: &str) -> bool {
    name.rsplit_once('.').is_some_and(|(stem, ext)| !stem.is_empty() && DOWNLOADS.contains(&ext.to_ascii_lowercase().as_str()))
}

impl Editor {
    pub fn new(folder: PathBuf, token_store: &str, memory_pins: bool) -> Self {
        // Window data directories left by a run that ended without cleaning up.
        let _ = std::fs::remove_dir_all(folder.join("editor-windows"));
        let pins = if memory_pins { PinStore::memory() } else { PinStore::open(folder.clone()) };
        Editor { devices: devices::Devices::new(token_store, folder.clone()), folder, pins, inner: Mutex::new(Inner::default()), last_input: Mutex::new(None) }
    }

    fn session_view(s: &Session) -> Value {
        json!({"world": s.world, "name": s.name, "address": s.address.shown, "place": s.place, "username": s.username, "role": s.role,
            "mustChange": s.must_change, "live": s.token.is_some(), "fingerprint": fingerprint(&s.pin)})
    }

    /// The menu's view: whether the Editor is here at all, the session (never its token), the window, recent worlds.
    pub fn state(&self) -> Value {
        let inner = self.inner.lock().unwrap();
        json!({"available": AVAILABLE, "session": inner.session.as_ref().map(Self::session_view), "window": inner.window.is_some(),
            "recent": self.pins.read(|f| f.recent())})
    }

    // ------------------------------------------------------------------------------------------------- connecting

    /// Screen 3: the hello over a first, recording connection, and what its key means.
    pub fn connect(&self, given: &str) -> Value {
        if !AVAILABLE {
            return refused("unavailable");
        }
        let address = match wire::parse_address(given) {
            Ok(a) => a,
            Err(_) => return refused("address"),
        };
        let sockets = match wire::resolve(&address) {
            Ok(s) => s,
            Err("refused") => return refused("address"),
            Err(_) => return json!({"ok": false, "error": "unresolved", "address": address.shown}),
        };
        let place = wire::place(sockets[0].ip());
        let base = wire::base_url(&sockets[0]);
        // Record mode: any key, recorded; only /editor/hello, nothing secret (no username, device token or code).
        let (agent, verifier) = tls::agent(Mode::Record, Duration::from_secs(15));
        let reply = agent.get(&format!("{base}/editor/hello")).call();
        let response = match reply {
            Ok(r) => r,
            Err(e) => {
                let nothing = tls::unreachable(&e);
                eprintln!("ratw-desktop: the Editor at {}: {}", address.shown, tls::error_text(&e));
                // Something answered, not as an Editor: perhaps the game's port (that check stores nothing).
                let ip = sockets[0].ip().to_string();
                if !nothing && crate::central::world_status(&ip, address.port, false)["status"].as_u64().unwrap_or(0) != 0 {
                    return json!({"ok": false, "error": "game-port", "address": address.shown, "place": place});
                }
                if nothing {
                    let game = address.port != wire::GAME_PORT
                        && crate::central::world_status(&ip, wire::GAME_PORT, false)["status"].as_u64().unwrap_or(0) != 0;
                    return json!({"ok": false, "error": "unreachable", "address": address.shown, "place": place, "gameAnswers": game});
                }
                return json!({"ok": false, "error": "not-editor", "address": address.shown, "place": place});
            }
        };
        let status = response.status().as_u16();
        let mut response = response;
        let body = response.body_mut().with_config().limit(wire::HELLO_MOST as u64 + 1).read_to_vec().unwrap_or_default();
        let hello = match (status, wire::read_hello(&body)) {
            (200, Ok(h)) => h,
            _ => return json!({"ok": false, "error": "not-editor", "address": address.shown, "place": place}),
        };
        let Some(spki) = verifier.seen() else { return refused("not-editor") };
        if hello.editor_api != wire::EDITOR_API {
            let error = if hello.editor_api > wire::EDITOR_API { "client-old" } else { "package-old" };
            return json!({"ok": false, "error": error, "address": address.shown, "place": place});
        }
        if hello.official {
            return json!({"ok": false, "error": "official", "address": address.shown, "place": place});
        }
        let pin = sha256(&spki);
        let verdict = self.pins.read(|f| pins::classify(f, &address.shown, &spki, &hello));
        // A key pinned from an Editor code, met for the first time: no "now at" notice, it was never anywhere else.
        let first_met = verdict == Verdict::Moved && self.pins.read(|f| f.by_pin(&pin).is_some_and(|k| k.addresses.is_empty()));
        self.pins.change(|f| f.apply(&verdict, &address.shown, &spki, &hello));
        let (known_user, previous) = match &verdict {
            Verdict::Changed { old } | Verdict::Rotated { old } => (String::new(), tls::pin_from_text(&old.pin).map(|p| fingerprint(&p))),
            _ => (self.pins.read(|f| f.by_pin(&pin).map(|k| k.username.clone())).unwrap_or_default(), None),
        };
        let username = {
            let inner = self.inner.lock().unwrap();
            match &inner.code {
                Some(c) if c.world == hello.world && c.pin == pin => c.username.clone(),
                _ => known_user,
            }
        };
        let out = json!({"ok": true, "state": if first_met { "trusted" } else { verdict.word() }, "address": address.shown, "place": place, "fingerprint": fingerprint(&pin),
            "previous": previous, "hello": hello.page(), "username": username,
            "code": self.inner.lock().unwrap().code.as_ref().is_some_and(|c| c.world == hello.world && c.pin == pin)});
        self.inner.lock().unwrap().connects.insert(address.shown.clone(), Connected { spki, hello, verdict });
        out
    }

    /// Screen 2's Editor code: the key it carries is pinned now, its secrets kept here (never the page) for the sign-in.
    pub fn use_code(&self, text: &str) -> Value {
        let code = match wire::read_code(text) {
            Ok(c) => c,
            Err(_) => return refused("code"),
        };
        self.pins.change(|f| f.pin_code(&code.world, &code.pin, &code.username, code.address.as_ref().map(|a| a.shown.as_str())));
        let out = json!({"ok": true, "address": code.address.as_ref().map(|a| a.shown.clone()), "username": code.username,
            "world": code.world, "fingerprint": fingerprint(&code.pin), "password": code.password.is_some()});
        self.inner.lock().unwrap().code = Some(code);
        out
    }

    /// Screen 3's TRUST AND CONTINUE: the key the last connection to this address showed, if it was new and the
    /// fingerprint shown is the one trusted.
    pub fn trust(&self, given: &str, shown: &str) -> Value {
        let Ok(address) = wire::parse_address(given) else { return refused("address") };
        let mut inner = self.inner.lock().unwrap();
        let Some(c) = inner.connects.get(&address.shown) else { return refused("connect-first") };
        if c.verdict != Verdict::New || fingerprint(&sha256(&c.spki)) != shown.trim() {
            return refused("connect-first");
        }
        self.pins.change(|f| f.trust(&address.shown, &c.spki, &c.hello));
        inner.connects.remove(&address.shown);
        json!({"ok": true})
    }

    /// The red screen's FORGET THE OLD CERTIFICATE: the old key, only with the world's name typed.
    pub fn forget(&self, given: &str, typed: &str) -> Value {
        let Ok(address) = wire::parse_address(given) else { return refused("address") };
        let mut inner = self.inner.lock().unwrap();
        let Some(Connected { verdict: Verdict::Changed { old }, .. }) = inner.connects.get(&address.shown) else { return refused("connect-first") };
        if !self.pins.change(|f| f.forget(&old.pin, typed)) {
            return refused("name");
        }
        inner.connects.remove(&address.shown);
        json!({"ok": true})
    }

    /// Recent worlds' FORGET: the world's key and every address it had.
    pub fn forget_recent(&self, pin: &str) -> Value {
        json!({"ok": self.pins.change(|f| f.remove(pin))})
    }

    // ------------------------------------------------------------------------------------------------- signing in

    /// Screen 4: signs in over the pinned key with this account's device token. One Editor session at a time: another
    /// world or another account signs the old one out and destroys its window (I10); the same account on the same world
    /// hands the open window the new session.
    pub fn sign_in(&self, app: Option<&AppHandle>, given: &str, username: &str, password: &str, with_code: bool) -> Value {
        if !AVAILABLE {
            return refused("unavailable");
        }
        let Ok(address) = wire::parse_address(given) else { return refused("address") };
        let username = username.trim().to_lowercase();
        if !wire::username_ok(&username) {
            return json!({"ok": false, "status": 401, "error": "wrong"});
        }
        let Some(known) = self.pins.read(|f| f.by_address(&address.shown).cloned()) else { return refused("connect-first") };
        let Some(pin) = pins::PinFile::pin_of(&known) else { return refused("connect-first") };
        let (mut secret, invite) = (password.to_string(), None::<String>);
        let mut invite = invite;
        if with_code {
            let inner = self.inner.lock().unwrap();
            match &inner.code {
                Some(c) if c.world == known.world && c.pin == pin && c.username == username => {
                    if secret.is_empty() {
                        secret = c.password.clone().unwrap_or_default();
                    }
                    invite = c.invite.clone();
                }
                _ => return refused("code"),
            }
        }
        if secret.is_empty() || secret.len() > 1024 {
            return json!({"ok": false, "status": 401, "error": "wrong"});
        }
        let sockets = match wire::resolve(&address) {
            Ok(s) => s,
            Err(_) => return refused("unreachable"),
        };
        let base = wire::base_url(&sockets[0]);
        let place = wire::place(sockets[0].ip());
        let (agent, _) = tls::agent(Mode::Exact(pin), Duration::from_secs(60));
        let device = self.devices.load(&known.world, &username);
        let mut body = json!({"username": username, "password": secret});
        if let Some(i) = &invite {
            body["invite"] = json!(i);
        }
        if let Some(d) = &device {
            body["device"] = json!(d);
        }
        let (status, reply) = call(&agent, &base, "POST", "/editor/login", None, Some(&body));
        if status == 0 {
            return refused("unreachable");
        }
        if status != 200 {
            return from_world(status, &reply);
        }
        let token = reply["token"].as_str().unwrap_or_default().to_string();
        let role = reply["role"].as_str().filter(|r| ["viewer", "dm", "builder", "owner"].contains(r)).unwrap_or("viewer").to_string();
        let must_change = reply["mustChange"].as_bool().unwrap_or(false);
        if token.is_empty() {
            return refused("not-editor");
        }
        if let Some(d) = reply["device"].as_str() {
            self.devices.save(&known.world, &username, d);
        }
        self.pins.change(|f| f.signed_in(&pin, &username));
        let mut old = None;
        let (view, close_window) = {
            let mut inner = self.inner.lock().unwrap();
            if with_code {
                inner.code = None;
            }
            inner.generation += 1;
            let generation = inner.generation;
            let one_time = must_change.then(|| secret.clone());
            let session = Session { generation, address, place, base, pin, world: known.world.clone(), name: known.name.clone(), username: username.clone(),
                role, must_change, token: Some(token), one_time, agent, manifest: None };
            let view = Self::session_view(&session);
            if let Some(previous) = inner.session.take() {
                if previous.world != session.world || previous.username != session.username {
                    old = Some(previous);
                }
            }
            let close = match &mut inner.window {
                Some(w) if w.world == session.world && w.username == session.username => {
                    w.generation = generation;               // The same account on the same world: the window keeps going.
                    false
                }
                Some(_) => true,
                None => false,
            };
            if close {
                inner.window = None;
            }
            inner.session = Some(session);
            inner.keepalive_at = Some(Instant::now());
            (view, close)
        };
        if let Some(previous) = old {
            if let Some(t) = &previous.token {
                call(&previous.agent, &previous.base, "POST", "/editor/logout", Some(t), None);
            }
        }
        if close_window {
            if let Some(app) = app {
                close_editor_window(app);
            }
        }
        json!({"ok": true, "session": view})
    }

    /// The current session's agent, base and token, or the page's refusal.
    fn live(&self) -> Result<(ureq::Agent, String, String, u64), Value> {
        let inner = self.inner.lock().unwrap();
        match &inner.session {
            Some(Session { token: Some(t), agent, base, generation, .. }) => Ok((agent.clone(), base.clone(), t.clone(), *generation)),
            Some(_) => Err(json!({"ok": false, "status": 401, "error": "ended"})),
            None => Err(json!({"ok": false, "status": 401, "error": "signed-out"})),
        }
    }

    /// A call on the session for the main window's screens. A 401 ends the session here too.
    fn session_call(&self, app: Option<&AppHandle>, method: &str, path: &str, body: Option<&Value>) -> (u16, Value) {
        let (agent, base, token, generation) = match self.live() {
            Ok(l) => l,
            Err(e) => return (401, e),
        };
        let (status, reply) = call(&agent, &base, method, path, Some(&token), body);
        if status == 401 && !matches!(reply["error"].as_str(), Some("expired")) {
            self.ended(app, generation);
        }
        (status, reply)
    }

    /// Screen 4b, and the menu's Change my password.
    pub fn change_password(&self, app: Option<&AppHandle>, old: &str, new: &str) -> Value {
        let kept = self.inner.lock().unwrap().session.as_ref().and_then(|s| s.one_time.clone());
        let old = if old.is_empty() { kept.as_deref().unwrap_or("") } else { old };
        if old.is_empty() || new.is_empty() || old.len() > 1024 || new.len() > 1024 {
            return json!({"ok": false, "status": 400, "error": "bad-request"});
        }
        let (status, reply) = self.session_call(app, "POST", "/editor/password", Some(&json!({"old": old, "new": new})));
        if status != 200 {
            return if status == 0 { refused("unreachable") } else { from_world(status, &reply) };
        }
        let mut inner = self.inner.lock().unwrap();
        if let Some(s) = inner.session.as_mut() {
            s.must_change = false;
            s.one_time = None;
            if let Some(d) = reply["device"].as_str() {
                self.devices.save(&s.world, &s.username, d);
            }
            if let Some(r) = reply["role"].as_str().filter(|r| ["viewer", "dm", "builder", "owner"].contains(r)) {
                s.role = r.to_string();
            }
            return json!({"ok": true, "session": Self::session_view(s)});
        }
        refused("signed-out")
    }

    /// The accounts panel's step-up: 5 minutes, bound to this session, asked in the main window only.
    pub fn step_up(&self, app: Option<&AppHandle>, password: &str) -> Value {
        if password.is_empty() || password.len() > 1024 {
            return json!({"ok": false, "status": 403, "error": "wrong-password"});
        }
        let (status, reply) = self.session_call(app, "POST", "/editor/step-up", Some(&json!({"password": password})));
        match status {
            200 => json!({"ok": true, "minutes": reply["minutes"].as_u64().unwrap_or(5)}),
            0 => refused("unreachable"),
            _ => from_world(status, &reply),
        }
    }

    /// The accounts panel (owner, after a step-up): the list, and add, reset, role, wolves, disable, enable, remove,
    /// end-sessions. Only the fields each needs are sent on; a code the world makes comes back once, with this world's
    /// address added when it isn't this computer's.
    pub fn accounts(&self, app: Option<&AppHandle>, verb: &str, body: &Value) -> Value {
        const VERBS: [&str; 9] = ["list", "add", "reset", "role", "wolves", "disable", "enable", "remove", "end-sessions"];
        if !VERBS.contains(&verb) {
            return refused("bad-request");
        }
        let (status, reply) = if verb == "list" {
            self.session_call(app, "GET", "/editor/accounts", None)
        } else {
            let mut sent = serde_json::Map::new();
            for (k, ok) in [("username", true), ("role", verb == "add" || verb == "role"), ("mayActOnWolves", verb == "add"), ("allowed", verb == "wolves")] {
                if let Some(v) = body.get(k).filter(|_| ok) {
                    if v.is_string() && v.as_str().is_some_and(|s| s.len() <= 64) || v.is_boolean() {
                        sent.insert(k.into(), v.clone());
                    }
                }
            }
            self.session_call(app, "POST", &format!("/editor/accounts/{verb}"), Some(&Value::Object(sent)))
        };
        if status != 200 {
            return if status == 0 { refused("unreachable") } else { from_world(status, &reply) };
        }
        let mut out = json!({"ok": true});
        if verb == "list" {
            out["accounts"] = reply.get("accounts").cloned().unwrap_or(json!([]));
            out["failedToday"] = reply.get("failedToday").cloned().unwrap_or(json!(0));
            out["banners"] = reply.get("banners").cloned().unwrap_or(json!([]));
        }
        if let Some(code) = reply.get("code").and_then(Value::as_str) {
            let inner = self.inner.lock().unwrap();
            let with = match &inner.session {
                Some(s) => wire::code_with_address(code, &s.address, s.place),
                None => code.to_string(),
            };
            out["code"] = json!(with);
        }
        out
    }

    /// Sign out: the world forgets the session, the shell its token, and the window goes with it.
    pub fn sign_out(&self, app: Option<&AppHandle>) -> Value {
        let session = {
            let mut inner = self.inner.lock().unwrap();
            inner.window = None;
            inner.keepalive_at = None;
            inner.session.take()
        };
        if let Some(s) = session {
            if let Some(t) = &s.token {
                call(&s.agent, &s.base, "POST", "/editor/logout", Some(t), None);
            }
        }
        if let Some(app) = app {
            close_editor_window(app);
        }
        json!({"ok": true})
    }

    /// The menu's status pill: the world's hello again, over the pinned key.
    pub fn status(&self) -> Value {
        let (agent, base) = {
            let inner = self.inner.lock().unwrap();
            match &inner.session {
                Some(s) => (s.agent.clone(), s.base.clone()),
                None => return refused("signed-out"),
            }
        };
        let reply = agent.get(&format!("{base}/editor/hello")).call();
        match reply {
            Ok(mut r) if r.status().as_u16() == 200 => {
                let body = r.body_mut().with_config().limit(wire::HELLO_MOST as u64 + 1).read_to_vec().unwrap_or_default();
                match wire::read_hello(&body) {
                    Ok(h) => json!({"ok": true, "hello": h.page()}),
                    Err(_) => refused("not-editor"),
                }
            }
            Ok(_) => refused("not-editor"),
            Err(_) => refused("unreachable"),
        }
    }

    /// The world said the session is over: the token goes; the window stays bound, for the same account.
    fn ended(&self, app: Option<&AppHandle>, generation: u64) {
        let view = {
            let mut inner = self.inner.lock().unwrap();
            match inner.session.as_mut() {
                Some(s) if s.generation == generation && s.token.is_some() => {
                    s.token = None;
                    s.manifest = None;
                    Some(json!({"state": "ended", "world": s.world, "username": s.username}))
                }
                _ => None,
            }
        };
        if let (Some(view), Some(app)) = (view, app) {
            let _ = app.emit_to("main", "editor-session", view);
        }
    }

    // ------------------------------------------------------------------------------------------------- the window

    /// The signed bundle for this session: fetched once, checked against the release keys.
    fn manifest(&self) -> Result<Arc<Manifest>, Value> {
        let (agent, base, token, generation) = self.live()?;
        if let Some(m) = self.inner.lock().unwrap().session.as_ref().and_then(|s| s.manifest.clone()) {
            return Ok(m);
        }
        let world = proxy::World { agent, base, token };
        let fetched = proxy::Upstream::send(&world, "GET", "/bundle.json", None, None, None).map_err(|_| refused("unreachable"))?;
        if fetched.status == 401 {
            self.ended(None, generation);
            return Err(json!({"ok": false, "status": 401, "error": "ended"}));
        }
        let body = proxy::decoded(&fetched).map_err(|_| refused("unsigned"))?;
        let manifest = match (fetched.status, bundle::verify(&body, &bundle::trusted_keys(), bundle::OLDEST, wire::EDITOR_API)) {
            (200, Ok(m)) => Arc::new(m),
            (_, Err(why)) => {
                eprintln!("ratw-desktop: the world's Editor bundle was refused: {why:?}");
                return Err(refused("unsigned"));
            }
            _ => return Err(refused("unsigned")),
        };
        let mut inner = self.inner.lock().unwrap();
        if let Some(s) = inner.session.as_mut().filter(|s| s.generation == generation) {
            s.manifest = Some(manifest.clone());
        }
        Ok(manifest)
    }

    /// The `editor` scheme: one request from a webview.
    pub fn proxy(&self, app: &AppHandle, label: &str, request: &tauri::http::Request<Vec<u8>>) -> tauri::http::Response<Vec<u8>> {
        let asked = proxy::asked(request);
        #[cfg(feature = "e2e")]
        if label == "editor" && asked.method == "POST" && asked.path == proxy::E2E_PATH {
            crate::e2e::reply(app, &String::from_utf8_lossy(&asked.body));
            return proxy::response(proxy::Answer { status: 204, content_type: "application/json", body: vec![], ended: false, input: false });
        }
        let (binding, upstream, manifest, generation) = {
            let inner = self.inner.lock().unwrap();
            match (&inner.session, &inner.window) {
                (Some(s), Some(w)) if label == "editor" && w.world == s.world && w.username == s.username => {
                    match (&s.token, w.generation == s.generation) {
                        (Some(t), true) => (Binding::Current, Some(proxy::World { agent: s.agent.clone(), base: s.base.clone(), token: t.clone() }),
                            s.manifest.clone(), s.generation),
                        _ => (Binding::Ended, None, None, s.generation),
                    }
                }
                _ => (Binding::Stale, None, None, 0),
            }
        };
        struct Nowhere;
        impl proxy::Upstream for Nowhere {
            fn send(&self, _: &str, _: &str, _: Option<&str>, _: Option<&str>, _: Option<&[u8]>) -> Result<proxy::Fetched, String> {
                Err("no session".into())
            }
        }
        // A session signed in again in place (the same account, the window kept): its signed list, fetched once.
        let manifest = match (&upstream, manifest) {
            (Some(_), None) => self.manifest().ok(),
            (_, m) => m,
        };
        let answer = match &upstream {
            Some(u) => proxy::handle(label, binding, &asked, manifest.as_deref(), u),
            None => proxy::handle(label, binding, &asked, None, &Nowhere),
        };
        if answer.input {
            *self.last_input.lock().unwrap() = Some(Instant::now());
        }
        if answer.ended && binding == Binding::Current {
            self.ended(Some(app), generation);
        }
        if answer.status >= 400 && label == "editor" {
            eprintln!("ratw-desktop: the Editor window: {} {} -> {}", asked.method, asked.path.split('?').next().unwrap_or(""), answer.status);
        }
        proxy::response(answer)
    }

    /// The keep-alive (every 5 minutes, only after the host's input in the Editor window in the last 30 minutes, so a
    /// window left alone still times out). Called about every 30 seconds.
    pub fn tick(&self, app: Option<&AppHandle>) {
        let due = {
            let inner = self.inner.lock().unwrap();
            let input = self.last_input.lock().unwrap().is_some_and(|t| t.elapsed() < INPUT_COUNTS_FOR);
            inner.session.as_ref().is_some_and(|s| s.token.is_some() && !s.must_change && inner.window.is_some())
                && input && inner.keepalive_at.is_none_or(|t| t.elapsed() >= KEEPALIVE_EVERY)
        };
        if !due {
            return;
        }
        self.inner.lock().unwrap().keepalive_at = Some(Instant::now());
        self.session_call(app, "POST", "/editor/keepalive", None);
    }

    /// At exit: every Editor window's data directory (the windows are gone by then).
    pub fn clean_up(&self) {
        let _ = std::fs::remove_dir_all(self.folder.join("editor-windows"));
    }

    fn window_title(&self) -> Option<String> {
        let inner = self.inner.lock().unwrap();
        inner.session.as_ref().map(|s| format!("RATW Editor: {} ({})", if s.name.is_empty() { "a world" } else { &s.name }, s.address.shown))
    }
}

/// Destroys the Editor window, if there is one (its data directory goes when it is gone).
fn close_editor_window(app: &AppHandle) {
    if let Some(w) = app.get_webview_window("editor") {
        let _ = w.destroy();
    }
}

/// The window's data directory, removed once its webview has let go of it. (WebView2's processes exit a moment later;
/// WebKitGTK's write a few files again as they close, so it is removed until it has stayed gone for three seconds.)
fn remove_later(dir: PathBuf) {
    std::thread::spawn(move || {
        let mut gone_for = 0;
        for _ in 0..120 {
            if dir.exists() {
                gone_for = 0;
                let _ = std::fs::remove_dir_all(&dir);
            } else {
                gone_for += 1;
                if gone_for >= 6 {
                    return;
                }
            }
            std::thread::sleep(Duration::from_millis(500));
        }
        eprintln!("ratw-desktop: the Editor window's data directory {} could not be removed", dir.display());
    });
}

fn random_hex() -> String {
    let mut bytes = [0u8; 12];
    let _ = getrandom::fill(&mut bytes);
    tls::hex(&bytes)
}

// ----------------------------------------------------------------------------------------------------------- commands
// Granted to the main window only (capabilities/default.json). Async, on the blocking pool: they wait on the network.

type Ed<'a> = State<'a, Arc<Editor>>;

async fn blocking<F: FnOnce() -> Value + Send + 'static>(f: F) -> Result<Value, String> {
    tauri::async_runtime::spawn_blocking(f).await.map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn editor_state(editor: Ed<'_>) -> Result<Value, String> {
    Ok(editor.state())
}

#[tauri::command]
pub async fn editor_connect(address: String, editor: Ed<'_>) -> Result<Value, String> {
    let e = editor.inner().clone();
    blocking(move || e.connect(&address)).await
}

#[tauri::command]
pub async fn editor_use_code(code: String, editor: Ed<'_>) -> Result<Value, String> {
    Ok(editor.use_code(&code))
}

#[tauri::command]
pub async fn editor_trust(address: String, fingerprint: String, editor: Ed<'_>) -> Result<Value, String> {
    Ok(editor.trust(&address, &fingerprint))
}

#[tauri::command]
pub async fn editor_forget(address: String, name: String, pin: Option<String>, editor: Ed<'_>) -> Result<Value, String> {
    Ok(match pin {
        Some(p) => editor.forget_recent(&p),
        None => editor.forget(&address, &name),
    })
}

#[tauri::command]
pub async fn editor_sign_in(address: String, username: String, password: Option<String>, code: Option<bool>, app: AppHandle, editor: Ed<'_>) -> Result<Value, String> {
    let e = editor.inner().clone();
    blocking(move || e.sign_in(Some(&app), &address, &username, password.as_deref().unwrap_or(""), code.unwrap_or(false))).await
}

#[tauri::command]
pub async fn editor_change_password(old: String, new: String, app: AppHandle, editor: Ed<'_>) -> Result<Value, String> {
    let e = editor.inner().clone();
    blocking(move || e.change_password(Some(&app), &old, &new)).await
}

#[tauri::command]
pub async fn editor_step_up(password: String, app: AppHandle, editor: Ed<'_>) -> Result<Value, String> {
    let e = editor.inner().clone();
    blocking(move || e.step_up(Some(&app), &password)).await
}

#[tauri::command]
pub async fn editor_accounts(verb: String, body: Option<Value>, app: AppHandle, editor: Ed<'_>) -> Result<Value, String> {
    let e = editor.inner().clone();
    blocking(move || e.accounts(Some(&app), &verb, &body.unwrap_or(Value::Null))).await
}

#[tauri::command]
pub async fn editor_sign_out(app: AppHandle, editor: Ed<'_>) -> Result<Value, String> {
    let e = editor.inner().clone();
    blocking(move || e.sign_out(Some(&app))).await
}

#[tauri::command]
pub async fn editor_status(editor: Ed<'_>) -> Result<Value, String> {
    let e = editor.inner().clone();
    blocking(move || e.status()).await
}

/// Screen 6: the Editor window on DM or Atlas, made for this session (or this session's, switched).
#[tauri::command]
pub async fn editor_open(suite: String, app: AppHandle, editor: Ed<'_>) -> Result<Value, String> {
    if suite != "dm" && suite != "atlas" {
        return Ok(refused("bad-request"));
    }
    let e = editor.inner().clone();
    {
        let inner = e.inner.lock().unwrap();
        match &inner.session {
            Some(s) if s.token.is_none() => return Ok(json!({"ok": false, "status": 401, "error": "ended"})),
            Some(s) if s.must_change => return Ok(refused("must-change")),
            Some(_) => {}
            None => return Ok(refused("signed-out")),
        }
    }
    let checked = { let e = e.clone(); blocking(move || match e.manifest() { Ok(_) => json!({"ok": true}), Err(v) => v }).await? };
    if checked["ok"] != true {
        return Ok(checked);
    }
    let url = editor_url(&format!("/?suite={suite}"));
    let title = e.window_title().unwrap_or_else(|| "RATW Editor".into());
    *e.last_input.lock().unwrap() = Some(Instant::now());       // (Opening it is the host's input.)
    let bound = {
        let inner = e.inner.lock().unwrap();
        matches!((&inner.session, &inner.window), (Some(s), Some(w)) if w.world == s.world && w.username == s.username && w.generation == s.generation)
    };
    if let Some(window) = app.get_webview_window("editor") {
        if bound {
            let _ = window.navigate(url);
            let _ = window.set_title(&title);
            let _ = window.unminimize();
            let _ = window.set_focus();
            return Ok(json!({"ok": true}));
        }
        let _ = window.destroy();
    }
    let dir = e.folder.join("editor-windows").join(random_hex());
    let (generation, world, username) = {
        let inner = e.inner.lock().unwrap();
        let s = inner.session.as_ref().ok_or("signed out")?;
        (s.generation, s.world.clone(), s.username.clone())
    };
    let made = tauri::WebviewWindowBuilder::new(&app, "editor", tauri::WebviewUrl::CustomProtocol(url))
        .title(&title)
        .inner_size(1280.0, 800.0)
        .min_inner_size(900.0, 600.0)
        .data_directory(dir.clone())
        .initialization_script(proxy::INPUT_SCRIPT)
        .on_navigation(|url| {
            let ok = editor_origin(url) || url.as_str() == "about:blank";
            if !ok {
                eprintln!("ratw-desktop: the Editor window may not go to {}", url.scheme());
            }
            ok
        })
        .on_new_window(|_, _| tauri::webview::NewWindowResponse::Deny)
        .on_download(|_, event| match event {
            tauri::webview::DownloadEvent::Requested { url, destination } => {
                let name = destination.file_name().and_then(|n| n.to_str()).map(str::to_string)
                    .or_else(|| url.path_segments().and_then(|mut s| s.next_back().map(str::to_string))).unwrap_or_default();
                let name: String = name.chars().filter(|c| c.is_ascii_alphanumeric() || matches!(c, '.' | '-' | '_')).take(80).collect();
                if !download_allowed(&name) {
                    eprintln!("ratw-desktop: the Editor window's download was refused (not .json, .png or .csv)");
                    return false;
                }
                // Always through the shell's save dialog: the host picks where, or nothing is saved.
                match rfd::FileDialog::new().set_title("Save from the Editor").set_file_name(&name).save_file() {
                    Some(path) if download_allowed(path.file_name().and_then(|n| n.to_str()).unwrap_or("")) => {
                        *destination = path;
                        true
                    }
                    _ => false,
                }
            }
            _ => true,
        })
        .build();
    let window = match made {
        Ok(w) => w,
        Err(err) => {
            eprintln!("ratw-desktop: the Editor window could not be opened: {err}");
            remove_later(dir);
            return Ok(refused("window"));
        }
    };
    e.inner.lock().unwrap().window = Some(Window { generation, world, username });
    let watched = e.clone();
    window.on_window_event(move |event| {
        if let tauri::WindowEvent::Destroyed = event {
            let mut inner = watched.inner.lock().unwrap();
            if inner.window.as_ref().is_some_and(|w| w.generation <= inner.session.as_ref().map_or(u64::MAX, |s| s.generation) && w.generation == generation) {
                inner.window = None;
            }
            drop(inner);
            remove_later(dir.clone());
        }
    });
    Ok(json!({"ok": true}))
}

/// The commands' names, for build.rs and the capability (the main window's only).
pub const COMMANDS: &[&str] = &["editor_state", "editor_connect", "editor_use_code", "editor_trust", "editor_forget", "editor_sign_in",
    "editor_change_password", "editor_step_up", "editor_accounts", "editor_sign_out", "editor_status", "editor_open"];

#[cfg(test)]
mod tests {
    use super::*;
    use tls::pin_text;
    use tls::tests::{pin_a, server_config, Reply, Seen, TestWorld, CERT_A, CERT_B, KEY_A, KEY_B};

    const W1: &str = "world-11111111111111111111111111111111";

    fn hello_body(world: &str) -> String {
        json!({"world": world, "name": "Ashfall", "package": "t", "editorApi": 1, "kind": "community", "serverState": "running",
            "progress": {"code": "ready", "percent": 100}, "setupPending": false}).to_string()
    }

    /// A stand-in world's Editor routes: hello, login (password "right-password-123", device tokens), and a session.
    fn routes(world: &'static str) -> Reply {
        Box::new(move |seen: &Seen| {
            let json = |status: u16, v: Value| (status, vec![("Content-Type".into(), "application/json".into())], v.to_string().into_bytes());
            let body: Value = serde_json::from_slice(&seen.body).unwrap_or(json!({}));
            let bearer = seen.headers.iter().find(|(k, _)| k == "authorization").map(|(_, v)| v.clone());
            match (seen.method.as_str(), seen.path.as_str()) {
                ("GET", "/editor/hello") => (200, vec![], hello_body(world).into_bytes()),
                ("POST", "/editor/login") if body["password"] == "right-password-123" =>
                    json(200, json!({"token": "SESSION-TOKEN-0123456789", "username": body["username"], "role": "owner", "mustChange": false, "device": "DEVICE-1"})),
                ("POST", "/editor/login") => json(401, json!({"error": "wrong", "message": "Wrong name or password."})),
                ("POST", "/editor/step-up") if bearer.as_deref() == Some("Bearer SESSION-TOKEN-0123456789") => json(200, json!({"minutes": 5})),
                ("GET", "/editor/accounts") if bearer.is_some() => json(200, json!({"accounts": [], "failedToday": 3, "banners": []})),
                ("POST", "/editor/keepalive") if bearer.is_some() => json(200, json!({"expiresAt": 1, "lastSeen": 1})),
                ("POST", "/editor/logout") => json(200, json!({"ok": true})),
                _ => json(401, json!({"error": "signed-out"})),
            }
        })
    }

    fn editor() -> Editor {
        let dir = std::env::temp_dir().join(format!("ratw-desktop-test-editor-{}-{}", std::process::id(), random_hex()));
        Editor::new(dir, "memory", true)
    }

    #[test]
    fn a_first_connection_sends_nothing_but_the_hello_then_trust_and_sign_in() {
        let world = TestWorld::start(Some(server_config(CERT_A, KEY_A, false)), routes(W1));
        let e = editor();
        let at = format!("127.0.0.1:{}", world.port);
        let c = e.connect(&at);
        assert_eq!(c["state"], "new", "{c}");
        assert_eq!(c["hello"]["name"], "Ashfall");
        let seen = world.requests();
        assert_eq!(seen.len(), 1);
        assert_eq!((seen[0].method.as_str(), seen[0].path.as_str(), seen[0].body.len()), ("GET", "/editor/hello", 0));
        assert!(!seen[0].headers.iter().any(|(k, _)| k == "authorization" || k == "cookie"), "record mode sends nothing secret");
        // Signing in before trusting is refused, and trusting needs the fingerprint shown.
        assert_eq!(e.sign_in(None, &at, "host-k7q2", "right-password-123", false)["error"], "connect-first");
        assert_eq!(e.trust(&at, "0000 0000 0000 0000 0000 0000 0000 0000")["ok"], false);
        assert_eq!(e.trust(&at, c["fingerprint"].as_str().unwrap())["ok"], true);
        assert_eq!(e.connect(&at)["state"], "trusted");
        let wrong = e.sign_in(None, &at, "host-k7q2", "nope-nope-nope", false);
        assert_eq!((wrong["status"].as_u64(), wrong["error"].as_str()), (Some(401), Some("wrong")));
        assert!(wrong.get("message").is_none(), "the world's own words never reach the page");
        let ok = e.sign_in(None, &at, "Host-K7Q2", "right-password-123", false);
        assert_eq!(ok["session"]["role"], "owner", "{ok}");
        assert_eq!(e.devices.load(W1, "host-k7q2").as_deref(), Some("DEVICE-1"), "the device token is kept (here: in memory)");
        // The next sign-in shows it; the token never reaches the page or the pin store.
        e.sign_in(None, &at, "host-k7q2", "right-password-123", false);
        let login = world.requests().into_iter().filter(|s| s.path == "/editor/login").last().unwrap();
        assert_eq!(serde_json::from_slice::<Value>(&login.body).unwrap()["device"], "DEVICE-1");
        let state = e.state().to_string();
        assert!(!state.contains("SESSION-TOKEN") && !state.contains("DEVICE-1") && !state.contains("right-password"));
        let pins = serde_json::to_string(&*e.pins.file.lock().unwrap()).unwrap();
        assert!(!pins.contains("SESSION-TOKEN") && !pins.contains("DEVICE-1") && !pins.contains("right-password"));
        assert_eq!(e.step_up(None, "right-password-123")["minutes"], 5);
        assert_eq!(e.accounts(None, "list", &json!({}))["failedToday"], 3);
        assert_eq!(e.accounts(None, "explode", &json!({}))["error"], "bad-request");
        e.sign_out(None);
        assert_eq!(e.step_up(None, "right-password-123")["error"], "signed-out");
    }

    #[test]
    fn a_changed_key_is_red_and_cannot_sign_in_until_forgotten_by_name() {
        let first = TestWorld::start(Some(server_config(CERT_A, KEY_A, false)), routes(W1));
        let e = editor();
        let at = format!("127.0.0.1:{}", first.port);
        let c = e.connect(&at);
        e.trust(&at, c["fingerprint"].as_str().unwrap());
        drop(first);
        // Another key answers at the same address: here, a second stand-in on its own port pinned to that address.
        let second = TestWorld::start(Some(server_config(CERT_B, KEY_B, false)), routes(W1));
        let other = format!("127.0.0.1:{}", second.port);
        e.pins.change(|f| f.keys[0].addresses.push(other.clone()));
        let red = e.connect(&other);
        assert_eq!(red["state"], "changed", "{red}");
        assert_eq!(red["previous"], fingerprint(&pin_a()));
        // No way to sign in: the pin is still the old key's, and the handshake fails before a byte.
        let before = second.requests().len();
        assert_eq!(e.sign_in(None, &other, "host-k7q2", "right-password-123", false)["error"], "unreachable");
        assert_eq!(second.requests().len(), before, "no password went to the new key");
        assert_eq!(e.forget(&other, "Ashfal")["error"], "name");
        assert_eq!(e.forget(&other, "ashfall")["ok"], true);
        assert_eq!(e.connect(&other)["state"], "new");
    }

    #[test]
    fn a_code_pins_the_key_and_signs_in_without_the_password_reaching_the_page() {
        let world = TestWorld::start(Some(server_config(CERT_A, KEY_A, false)), routes(W1));
        let e = editor();
        let at = format!("127.0.0.1:{}", world.port);
        let record = json!({"v": 1, "w": W1, "k": pin_text(&pin_a()), "u": "pell", "p": "right-password-123", "i": "INVITE", "a": at});
        let code = format!("{}{}", wire::CODE_PREFIX, tls::b64url(record.to_string().as_bytes()));
        let used = e.use_code(&code);
        assert_eq!(used["username"], "pell");
        assert!(!used.to_string().contains("right-password") && !used.to_string().contains("INVITE"));
        assert_eq!(e.connect(&at)["state"], "trusted", "pinned from the code: no screen");
        let ok = e.sign_in(None, &at, "pell", "", true);
        assert_eq!(ok["ok"], true, "{ok}");
        let login = world.requests().into_iter().find(|s| s.path == "/editor/login").unwrap();
        let sent: Value = serde_json::from_slice(&login.body).unwrap();
        assert_eq!((sent["password"].as_str(), sent["invite"].as_str()), (Some("right-password-123"), Some("INVITE")));
        assert_eq!(e.sign_in(None, &at, "pell", "", true)["error"], "code", "a code works once");
    }

    #[test]
    fn the_wrong_port_and_nothing_listening() {
        let e = editor();
        let port = std::net::TcpListener::bind("127.0.0.1:0").unwrap().local_addr().unwrap().port();
        let c = e.connect(&format!("127.0.0.1:{port}"));
        assert_eq!((c["error"].as_str(), c["place"].as_str()), (Some("unreachable"), Some("loopback")));
        // A world's game port: plain HTTP with a world's /status.
        let id = format!("world-{}", "a".repeat(32));
        let game = TestWorld::start(None, Box::new(move |_| (200, vec![], json!({"ratw": "world", "id": id, "protocol": 3}).to_string().into_bytes())));
        assert_eq!(e.connect(&format!("127.0.0.1:{}", game.port))["error"], "game-port");
        assert!(e.pins.read(|f| f.keys.is_empty()), "that check stores nothing");
        assert_eq!(e.connect("not an address")["error"], "address");
        assert_eq!(e.connect("169.254.169.254")["error"], "address");
    }

    #[test]
    fn the_keep_alive_waits_for_the_hosts_input() {
        let world = TestWorld::start(Some(server_config(CERT_A, KEY_A, false)), routes(W1));
        let e = editor();
        let at = format!("127.0.0.1:{}", world.port);
        let c = e.connect(&at);
        e.trust(&at, c["fingerprint"].as_str().unwrap());
        e.sign_in(None, &at, "host-k7q2", "right-password-123", false);
        let keepalives = || world.requests().iter().filter(|s| s.path == "/editor/keepalive").count();
        {
            let mut inner = e.inner.lock().unwrap();
            let g = inner.session.as_ref().unwrap().generation;
            inner.window = Some(Window { generation: g, world: W1.into(), username: "host-k7q2".into() });
            inner.keepalive_at = Some(Instant::now() - KEEPALIVE_EVERY);
        }
        e.tick(None);
        assert_eq!(keepalives(), 0, "an open window left alone renews nothing");
        *e.last_input.lock().unwrap() = Some(Instant::now() - INPUT_COUNTS_FOR - Duration::from_secs(1));
        e.tick(None);
        assert_eq!(keepalives(), 0, "input over 30 minutes ago doesn't count");
        *e.last_input.lock().unwrap() = Some(Instant::now());
        e.tick(None);
        assert_eq!(keepalives(), 1, "after the host's input, a keep-alive");
        e.tick(None);
        assert_eq!(keepalives(), 1, "at most every 5 minutes");
        e.inner.lock().unwrap().window = None;
        e.inner.lock().unwrap().keepalive_at = Some(Instant::now() - KEEPALIVE_EVERY);
        e.tick(None);
        assert_eq!(keepalives(), 1, "no Editor window, no keep-alive");
    }

    #[test]
    fn the_editor_window_stays_on_its_origin() {
        let on = |url: &str, windows: bool| editor_origin_on(&tauri::Url::parse(url).unwrap(), windows);
        assert!(on("editor://localhost/?suite=dm", false));
        assert!(on("http://editor.localhost/assets/x.js", true));
        for bad in ["blob:editor://localhost/1234", "data:text/html,hi", "editor://evil/", "editor://localhost:1/", "tauri://localhost/",
            "https://example.net/", "http://editor.localhost/", "file:///etc/passwd"] {
            assert!(!on(bad, false), "{bad}");
        }
        for bad in ["blob:http://editor.localhost/1", "http://editor.localhost:8080/", "https://editor.localhost/", "http://tauri.localhost/"] {
            assert!(!on(bad, true), "{bad} on Windows");
        }
        assert!(download_allowed("world.json") && download_allowed("map.PNG") && download_allowed("rows.csv"));
        for bad in ["run.exe", "x.json.exe", ".json", "page.html", "noext"] {
            assert!(!download_allowed(bad), "{bad}");
        }
    }
}
