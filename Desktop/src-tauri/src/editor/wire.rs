// What the shell reads from the page and from worlds, checked (Docs/Design/78, sections 2 and 6.6): a world's address as
// the host typed it, a world's /editor/hello, and an Editor code. Text a world controls never reaches the main window
// unchecked (I12): the hello's name loses control and bidirectional characters and is cut to 40, progress is a known
// code and a percent, and every other field is dropped or refused.
use std::net::{IpAddr, SocketAddr, ToSocketAddrs};

use serde::Serialize;
use serde_json::{Map, Value};

use super::tls::{pin_from_text, Pin};

/// The Editor port, unless the address names another (doc 78, 3.2).
pub const PORT: u16 = 17789;
/// The game's port, for screen 3's "that's your game port" and "is the game up" checks.
pub const GAME_PORT: u16 = 7788;
/// The Editor API this client speaks (tools/world_admin.py: EDITOR_API).
pub const EDITOR_API: u64 = 1;
pub const HELLO_MOST: usize = 1024;
pub const CODE_PREFIX: &str = "ratw-editor:";

// ---------------------------------------------------------------------------------------------------------------------
// Addresses.

/// A world's address as the host gave it: the host (lowercased, IPv6 without brackets), the port, and the form the pin
/// store keys aliases by and the screens show ("192.168.1.20:17789", "[2001:db8::1]:17789").
#[derive(Debug, Clone, PartialEq)]
pub struct Address {
    pub host: String,
    pub port: u16,
    pub shown: String,
}

/// Where an address is, for screen 3's words: this computer, a home network, or the internet.
#[derive(Debug, Clone, Copy, PartialEq, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum Place {
    Loopback,
    Home,
    Public,
}

pub fn parse_address(given: &str) -> Result<Address, &'static str> {
    let text = given.trim();
    if text.is_empty() {
        return Err("empty");
    }
    if text.len() > 260 {
        return Err("long");
    }
    let (host, port) = if let Some(rest) = text.strip_prefix('[') {
        let (host, after) = rest.split_once(']').ok_or("bad")?;
        let port = match after {
            "" => PORT,
            p => p.strip_prefix(':').ok_or("bad")?.parse::<u16>().map_err(|_| "port")?,
        };
        host.parse::<std::net::Ipv6Addr>().map_err(|_| "bad")?;
        (host.to_ascii_lowercase(), port)
    } else if text.matches(':').count() > 1 {
        text.parse::<std::net::Ipv6Addr>().map_err(|_| "bad")?;
        (text.to_ascii_lowercase(), PORT)
    } else {
        match text.split_once(':') {
            Some((h, p)) => (h.to_ascii_lowercase(), p.parse::<u16>().map_err(|_| "port")?),
            None => (text.to_ascii_lowercase(), PORT),
        }
    };
    if port == 0 {
        return Err("port");
    }
    let host = host.trim_end_matches('.').to_string();
    let name_ok = !host.is_empty() && host.len() <= 253
        && host.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'.' || b == b'-' || b == b':')
        && !host.starts_with('-') && !host.starts_with('.');
    if !name_ok {
        return Err("bad");
    }
    // `localhost` may resolve to ::1, which a loopback-only Editor port doesn't answer (doc 78, screen 2).
    let host = if host == "localhost" { "127.0.0.1".to_string() } else { host };
    let shown = if host.contains(':') { format!("[{host}]:{port}") } else { format!("{host}:{port}") };
    Ok(Address { host, port, shown })
}

