// The shell's side of central (Docs/Design/77-central-accounts-and-hosted-worlds.md, 3.1 and 9): the bundled page asks
// the shell to make every call to central, and the shell keeps the tokens. The page gets central's replies with the
// access and refresh tokens taken out, so a script slipped into the page by a hostile world can't read them; with
// "remember me" the refresh token is kept in the OS's keychain (tokens.rs), never in the page's storage, and the
// password is never kept at all.
//
// The same calls, the same allow-list and the same token handling as the page's FetchCentral (Client/src/net/central.ts),
// which a browser uses in development.
use serde_json::{json, Map, Value};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use crate::tokens::TokenStore;

/// Central's address: the one on the dev server (Docs/Central-server.md; an sslip.io name until the game has a domain
/// of its own). A central on this computer, for development and the tests, is `--central http://127.0.0.1:7790`.
pub const BUILT_IN: &str = "https://ratw.188-68-41-241.sslip.io";

/// Central's address, checked: `https://` anywhere, plain `http://` only to this computer (127.x, localhost, ::1); no
/// path, query, fragment or user name. Gives the base (no trailing slash) and what the page shows ("127.0.0.1:7790").
pub fn parse_central(given: &str) -> Result<(String, String), String> {
    let url = tauri::Url::parse(given.trim()).map_err(|e| format!("not an address: {e}"))?;
    let host = url.host_str().ok_or("no host")?.to_ascii_lowercase();
    match url.scheme() {
        "https" => {}
        "http" if is_loopback(&host) => {}
        "http" => return Err("plain http is only for a central on this computer; use https".into()),
        other => return Err(format!("{other}: use https (or http to this computer)")),
    }
    if !url.username().is_empty() || url.password().is_some() || url.query().is_some() || url.fragment().is_some() {
        return Err("only a scheme, a host and a port".into());
    }
    if url.path() != "/" && !url.path().is_empty() {
        return Err("no path: central's calls are at its root".into());
    }
    let port = url.port().map(|p| format!(":{p}")).unwrap_or_default();
    let host_part = if host.contains(':') && !host.starts_with('[') { format!("[{host}]") } else { host.clone() };
    Ok((format!("{}://{host_part}{port}", url.scheme()), format!("{host_part}{port}")))
}

pub fn is_loopback(host: &str) -> bool {
    let h = host.trim_start_matches('[').trim_end_matches(']');
    h == "localhost" || h == "::1" || h.parse::<std::net::Ipv4Addr>().map(|a| a.is_loopback()).unwrap_or(false)
}

/// The calls the page may ask for, and nothing else (the page's own list is the same, net/central.ts). Refreshing is
/// the shell's own business, never the page's.
pub fn allowed(method: &str, path: &str) -> bool {
    let exact: &[(&str, &[&str])] = &[
        ("/account/register", &["POST"]),
        ("/account/login", &["POST"]),
        ("/account/logout", &["POST"]),
        ("/account", &["GET", "DELETE"]),
        ("/account/quickened/announced", &["POST"]),   // (The Quickened fanfare shown: doc 49, 9.1.)
        ("/wolves", &["GET", "POST"]),
        ("/wolves/name", &["POST"]),
        ("/tickets", &["POST"]),
        ("/worlds", &["GET"]),
        ("/rules", &["GET"]),
        ("/health", &["GET"]),
        ("/host/worlds", &["GET", "POST"]),
    ];
    if let Some((_, methods)) = exact.iter().find(|(p, _)| *p == path) {
        return methods.contains(&method);
    }
    let id = |s: &str| s.len() == 32 && s.bytes().all(|b| b.is_ascii_hexdigit() && !b.is_ascii_uppercase());
    if let Some(wolf) = path.strip_prefix("/wolves/wolf-") {
        return id(wolf) && (method == "PATCH" || method == "DELETE");
    }
    // The Host page (doc 77, 3.2): a new token for one of the account's worlds, or the world removed. The world's own
    // calls (/worlds/heartbeat, /worlds/leave) are the world agent's, never the client's.
    if let Some(world) = path.strip_prefix("/host/worlds/world-") {
        return match world.strip_suffix("/token") {
            Some(token_of) => id(token_of) && method == "POST",
            None => id(world) && method == "DELETE",
        };
    }
    false
}

const TOKEN_FIELDS: [&str; 4] = ["access_token", "access_expires", "refresh_token", "refresh_expires"];

