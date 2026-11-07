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

/// Central's address for now: a central on this computer (the user sets up the real one later, doc 77 Q10). A
/// release builds in the real address.
pub const BUILT_IN: &str = "http://127.0.0.1:7790";

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
        ("/wolves", &["GET", "POST"]),
        ("/wolves/name", &["POST"]),
        ("/tickets", &["POST"]),
        ("/worlds", &["GET"]),
        ("/rules", &["GET"]),
        ("/health", &["GET"]),
    ];
    if let Some((_, methods)) = exact.iter().find(|(p, _)| *p == path) {
        return methods.contains(&method);
    }
    let wolf = path.strip_prefix("/wolves/wolf-").map(|id| id.len() == 32 && id.bytes().all(|b| b.is_ascii_hexdigit() && !b.is_ascii_uppercase()));
    wolf == Some(true) && (method == "PATCH" || method == "DELETE")
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
    renewing: Mutex<()>,
}

impl Central {
    pub fn new(base: String, shown: String, development: bool, http: Box<dyn Http>, store: Arc<dyn TokenStore>) -> Self {
        Central { base, shown, development, http, store, tokens: Mutex::new(None), renewing: Mutex::new(()) }
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

    fn forget(&self) {
        *self.tokens.lock().unwrap() = None;
        self.store.clear();
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
                self.take_tokens(&reply, remember);
            }
            (status, reply)
        } else {
            let used = self.access();
            let (mut status, mut reply) = self.http.send(method, &url, body.as_ref(), used.as_deref());
            if status == 401 && path != "/account/logout" && self.renew_after(used.as_deref()) {
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
        let used = self.access();
        let _one = self.renewing.lock().unwrap();
        let (status, reply) = self.renew(used.as_deref());
        json!({"status": status, "body": strip(reply)})
    }

    /// After a 401 with `used`: refreshes once (unless another call already has), true when there is a new access token.
    fn renew_after(&self, used: Option<&str>) -> bool {
        let _one = self.renewing.lock().unwrap();
        if self.access().as_deref() != used {
            return self.access().is_some();            // (Another call refreshed meanwhile.)
        }
        self.renew(used).0 == 200
    }

    fn renew(&self, _used: Option<&str>) -> (u16, Value) {
        let (refresh, remember) = match self.tokens.lock().unwrap().as_ref() {
            Some(t) if !t.refresh.is_empty() => (t.refresh.clone(), t.remember),
            _ => return (401, refusal("Sign in.", "unauthenticated")),
        };
        let mut body = Map::new();
        body.insert("refresh_token".into(), Value::String(refresh));
        let (status, reply) = self.http.send("POST", &format!("{}/account/refresh", self.base), Some(&Value::Object(body)), None);
        if status == 200 {
            self.take_tokens(&reply, remember);
        } else if status == 401 || status == 403 {
            self.forget();
        }
        (status, reply)
    }
}

/// A world's GET /status (doc 77, 5.1 and 7: the client's ping): one request, 2 seconds, a reply of at most 2 KB, no
/// redirects. {status, body, ms}; status 0 when it couldn't be reached.
pub fn world_status(host: &str, port: u16, secure: bool) -> Value {
    let fine = !host.is_empty() && host.len() <= 253 && host.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'.' || b == b'-' || b == b':');
    if !fine || port == 0 {
        return json!({"status": 0, "body": null, "ms": 0});
    }
    let host_part = if host.contains(':') { format!("[{host}]") } else { host.to_owned() };
    let url = format!("{}://{host_part}:{port}/status", if secure { "https" } else { "http" });
    let agent: ureq::Agent = ureq::Agent::config_builder()
        .timeout_global(Some(Duration::from_secs(2)))
        .http_status_as_error(false)
        .max_redirects(0)
        .build()
        .into();
    let started = std::time::Instant::now();
    match agent.get(&url).call() {
        Ok(mut response) => {
            let status = response.status().as_u16();
            let text = response.body_mut().with_config().limit(2048).read_to_string();
            let ms = started.elapsed().as_secs_f64() * 1000.0;
            match text.ok().and_then(|t| serde_json::from_str::<Value>(&t).ok()) {
                Some(body) => json!({"status": status, "body": body, "ms": ms}),
                None => json!({"status": 0, "body": null, "ms": ms}),
            }
        }
        Err(_) => json!({"status": 0, "body": null, "ms": 0}),
    }
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
    fn only_the_clients_calls() {
        assert!(allowed("POST", "/account/login"));
        assert!(allowed("DELETE", "/account"));
        assert!(allowed("GET", "/worlds"));
        assert!(allowed("DELETE", "/wolves/wolf-0123456789abcdef0123456789abcdef"));
        assert!(!allowed("DELETE", "/wolves/wolf-0123456789ABCDEF0123456789abcdef"));
        assert!(!allowed("POST", "/account/refresh"), "refreshing is the shell's own");
        assert!(!allowed("GET", "/keys"));
        assert!(!allowed("GET", "/wolves/wolf-0123456789abcdef0123456789abcdef"));
        assert!(!allowed("GET", "/account/../keys"));
    }

    /// Central, faked: records what it was sent and answers by the path.
    struct Fake {
        seen: Mutex<Vec<(String, String, Option<String>)>>,
        access: Mutex<String>,
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
                    if body.and_then(|b| b["refresh_token"].as_str()) == Some("R1") || body.and_then(|b| b["refresh_token"].as_str()) == Some("R2") {
                        *self.access.lock().unwrap() = "A2".into();
                        (200, json!({"ok": true, "account": {"id": "acct-1"}, "access_token": "A2", "refresh_token": "R2"}))
                    } else {
                        (401, json!({"ok": false, "code": "unauthenticated"}))
                    }
                }
                _ if bearer == Some(current.as_str()) && !current.is_empty() => (200, json!({"ok": true, "wolves": []})),
                _ => (401, json!({"ok": false, "code": "unauthenticated"})),
            }
        }
    }

    fn central(store: Arc<MemoryStore>) -> (Central, Arc<Fake>) {
        let fake = Arc::new(Fake { seen: Mutex::new(vec![]), access: Mutex::new(String::new()) });
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
}