/// The addresses to connect to: an IP as it is, a name resolved once here (so the address checked is the address
/// used). Never a link-local, unspecified, multicast or broadcast address (central::pingable). IPv4 first.
pub fn resolve(address: &Address) -> Result<Vec<SocketAddr>, &'static str> {
    let found: Vec<SocketAddr> = match address.host.parse::<IpAddr>() {
        Ok(ip) => vec![SocketAddr::new(ip, address.port)],
        Err(_) => (address.host.as_str(), address.port).to_socket_addrs().map_err(|_| "unresolved")?.collect(),
    };
    if found.is_empty() {
        return Err("unresolved");
    }
    if found.iter().any(|a| !crate::central::pingable(a.ip())) {
        return Err("refused");
    }
    let mut sorted = found;
    sorted.sort_by_key(|a| !a.is_ipv4());
    Ok(sorted)
}

pub fn place(ip: IpAddr) -> Place {
    match ip {
        IpAddr::V4(a) if a.is_loopback() => Place::Loopback,
        IpAddr::V4(a) if a.is_private() || (a.octets()[0] == 100 && (a.octets()[1] & 0xc0) == 64) => Place::Home,
        IpAddr::V6(a) if a.is_loopback() => Place::Loopback,
        IpAddr::V6(a) if a.to_ipv4_mapped().is_some() => place(IpAddr::V4(a.to_ipv4_mapped().unwrap())),
        IpAddr::V6(a) if (a.segments()[0] & 0xfe00) == 0xfc00 => Place::Home,
        _ => Place::Public,
    }
}

/// The URL base for one resolved address (`https://203.0.113.7:17789`): the shell connects to the address it checked.
pub fn base_url(socket: &SocketAddr) -> String {
    match socket.ip() {
        IpAddr::V4(a) => format!("https://{a}:{}", socket.port()),
        IpAddr::V6(a) => format!("https://[{a}]:{}", socket.port()),
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// The hello.

#[derive(Debug, Clone, PartialEq, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Progress {
    pub code: String,
    pub percent: u8,
}

/// The game server's state as the world's entrypoint last wrote it (doc 78, section 2's pill; R1 H2): up or not, why,
/// its last exit code, its crashes in a row and the seconds until the next start. Only known words and bounded numbers.
#[derive(Debug, Clone, PartialEq, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ServerUp {
    pub up: bool,
    pub reason: String,
    pub code: Option<u8>,
    pub failures: u8,
    pub retry_in: Option<u32>,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Rotation {
    pub spki: Vec<u8>,
    pub signature: Vec<u8>,
    pub previous: String,
}

/// A world's /editor/hello, checked. What the page sees of it is `page()`.
#[derive(Debug, Clone, PartialEq)]
pub struct Hello {
    pub world: String,
    pub name: String,
    pub package: String,
    pub editor_api: u64,
    pub official: bool,
    pub server_state: String,
    pub progress: Progress,
    pub setup_pending: bool,
    pub rotation: Option<Rotation>,
}

// ("slow": the world was set up, but its database didn't answer the hello in time.)
const PROGRESS_CODES: [&str; 8] = ["starting", "database", "importing", "migrating", "building", "ready", "stopped", "slow"];
const SERVER_REASONS: [&str; 6] = ["starting", "running", "restarting", "exit", "world.toml", "closed"];

/// The hello's "server", checked (read beside `read_hello`, from the same body): None when absent or not well formed.
pub fn read_server_up(body: &[u8]) -> Option<ServerUp> {
    let value: Value = serde_json::from_slice(body).ok()?;
    value.get("server").and_then(Value::as_object).and_then(read_server)
}

/// What the main window is told of a hello: its checked fields and the server's state (null when the world gave none).
pub fn hello_page(hello: &Hello, body: &[u8]) -> Value {
    let mut page = hello.page();
    page["server"] = serde_json::to_value(read_server_up(body)).unwrap_or(Value::Null);
    page
}

fn read_server(o: &Map<String, Value>) -> Option<ServerUp> {
    let reason = o.get("reason").and_then(Value::as_str).filter(|r| SERVER_REASONS.contains(r))?.to_string();
    Some(ServerUp {
        up: o.get("up").and_then(Value::as_bool)?,
        reason,
        code: o.get("code").and_then(Value::as_u64).filter(|c| *c <= 255).map(|c| c as u8),
        failures: o.get("failures").and_then(Value::as_u64).unwrap_or(0).min(99) as u8,
        retry_in: o.get("retryIn").and_then(Value::as_u64).map(|r| r.min(86_400) as u32),
    })
}
const SERVER_STATES: [&str; 4] = ["running", "maintenance", "host_only", "unknown"];

pub fn world_id(text: &str) -> bool {
    let hex = |s: &str| s.len() == 32 && s.bytes().all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b));
    text.strip_prefix("world-").map(hex).unwrap_or(false) || text.strip_prefix("local-").map(hex).unwrap_or(false)
}