fn strip(mut body: Value) -> Value {
    if let Some(o) = body.as_object_mut() {
        for f in TOKEN_FIELDS {
            o.remove(f);
        }
    }
    body
}

fn refusal(error: &str, code: &str) -> Value {
    json!({"ok": false, "error": error, "code": code})
}

/// One HTTP exchange with central: (status, the JSON body); status 0 when it couldn't be reached.
pub trait Http: Send + Sync {
    fn send(&self, method: &str, url: &str, body: Option<&Value>, bearer: Option<&str>) -> (u16, Value);
}

/// ureq, 15 seconds, no redirects, a reply of at most 1 MB.
pub struct UreqHttp {
    agent: ureq::Agent,
}

impl UreqHttp {
    pub fn new() -> Self {
        let agent = ureq::Agent::config_builder()
            .timeout_global(Some(Duration::from_secs(15)))
            .http_status_as_error(false)
            .max_redirects(0)
            .proxy(None) // (Never HTTP_PROXY from the environment: the password and tokens go to central alone.)
            .user_agent(concat!("ratw-desktop/", env!("CARGO_PKG_VERSION")))
            .build()
            .into();
        UreqHttp { agent }
    }
}

impl Http for UreqHttp {
    fn send(&self, method: &str, url: &str, body: Option<&Value>, bearer: Option<&str>) -> (u16, Value) {
        let auth = bearer.map(|t| format!("Bearer {t}"));
        let result = match (method, body) {
            ("GET", _) | ("DELETE", None) => {
                let mut r = if method == "GET" { self.agent.get(url).force_send_body() } else { self.agent.delete(url).force_send_body() };
                if let Some(a) = &auth {
                    r = r.header("Authorization", a);
                }
                r.send_empty()
            }
            (_, body) => {
                let mut r = match method {
                    "POST" => self.agent.post(url),
                    "PATCH" => self.agent.patch(url),
                    "DELETE" => self.agent.delete(url).force_send_body(),
                    _ => return (400, refusal("Not a call the client makes.", "bad_request")),
                };
                if let Some(a) = &auth {
                    r = r.header("Authorization", a);
                }
                r.header("Content-Type", "application/json").send(serde_json::to_vec(body.unwrap_or(&json!({}))).unwrap_or_default())
            }
        };
        match result {
            Ok(mut response) => {
                let status = response.status().as_u16();
                let text = response.body_mut().with_config().limit(1 << 20).read_to_string().unwrap_or_default();
                match serde_json::from_str::<Value>(&text) {
                    Ok(v) if v.is_object() => (status, v),
                    _ => (status, refusal("Central sent something unreadable.", "bad_reply")),
                }
            }
            Err(_) => (0, refusal("Central could not be reached.", "unreachable")),
        }
    }
}

struct Tokens {
    access: String,
    refresh: String,
    remember: bool,
}

/// Central, as the shell holds it: the address, the tokens of the one signed-in account, and where a remembered
/// sign-in is kept.
pub struct Central {
    pub base: String,
    pub shown: String,
    pub development: bool,
    http: Box<dyn Http>,
    store: Arc<dyn TokenStore>,
    tokens: Mutex<Option<Tokens>>,
    /// Bumped by every sign-in, sign-out and deletion: a refresh that was under way when one happened is dropped, so it
    /// can't bring back the tokens of a session the user just left.
    generation: Mutex<u64>,
    renewing: Mutex<()>,
}

impl Central {
    pub fn new(base: String, shown: String, development: bool, http: Box<dyn Http>, store: Arc<dyn TokenStore>) -> Self {
        Central { base, shown, development, http, store, tokens: Mutex::new(None), generation: Mutex::new(0), renewing: Mutex::new(()) }
    }

    /// What the page is told of central (central_info): where it is to say, and the base URL itself as `url`, scheme
    /// and all, for the commands on the Host page (an https:// central can be on this computer too: never guessed).
    pub fn info(&self) -> Value {
        json!({"where": self.shown, "url": self.base, "development": self.development, "remembered": self.remembered(), "store": self.store_kind()})
    }

    pub fn store_kind(&self) -> &'static str {
        self.store.kind()
    }

    pub fn remembered(&self) -> bool {
        self.store.load().is_some()
    }

    fn access(&self) -> Option<String> {
        self.tokens.lock().unwrap().as_ref().map(|t| t.access.clone())
    }

    fn generation(&self) -> u64 {
        *self.generation.lock().unwrap()
    }

    fn forget(&self) {
        let mut g = self.generation.lock().unwrap();
        *g += 1;
        *self.tokens.lock().unwrap() = None;
        self.store.clear();
    }

    /// A sign-in's tokens: a new session, so any refresh under way for the old one is dropped.
    fn sign_in(&self, reply: &Value, remember: bool) {
        let mut g = self.generation.lock().unwrap();
        *g += 1;
        self.take_tokens(reply, remember);
    }

    fn take_tokens(&self, reply: &Value, remember: bool) {
        let access = reply["access_token"].as_str().unwrap_or_default().to_owned();
        let refresh = reply["refresh_token"].as_str().unwrap_or_default().to_owned();
        if remember && !refresh.is_empty() {
            if let Err(e) = self.store.save(&refresh) {
                eprintln!("ratw-desktop: the sign-in could not be remembered: {e}");
            }
        } else {
            self.store.clear();
        }
        *self.tokens.lock().unwrap() = Some(Tokens { access, refresh, remember });
    }

    /// A call for the page: {status, body}, the body without tokens.
    pub fn call(&self, method: &str, path: &str, body: Option<Value>) -> Value {
        if !allowed(method, path) {
            return json!({"status": 400, "body": refusal("Not a call the client makes.", "bad_request")});
        }
        let url = format!("{}{}", self.base, path);
        let body = body.filter(|b| !b.is_null());
        if body.as_ref().is_some_and(|b| !b.is_object()) {
            return json!({"status": 400, "body": refusal("A body is a JSON object.", "bad_request")});
        }
        let (status, reply) = if path == "/account/login" || path == "/account/register" {
            let remember = body.as_ref().and_then(|b| b.get("remember")).and_then(Value::as_bool).unwrap_or(false);
            let (status, reply) = self.http.send(method, &url, body.as_ref(), None);
            if status == 200 {
                self.sign_in(&reply, remember);
            }
            (status, reply)
        } else {
            let used = self.access();
            let (mut status, mut reply) = self.http.send(method, &url, body.as_ref(), used.as_deref());
            // Refreshed only when the access token is the problem: a wrong password on a deletion (bad_sign_in) is not
            // sent twice. Signing out is retried too, so a token that expired while playing still ends the session.
            if status == 401 && reply["code"] == "unauthenticated" && self.renew_after(used.as_deref()) {
                (status, reply) = self.http.send(method, &url, body.as_ref(), self.access().as_deref());
            }
            if path == "/account/logout" || (path == "/account" && method == "DELETE" && status == 200) {
                self.forget();
            }
            (status, reply)
        };
        json!({"status": status, "body": strip(reply)})
    }

    /// A remembered sign-in, refreshed (at launch): {status, body}; 401 when none is kept.
    pub fn resume(&self) -> Value {
        if self.tokens.lock().unwrap().is_none() {
            let Some(refresh) = self.store.load() else {
                return json!({"status": 401, "body": refusal("Sign in.", "unauthenticated")});
            };
            *self.tokens.lock().unwrap() = Some(Tokens { access: String::new(), refresh, remember: true });
        }
        let _one = self.renewing.lock().unwrap();
        let (status, reply) = self.renew();
        json!({"status": status, "body": strip(reply)})
    }

    /// After a 401 with `used`: refreshes once (unless another call already has), true when there is a new access token.
    fn renew_after(&self, used: Option<&str>) -> bool {
        let _one = self.renewing.lock().unwrap();
        if self.access().as_deref() != used {
            return self.access().is_some();            // (Another call refreshed meanwhile.)
        }
        self.renew().0 == 200
    }

    /// Spends the refresh token for new ones (with `renewing` held). If the user signed in, out or deleted the account
    /// meanwhile, the answer is dropped, whatever it was.
    fn renew(&self) -> (u16, Value) {
        let started = self.generation();
        let (refresh, remember) = match self.tokens.lock().unwrap().as_ref() {
            Some(t) if !t.refresh.is_empty() => (t.refresh.clone(), t.remember),
            _ => return (401, refusal("Sign in.", "unauthenticated")),
        };
        let mut body = Map::new();
        body.insert("refresh_token".into(), Value::String(refresh));
        let (status, reply) = self.http.send("POST", &format!("{}/account/refresh", self.base), Some(&Value::Object(body)), None);
        let g = self.generation.lock().unwrap();
        if *g != started {
            return (401, refusal("Sign in.", "unauthenticated"));
        }
        if status == 200 {
            self.take_tokens(&reply, remember);
        } else if status == 401 || status == 403 {
            *self.tokens.lock().unwrap() = None;
            self.store.clear();
        }
        drop(g);
        (status, reply)
    }
}