/// Control, bidirectional and invisible characters, which could make a world's name look like the client's own words.
fn hidden(c: char) -> bool {
    c.is_control()
        || matches!(c, '\u{061c}' | '\u{200b}'..='\u{200f}' | '\u{2028}'..='\u{202e}' | '\u{2060}'..='\u{2069}' | '\u{feff}' | '\u{fff9}'..='\u{fffb}')
}

/// A world's name as the screens may show it: no hidden characters, spaces collapsed, at most 40 characters.
pub fn clean_name(name: &str) -> String {
    let kept: String = name.chars().filter(|c| !hidden(*c)).collect();
    kept.split_whitespace().collect::<Vec<_>>().join(" ").chars().take(40).collect()
}

pub fn read_hello(body: &[u8]) -> Result<Hello, &'static str> {
    if body.len() > HELLO_MOST {
        return Err("too-long");
    }
    let value: Value = serde_json::from_slice(body).map_err(|_| "not-json")?;
    let o = value.as_object().ok_or("not-json")?;
    let world = o.get("world").and_then(Value::as_str).filter(|w| world_id(w)).ok_or("world")?.to_string();
    let editor_api = o.get("editorApi").and_then(Value::as_u64).ok_or("api")?;
    let official = match o.get("kind").and_then(Value::as_str) {
        Some("community") => false,
        Some("official") => true,
        _ => return Err("kind"),
    };
    let name = clean_name(o.get("name").and_then(Value::as_str).unwrap_or(""));
    let package: String = o.get("package").and_then(Value::as_str).unwrap_or("").chars()
        .filter(|c| c.is_ascii_alphanumeric() || matches!(c, '.' | '-' | '_' | '+')).take(40).collect();
    let server_state = o.get("serverState").and_then(Value::as_str).filter(|s| SERVER_STATES.contains(s)).unwrap_or("unknown").to_string();
    let progress = match o.get("progress").and_then(Value::as_object) {
        Some(p) => Progress {
            // (A code this client doesn't know is "starting": no free text reaches the page.)
            code: p.get("code").and_then(Value::as_str).filter(|c| PROGRESS_CODES.contains(c)).unwrap_or("starting").to_string(),
            percent: p.get("percent").and_then(Value::as_f64).map(|v| v.clamp(0.0, 100.0) as u8).unwrap_or(0),
        },
        None => Progress { code: "ready".into(), percent: 100 },
    };
    let setup_pending = o.get("setupPending").and_then(Value::as_bool).unwrap_or(false);
    let rotation = o.get("rotation").and_then(Value::as_object).and_then(read_rotation);
    Ok(Hello { world, name, package, editor_api, official, server_state, progress, setup_pending, rotation })
}

fn read_rotation(r: &Map<String, Value>) -> Option<Rotation> {
    if r.get("algorithm").and_then(Value::as_str).is_some_and(|a| a != "ecdsa-p256-sha256") {
        return None;
    }
    let field = |k: &str| r.get(k).and_then(Value::as_str).filter(|s| s.len() <= 400);
    Some(Rotation {
        spki: super::tls::unb64(field("spki")?)?,
        signature: super::tls::unb64(field("signature")?)?,
        previous: field("previous")?.to_string(),
    })
}

impl Hello {
    /// What the main window is told: checked fields only, the rotation never.
    pub fn page(&self) -> Value {
        serde_json::json!({"world": self.world, "name": self.name, "package": self.package, "editorApi": self.editor_api,
            "kind": if self.official { "official" } else { "community" }, "serverState": self.server_state,
            "progress": self.progress, "setupPending": self.setup_pending})
    }

    pub fn ready(&self) -> bool {
        self.progress.code == "ready"
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Editor codes: `ratw-editor:` and base64url of {v: 1, w: world id, k: the key's pin, u: username, p?: one-time
// password, i?: invite, a?: address} (tools/world_admin.py, make_code and read_code).

#[derive(Debug, Clone, PartialEq)]
pub struct Code {
    pub world: String,
    pub pin: Pin,
    pub username: String,
    pub password: Option<String>,
    pub invite: Option<String>,
    pub address: Option<Address>,
}

pub fn username_ok(name: &str) -> bool {
    let b = name.as_bytes();
    (3..=32).contains(&b.len()) && b[0].is_ascii_lowercase() && b.iter().all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || *c == b'_' || *c == b'-')
}

pub fn read_code(text: &str) -> Result<Code, &'static str> {
    let text = text.trim();
    let body = text.strip_prefix(CODE_PREFIX).ok_or("prefix")?;
    if text.len() > 1024 {
        return Err("long");
    }
    let raw = super::tls::unb64url(body).ok_or("encoding")?;
    let value: Value = serde_json::from_slice(&raw).map_err(|_| "json")?;
    let o = value.as_object().ok_or("json")?;
    if o.get("v").and_then(Value::as_u64) != Some(1) {
        return Err("version");
    }
    let world = o.get("w").and_then(Value::as_str).filter(|w| world_id(w)).ok_or("world")?.to_string();
    let pin = o.get("k").and_then(Value::as_str).and_then(pin_from_text).ok_or("key")?;
    let username = o.get("u").and_then(Value::as_str).filter(|u| username_ok(u)).ok_or("username")?.to_string();
    let secret = |k: &str, most: usize| -> Result<Option<String>, &'static str> {
        match o.get(k) {
            None | Some(Value::Null) => Ok(None),
            Some(Value::String(s)) if !s.is_empty() && s.len() <= most && s.bytes().all(|b| b.is_ascii_graphic()) => Ok(Some(s.clone())),
            _ => Err("secret"),
        }
    };
    let password = secret("p", 256)?;
    let invite = secret("i", 200)?;
    if password.is_none() && invite.is_none() {
        return Err("secret");
    }
    let address = match o.get("a").and_then(Value::as_str) {
        Some(a) => Some(parse_address(a).map_err(|_| "address")?),
        None => None,
    };
    Ok(Code { world, pin, username, password, invite, address })
}