/// A world's GET /status (doc 77, 5.1 and 7: the client's ping): one request, 2 seconds, a reply of at most 2 KB, no
/// redirects, no proxy. {status, body, ms}; status 0 when it couldn't be reached or isn't a world.
///
/// The page names the address, and a script slipped into the page could name any, so: never a link-local, unspecified,
/// multicast or broadcast address (cloud metadata lives at 169.254.169.254), and only a world's own reply comes back
/// (`ratw: "world"` and a world id), cut down to the fields the list shows. Anything else on this computer or the LAN
/// gives the page nothing to read. (Loopback and private addresses stay allowed: a world on this computer or the LAN is
/// what the direct-address box is for.)
pub fn world_status(host: &str, port: u16, secure: bool) -> Value {
    let unreachable = json!({"status": 0, "body": null, "ms": 0});
    let fine = !host.is_empty() && host.len() <= 253 && host.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'.' || b == b'-' || b == b':');
    if !fine || port == 0 {
        return unreachable;
    }
    use std::net::ToSocketAddrs;
    let Ok(addresses) = (host, port).to_socket_addrs().map(|a| a.collect::<Vec<_>>()) else { return unreachable };
    if addresses.is_empty() || addresses.iter().any(|a| !pingable(a.ip())) {
        return unreachable;
    }
    let host_part = if host.contains(':') { format!("[{host}]") } else { host.to_owned() };
    let url = format!("{}://{host_part}:{port}/status", if secure { "https" } else { "http" });
    let agent: ureq::Agent = ureq::Agent::config_builder()
        .timeout_global(Some(Duration::from_secs(2)))
        .http_status_as_error(false)
        .max_redirects(0)
        .proxy(None)
        .build()
        .into();
    let started = std::time::Instant::now();
    match agent.get(&url).call() {
        Ok(mut response) => {
            let status = response.status().as_u16();
            let text = response.body_mut().with_config().limit(2048).read_to_string();
            let ms = started.elapsed().as_secs_f64() * 1000.0;
            match text.ok().and_then(|t| serde_json::from_str::<Value>(&t).ok()).and_then(|b| world_reply(&b)) {
                Some(body) => json!({"status": status, "body": body, "ms": ms}),
                None => unreachable,
            }
        }
        Err(_) => unreachable,
    }
}

/// Whether the ping may go to this address.
pub fn pingable(ip: std::net::IpAddr) -> bool {
    use std::net::IpAddr;
    match ip {
        IpAddr::V4(a) => !(a.is_link_local() || a.is_unspecified() || a.is_multicast() || a.is_broadcast()),
        IpAddr::V6(a) => {
            if let Some(v4) = a.to_ipv4_mapped() {
                return pingable(IpAddr::V4(v4));
            }
            let link_local = (a.segments()[0] & 0xffc0) == 0xfe80;
            !(link_local || a.is_unspecified() || a.is_multicast())
        }
    }
}

/// A world's /status, cut down to what the list shows (Client/src/net/worlds.ts, readStatus); None if it isn't one.
pub fn world_reply(body: &Value) -> Option<Value> {
    let id = body.get("id")?.as_str()?;
    let world_id = id.len() == 38 && id.starts_with("world-") && id[6..].bytes().all(|b| b.is_ascii_hexdigit() && !b.is_ascii_uppercase());
    if body.get("ratw")?.as_str()? != "world" || !world_id || !body.get("protocol")?.is_number() {
        return None;
    }
    let mut out = Map::new();
    for key in ["ratw", "id", "name", "kind", "protocol", "clientData", "players", "limit", "voices", "cheats", "private"] {
        if let Some(v) = body.get(key) {
            let small = match v {
                Value::String(t) => t.chars().count() <= 64,
                Value::Number(_) | Value::Bool(_) => true,
                _ => false,
            };
            if small {
                out.insert(key.into(), v.clone());
            }
        }
    }
    Some(Value::Object(out))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::tokens::MemoryStore;

    #[test]
    fn the_central_address_rule() {
        assert_eq!(parse_central("http://127.0.0.1:7790").unwrap(), ("http://127.0.0.1:7790".into(), "127.0.0.1:7790".into()));
        assert_eq!(parse_central("http://localhost:7790/").unwrap().0, "http://localhost:7790");
        assert_eq!(parse_central("http://[::1]:7790").unwrap().0, "http://[::1]:7790");
        assert_eq!(parse_central("https://central.example.net").unwrap(), ("https://central.example.net".into(), "central.example.net".into()));
        assert_eq!(parse_central("HTTPS://Central.Example.net:8443").unwrap().0, "https://central.example.net:8443");
        for bad in ["http://192.168.1.4:7790", "http://central.example.net", "ftp://127.0.0.1", "https://a.net/path", "https://a.net/?x=1",
            "https://user:pw@a.net", "127.0.0.1:7790", "", "ws://127.0.0.1:7790"] {
            assert!(parse_central(bad).is_err(), "{bad} should be refused");
        }
    }

    #[test]
    fn the_page_is_told_centrals_real_url() {
        for (given, url) in [("https://127.0.0.1:7790", "https://127.0.0.1:7790"), ("http://localhost:7790/", "http://localhost:7790"),
            ("https://central.example.net", "https://central.example.net")] {
            let (base, shown) = parse_central(given).unwrap();
            let (mut c, _) = central(Arc::new(MemoryStore::default()));
            c.base = base;
            c.shown = shown.clone();
            let info = c.info();
            assert_eq!(info["url"], url, "{given}");
            assert_eq!(info["where"], shown);
            assert!(info.get("access_token").is_none() && info.get("refresh_token").is_none());
        }
    }

    #[test]
    fn only_the_clients_calls() {
        assert!(allowed("POST", "/account/login"));
        assert!(allowed("DELETE", "/account"));
        assert!(allowed("POST", "/account/quickened/announced"));
        assert!(!allowed("GET", "/account/quickened/announced"));
        assert!(allowed("GET", "/worlds"));
        assert!(allowed("DELETE", "/wolves/wolf-0123456789abcdef0123456789abcdef"));
        assert!(!allowed("DELETE", "/wolves/wolf-0123456789ABCDEF0123456789abcdef"));
        assert!(!allowed("POST", "/account/refresh"), "refreshing is the shell's own");
        assert!(!allowed("GET", "/keys"));
        assert!(!allowed("GET", "/wolves/wolf-0123456789abcdef0123456789abcdef"));
        assert!(!allowed("GET", "/account/../keys"));
        // The Host page's calls, and not the world's own.
        let world = "/host/worlds/world-0123456789abcdef0123456789abcdef";
        assert!(allowed("GET", "/host/worlds"));
        assert!(allowed("POST", "/host/worlds"));
        assert!(!allowed("DELETE", "/host/worlds"));
        assert!(allowed("POST", &format!("{world}/token")));
        assert!(!allowed("GET", &format!("{world}/token")));
        assert!(!allowed("DELETE", &format!("{world}/token")));
        assert!(allowed("DELETE", world));
        assert!(!allowed("GET", world));
        assert!(!allowed("POST", world));
        assert!(!allowed("DELETE", "/host/worlds/world-0123456789ABCDEF0123456789abcdef"));
        assert!(!allowed("DELETE", "/host/worlds/world-0123456789abcdef0123456789abcde"));
        assert!(!allowed("POST", "/host/worlds/world-0123456789abcdef0123456789abcdef/token/"));
        assert!(!allowed("POST", "/host/worlds/wolf-0123456789abcdef0123456789abcdef/token"));
        assert!(!allowed("POST", "/worlds/heartbeat"), "the heartbeat is the world agent's");
        assert!(!allowed("POST", "/worlds/leave"));
    }

    /// Central, faked: records what it was sent and answers by the path.
    struct Fake {
        seen: Mutex<Vec<(String, String, Option<String>)>>,
        access: Mutex<String>,
        slow_refresh_ms: Mutex<u64>,
    }

    impl Http for Arc<Fake> {
        fn send(&self, method: &str, url: &str, body: Option<&Value>, bearer: Option<&str>) -> (u16, Value) {
            let path = url.trim_start_matches("http://central");
            self.seen.lock().unwrap().push((method.into(), path.into(), bearer.map(str::to_owned)));
            let current = self.access.lock().unwrap().clone();
            match path {
                "/account/login" | "/account/register" => {
                    *self.access.lock().unwrap() = "A1".into();
                    (200, json!({"ok": true, "account": {"id": "acct-1"}, "access_token": "A1", "access_expires": 1, "refresh_token": "R1", "refresh_expires": 2}))
                }
                "/account/refresh" => {
                    std::thread::sleep(Duration::from_millis(*self.slow_refresh_ms.lock().unwrap()));
                    if body.and_then(|b| b["refresh_token"].as_str()) == Some("R1") || body.and_then(|b| b["refresh_token"].as_str()) == Some("R2") {
                        *self.access.lock().unwrap() = "A2".into();
                        (200, json!({"ok": true, "account": {"id": "acct-1"}, "access_token": "A2", "refresh_token": "R2"}))
                    } else {
                        (401, json!({"ok": false, "code": "unauthenticated"}))
                    }
                }
                "/account" if body.is_some_and(|b| b["password"] != "pw") => (401, json!({"ok": false, "code": "bad_sign_in"})),
                _ if bearer == Some(current.as_str()) && !current.is_empty() => (200, json!({"ok": true, "wolves": []})),
                _ => (401, json!({"ok": false, "code": "unauthenticated"})),
            }
        }
    }

    fn central(store: Arc<MemoryStore>) -> (Central, Arc<Fake>) {
        let fake = Arc::new(Fake { seen: Mutex::new(vec![]), access: Mutex::new(String::new()), slow_refresh_ms: Mutex::new(0) });
        (Central::new("http://central".into(), "central".into(), true, Box::new(fake.clone()), store), fake)
    }

    #[test]
    fn the_page_never_sees_a_token_and_remember_me_keeps_only_the_refresh_token() {
        let store = Arc::new(MemoryStore::default());
        let (c, fake) = central(store.clone());
        let r = c.call("POST", "/account/login", Some(json!({"username": "ash", "password": "pw", "remember": false})));
        assert_eq!(r["status"], 200);
        for f in TOKEN_FIELDS {
            assert!(r["body"].get(f).is_none(), "{f} reached the page");
        }
        assert_eq!(r["body"]["account"]["id"], "acct-1");
        assert_eq!(store.load(), None, "not remembered");
        let r = c.call("GET", "/wolves", None);
        assert_eq!(r["status"], 200);
        assert_eq!(fake.seen.lock().unwrap().last().unwrap().2.as_deref(), Some("A1"), "the shell adds the access token");
        c.call("POST", "/account/login", Some(json!({"username": "ash", "password": "pw", "remember": true})));
        assert_eq!(store.load().as_deref(), Some("R1"), "remembered: the refresh token, never the password");
        c.call("POST", "/account/logout", Some(json!({})));
        assert_eq!(store.load(), None, "signing out forgets it");
        assert_eq!(c.call("GET", "/wolves", None)["status"], 401);
    }

    #[test]
    fn an_expired_access_token_is_refreshed_once_and_a_remembered_sign_in_resumes() {
        let store = Arc::new(MemoryStore::default());
        let (c, fake) = central(store.clone());
        c.call("POST", "/account/login", Some(json!({"username": "ash", "password": "pw", "remember": true})));
        *fake.access.lock().unwrap() = "expired".into();      // (Central has moved on: A1 is refused.)
        let r = c.call("GET", "/wolves", None);
        assert_eq!(r["status"], 200, "refreshed and tried again");
        assert_eq!(store.load().as_deref(), Some("R2"), "the new refresh token is kept");
        assert!(fake.seen.lock().unwrap().iter().any(|(_, p, _)| p == "/account/refresh"));
        // A new launch: nothing in memory, the keychain's token.
        let (again, _) = central(store.clone());
        let r = again.resume();
        assert_eq!(r["status"], 200);
        assert!(r["body"].get("refresh_token").is_none());
        assert_eq!(again.call("GET", "/wolves", None)["status"], 200);
        // A refused refresh forgets the remembered sign-in.
        let bad = Arc::new(MemoryStore::default());
        bad.save("stolen").unwrap();
        let (third, _) = central(bad.clone());
        assert_eq!(third.resume()["status"], 401);
        assert_eq!(bad.load(), None);
        let (none, _) = central(Arc::new(MemoryStore::default()));
        assert_eq!(none.resume()["status"], 401);
    }

    #[test]
    fn refused_calls_never_reach_central() {
        let (c, fake) = central(Arc::new(MemoryStore::default()));
        assert_eq!(c.call("POST", "/account/refresh", Some(json!({"refresh_token": "x"})))["status"], 400);
        assert_eq!(c.call("GET", "/keys", None)["status"], 400);
        assert_eq!(c.call("POST", "/wolves", Some(json!([1, 2])))["status"], 400);
        assert!(fake.seen.lock().unwrap().is_empty());
    }

    #[test]
    fn a_status_from_a_bad_host_is_not_asked() {
        assert_eq!(world_status("evil/path", 80, false)["status"], 0);
        assert_eq!(world_status("", 80, false)["status"], 0);
        assert_eq!(world_status("127.0.0.1", 0, false)["status"], 0);
    }

    fn refreshes(fake: &Fake) -> usize {
        fake.seen.lock().unwrap().iter().filter(|(_, p, _)| p == "/account/refresh").count()
    }

    #[test]
    fn a_wrong_password_is_not_sent_twice() {
        let (c, fake) = central(Arc::new(MemoryStore::default()));
        c.call("POST", "/account/login", Some(json!({"username": "ash", "password": "pw", "remember": true})));
        let r = c.call("DELETE", "/account", Some(json!({"password": "typo"})));
        assert_eq!(r["status"], 401);
        assert_eq!(refreshes(&fake), 0, "bad_sign_in is not a stale token");
        assert_eq!(fake.seen.lock().unwrap().iter().filter(|(m, p, _)| m == "DELETE" && p == "/account").count(), 1);
    }

    #[test]
    fn signing_out_with_an_expired_token_still_ends_the_session() {
        let store = Arc::new(MemoryStore::default());
        let (c, fake) = central(store.clone());
        c.call("POST", "/account/login", Some(json!({"username": "ash", "password": "pw", "remember": true})));
        *fake.access.lock().unwrap() = "expired".into();
        let r = c.call("POST", "/account/logout", Some(json!({})));
        assert_eq!(r["status"], 200, "refreshed, then signed out on central");
        assert_eq!(refreshes(&fake), 1);
        assert_eq!(store.load(), None);
        assert_eq!(c.call("GET", "/wolves", None)["status"], 401);
    }

    #[test]
    fn a_refresh_under_way_cannot_bring_back_an_old_session() {
        let store = Arc::new(MemoryStore::default());
        let (c, fake) = central(store.clone());
        let c = Arc::new(c);
        c.call("POST", "/account/login", Some(json!({"username": "ash", "password": "pw", "remember": true})));
        *fake.access.lock().unwrap() = "expired".into();
        *fake.slow_refresh_ms.lock().unwrap() = 300;
        let slow = { let c = c.clone(); std::thread::spawn(move || c.call("GET", "/wolves", None)) };
        std::thread::sleep(Duration::from_millis(100));
        // Meanwhile: a sign-in to another session, not remembered.
        c.call("POST", "/account/login", Some(json!({"username": "ash", "password": "pw", "remember": false})));
        slow.join().unwrap();
        assert_eq!(store.load(), None, "the old session's new refresh token was not kept");
        assert_eq!(c.access().as_deref(), Some("A1"), "the new sign-in's token stands");
    }

    #[test]
    fn the_ping_goes_only_to_worlds() {
        use std::net::IpAddr;
        for bad in ["169.254.169.254", "0.0.0.0", "224.0.0.1", "255.255.255.255", "fe80::1", "::", "::ffff:169.254.169.254"] {
            assert!(!pingable(bad.parse::<IpAddr>().unwrap()), "{bad}");
        }
        for ok in ["127.0.0.1", "192.168.1.20", "10.0.0.5", "::1", "2001:db8::1", "93.184.216.34"] {
            assert!(pingable(ok.parse::<IpAddr>().unwrap()), "{ok}");
        }
        assert_eq!(world_status("169.254.169.254", 80, false)["status"], 0);
        let id = format!("world-{}", "a".repeat(32));
        let w = world_reply(&json!({"ratw": "world", "id": id, "protocol": 1, "name": "Hollow", "players": 2, "secret": "x", "kind": {"a": 1}})).unwrap();
        assert_eq!(w["name"], "Hollow");
        assert!(w.get("secret").is_none() && w.get("kind").is_none(), "only the list's own fields");
        assert!(world_reply(&json!({"ok": true, "status": "running"})).is_none(), "another service's reply gives nothing");
        assert!(world_reply(&json!({"ratw": "world", "id": "world-x", "protocol": 1})).is_none());
    }
}