/// A code the world made (screen 5's add and reset), with the address this client reaches the world at added, unless
/// that is this computer (useless to anyone else). The record is otherwise kept as it came.
pub fn code_with_address(code: &str, address: &Address, place: Place) -> String {
    if place == Place::Loopback {
        return code.to_string();
    }
    let Some(body) = code.strip_prefix(CODE_PREFIX) else { return code.to_string() };
    let Some(mut record) = super::tls::unb64url(body).and_then(|r| serde_json::from_slice::<Map<String, Value>>(&r).ok()) else {
        return code.to_string();
    };
    if record.contains_key("a") {
        return code.to_string();
    }
    record.insert("a".into(), Value::String(address.shown.clone()));
    format!("{CODE_PREFIX}{}", super::tls::b64url(serde_json::to_string(&record).unwrap_or_default().as_bytes()))
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn addresses_default_to_the_editor_port() {
        let a = parse_address(" 192.168.1.20 ").unwrap();
        assert_eq!((a.host.as_str(), a.port, a.shown.as_str()), ("192.168.1.20", 17789, "192.168.1.20:17789"));
        assert_eq!(parse_address("MyWorld.Example.net:17800").unwrap().shown, "myworld.example.net:17800");
        assert_eq!(parse_address("localhost").unwrap().shown, "127.0.0.1:17789", "localhost is tried as 127.0.0.1");
        assert_eq!(parse_address("localhost:17790").unwrap().shown, "127.0.0.1:17790");
        assert_eq!(parse_address("[2001:db8::1]:17789").unwrap().shown, "[2001:db8::1]:17789");
        assert_eq!(parse_address("2001:db8::1").unwrap().shown, "[2001:db8::1]:17789");
        for bad in ["", "a b", "http://x", "x:0", "x:99999", "x:port", "-x", "x/y", "[::1", &"a".repeat(261), "[nothost]:1"] {
            assert!(parse_address(bad).is_err(), "{bad}");
        }
    }

    #[test]
    fn addresses_are_placed_for_screen_3() {
        let p = |s: &str| place(s.parse().unwrap());
        assert_eq!(p("127.0.0.1"), Place::Loopback);
        assert_eq!(p("::1"), Place::Loopback);
        assert_eq!(p("192.168.1.20"), Place::Home);
        assert_eq!(p("10.0.0.5"), Place::Home);
        assert_eq!(p("172.20.1.1"), Place::Home);
        assert_eq!(p("100.100.1.1"), Place::Home);
        assert_eq!(p("fd00::1"), Place::Home);
        assert_eq!(p("203.0.113.7"), Place::Public);
        assert_eq!(p("2001:db8::1"), Place::Public);
        assert!(resolve(&parse_address("169.254.169.254").unwrap()).is_err(), "never a link-local address");
        assert!(resolve(&parse_address("0.0.0.0").unwrap()).is_err());
        assert_eq!(base_url(&resolve(&parse_address("127.0.0.1:5").unwrap()).unwrap()[0]), "https://127.0.0.1:5");
    }

    fn hello(extra: Value) -> Value {
        let mut h = json!({"world": format!("world-{}", "a".repeat(32)), "name": "Ashfall", "package": "1.2.3", "editorApi": 1,
            "kind": "community", "serverState": "running", "progress": {"code": "ready", "percent": 100}, "setupPending": false});
        for (k, v) in extra.as_object().unwrap() {
            h[k] = v.clone();
        }
        h
    }

    fn read(v: Value) -> Result<Hello, &'static str> {
        read_hello(v.to_string().as_bytes())
    }

    #[test]
    fn a_hello_is_checked_and_trimmed() {
        let h = read(hello(json!({}))).unwrap();
        assert_eq!(h.name, "Ashfall");
        assert!(h.ready() && !h.official);
        // A 200-character name, bidirectional and control characters, free-text progress, unknown fields.
        let h = read(hello(json!({"name": format!("\u{202e}Evil\u{0007}\n {}", "x".repeat(200)), "progress": {"code": "Click here to sign in", "percent": 400},
            "serverState": "<b>", "package": "1.0 <script>", "extra": "dropped", "token": "nope"}))).unwrap();
        assert_eq!(h.name.chars().count(), 40);
        assert!(h.name.starts_with("Evil x"), "{}", h.name);
        assert!(!h.name.chars().any(hidden));
        assert_eq!(h.progress, Progress { code: "starting".into(), percent: 100 });
        assert_eq!(h.server_state, "unknown");
        assert_eq!(h.package, "1.0script");
        let page = h.page();
        assert!(page.get("extra").is_none() && page.get("token").is_none() && page.get("rotation").is_none());
        assert_eq!(page.as_object().unwrap().len(), 8);
        // The server's state: known reasons and bounded numbers only; null when absent or not well formed.
        let page_of = |extra: Value| {
            let body = hello(extra).to_string();
            hello_page(&read_hello(body.as_bytes()).unwrap(), body.as_bytes())
        };
        assert_eq!(page_of(json!({}))["server"], Value::Null);
        assert_eq!(page_of(json!({"server": {"up": false, "reason": "exit", "code": 11, "failures": 3, "retryIn": 120, "extra": "x"}}))["server"],
            json!({"up": false, "reason": "exit", "code": 11, "failures": 3, "retryIn": 120}));
        assert_eq!(page_of(json!({"server": {"up": false, "reason": "world.toml", "code": 9999, "failures": 500, "retryIn": 1e12}}))["server"],
            json!({"up": false, "reason": "world.toml", "code": null, "failures": 99, "retryIn": null}));
        assert_eq!(page_of(json!({"server": {"up": false, "reason": "<b>crashed</b>"}}))["server"], Value::Null);
        assert_eq!(page_of(json!({"server": {"reason": "exit"}}))["server"], Value::Null);
        assert_eq!(page_of(json!({"server": "up"}))["server"], Value::Null);
        assert_eq!(read(hello(json!({"progress": {"code": "slow", "percent": 100}}))).unwrap().progress.code, "slow");
        // Refused outright: no world id, an unknown kind, too long, not JSON.
        assert!(read(hello(json!({"world": "world-x"}))).is_err());
        assert!(read(hello(json!({"kind": "evil"}))).is_err());
        assert!(read(hello(json!({"editorApi": "1"}))).is_err());
        assert!(read_hello(&vec![b' '; 1025]).is_err());
        assert!(read_hello(b"<html>").is_err());
        assert!(read(hello(json!({"kind": "official"}))).unwrap().official);
    }

    fn code(record: Value) -> String {
        format!("{CODE_PREFIX}{}", super::super::tls::b64url(record.to_string().as_bytes()))
    }

    #[test]
    fn editor_codes() {
        let k = super::super::tls::b64url(&[7u8; 32]);
        let w = format!("local-{}", "0".repeat(32));
        let c = read_code(&code(json!({"v": 1, "w": w, "k": k, "u": "host-k7q2", "p": "pw-123", "i": "inv", "a": "192.168.1.20"}))).unwrap();
        assert_eq!((c.username.as_str(), c.pin, c.password.as_deref(), c.invite.as_deref()), ("host-k7q2", [7u8; 32], Some("pw-123"), Some("inv")));
        assert_eq!(c.address.unwrap().shown, "192.168.1.20:17789");
        assert!(read_code(&code(json!({"v": 1, "w": w, "k": k, "u": "pell", "i": "inv"}))).unwrap().address.is_none());
        for bad in [json!({"v": 2, "w": w, "k": k, "u": "pell", "i": "x"}), json!({"v": 1, "w": "world-1", "k": k, "u": "pell", "i": "x"}),
            json!({"v": 1, "w": w, "k": "short", "u": "pell", "i": "x"}), json!({"v": 1, "w": w, "k": k, "u": "Pell!", "i": "x"}),
            json!({"v": 1, "w": w, "k": k, "u": "pell"}), json!({"v": 1, "w": w, "k": k, "u": "pell", "p": "has space"}),
            json!({"v": 1, "w": w, "k": k, "u": "pell", "i": "x", "a": "bad address"})] {
            assert!(read_code(&code(bad.clone())).is_err(), "{bad}");
        }
        assert!(read_code("192.168.1.20").is_err());
        assert!(read_code(&format!("{CODE_PREFIX}!!!")).is_err());
        // An address added to a code made on screen 5, but never this computer's.
        let made = code(json!({"i": "inv", "k": k, "p": "pw", "u": "pell", "v": 1, "w": w}));
        let home = parse_address("192.168.1.20").unwrap();
        let with = read_code(&code_with_address(&made, &home, Place::Home)).unwrap();
        assert_eq!(with.address.unwrap().shown, "192.168.1.20:17789");
        assert_eq!(code_with_address(&made, &parse_address("127.0.0.1").unwrap(), Place::Loopback), made);
    }
}
