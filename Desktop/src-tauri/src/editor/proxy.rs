// The `editor` URI scheme (Docs/Design/78, 6.6): the Editor window's every request, proxied by the shell to its world
// over the pinned client, with the session's bearer token added here. The page never holds the token (I4), can add no
// header of its own, and runs only signed files (I6).
//
// The rules, each tested below:
// - only the window labelled `editor`, and only while it is bound to the current session (I10): a window from another
//   world or account is refused (403); one whose session ended gets 401 (it keeps its queued saves until the same account
//   signs in again);
// - GET and POST only; never `/editor/*` (the account routes are the client's own screen's, never a page's);
// - sent on: the method, the path and query, the body (up to 16 MB; Content-Length set from it, since neither webview
//   hands it over, M0a), Content-Type and Accept; never Cookie, Origin, Authorization or Host from the page;
// - replies up to 32 MB on the wire and 32 MB after gunzip; no redirects (the agent follows none); 60 s;
// - a page file must be on the signed list and hash to it, and its type comes from its name; `/api/` replies are JSON
//   (or octet-stream for ground) with nosniff, whatever the world said; the shell's own CSP on every reply;
// - a 401 from the world tells the main window (the menu shows "Your Editor session ended").
use std::io::Read;

use super::bundle::{content_type, file_of, Manifest};

pub const MAX_REQUEST: usize = 16 * 1024 * 1024;
pub const MAX_REPLY: u64 = 32 * 1024 * 1024;
/// The page's CSP, set by the shell whatever the world sends (no inline script, no eval, nothing from elsewhere).
pub const CSP: &str = "default-src 'self'; script-src 'self'; connect-src 'self'; img-src 'self' blob: data:; form-action 'none'; \
                       base-uri 'none'; object-src 'none'; frame-src 'none'; frame-ancestors 'none'";
/// The shell's own path in the Editor window's origin: the input script's "the host did something" (the keep-alive's
/// gate). Never forwarded.
pub const INPUT_PATH: &str = "/.ratw-shell/input";
/// An e2e build's path for the Editor window's replies to the test (src/e2e.rs). Refused in every other build.
pub const E2E_PATH: &str = "/.ratw-shell/e2e";

/// Runs in the Editor window before the page (the shell's script, not the world's): tells the shell, at most every 20
/// seconds, that the host pressed a key, clicked or scrolled. The keep-alive renews the session only after such input.
pub const INPUT_SCRIPT: &str = "(() => { let last = 0; const ping = (e) => { if (!e.isTrusted) return; const now = Date.now(); if (now - last < 20000) return; last = now; \
    try { fetch('/.ratw-shell/input', {method: 'POST', keepalive: true}).catch(() => {}); } catch (e) {} }; \
    for (const t of ['keydown', 'pointerdown', 'wheel']) addEventListener(t, ping, {capture: true, passive: true}); })();";

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Binding {
    /// The window belongs to the current, live session.
    Current,
    /// The window's session ended (timed out, ended by the owner); the same account may sign in again and keep it.
    Ended,
    /// No session, or the window belongs to another world or account.
    Stale,
}

#[derive(Debug, Clone, Default)]
pub struct Asked {
    pub method: String,
    pub path: String,
    pub content_type: Option<String>,
    pub accept: Option<String>,
    pub body: Vec<u8>,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Answer {
    pub status: u16,
    pub content_type: &'static str,
    pub body: Vec<u8>,
    /// The world said the session is over (a 401): the main window is told.
    pub ended: bool,
    /// The input script's ping.
    pub input: bool,
}

impl Answer {
    fn text(status: u16, message: &str) -> Answer {
        Answer { status, content_type: "application/json", body: serde_json::json!({"error": message}).to_string().into_bytes(), ended: false, input: false }
    }

    /// The shell's own page while the world doesn't answer: it asks again every 5 seconds by itself.
    fn restarting() -> Answer {
        let body = "<!doctype html><html><head><meta charset=\"utf-8\"><meta http-equiv=\"refresh\" content=\"5\"><title>RATW Editor</title></head>\
                    <body style=\"background:#141613;color:#e8e1d3;font:16px system-ui;padding:3em\"><p>Restarting…</p>\
                    <p style=\"color:#a59f90\">Your world is restarting: the Editor reconnects by itself.</p></body></html>";
        Answer { status: 503, content_type: "text/html; charset=utf-8", body: body.as_bytes().to_vec(), ended: false, input: false }
    }

    /// The shell's own page in place of a file it refused.
    fn page(status: u16, message: &str) -> Answer {
        let body = format!("<!doctype html><html><head><meta charset=\"utf-8\"><title>RATW Editor</title></head>\
                            <body style=\"background:#141613;color:#e8e1d3;font:16px system-ui;padding:3em\"><p>{message}</p></body></html>");
        Answer { status, content_type: "text/html; charset=utf-8", body: body.into_bytes(), ended: false, input: false }
    }
}

/// The API's answer while the world doesn't answer (503): a code the Editor page words itself.
pub const RESTARTING: &str = "restarting";
pub const NOT_OFFICIAL: &str = "This world's Editor isn't an official RATW build.";
pub const ENDED: &str = "Your Editor session ended: sign in again from the Editor menu.";

/// What came back from the world, before decompression.
#[derive(Debug, Clone)]
pub struct Fetched {
    pub status: u16,
    pub content_type: Option<String>,
    pub encoding: Option<String>,
    pub body: Vec<u8>,
}

/// One exchange with the world (the pinned client, the bearer and Accept-Encoding added by it).
pub trait Upstream {
    fn send(&self, method: &str, path: &str, content_type: Option<&str>, accept: Option<&str>, body: Option<&[u8]>) -> Result<Fetched, String>;
}

/// A header value the page may pass on: short, printable, one line.
fn header_ok(v: &Option<String>) -> Option<&str> {
    v.as_deref().filter(|v| v.len() <= 200 && v.bytes().all(|b| (0x20..0x7f).contains(&b)))
}

fn path_ok(path: &str) -> bool {
    let only_path = path.split(['?', '#']).next().unwrap_or("");
    path.starts_with('/') && !path.starts_with("//") && path.len() <= 4096 && !path.contains('\\')
        && path.bytes().all(|b| b.is_ascii_graphic())
        // (A segment of dots, written plain or encoded or both: `.%2e` too, R1 review.)
        && !only_path.split('/').any(|p| matches!(p.to_ascii_lowercase().replace("%2e", ".").as_str(), "." | ".."))
}

/// The body as the page should get it: gunzipped when the world gzipped it, 32 MB at most either way.
pub fn decoded(fetched: &Fetched) -> Result<Vec<u8>, &'static str> {
    if fetched.body.len() as u64 > MAX_REPLY {
        return Err("over 32 MB");
    }
    match fetched.encoding.as_deref().map(str::to_ascii_lowercase).as_deref() {
        None | Some("") | Some("identity") => Ok(fetched.body.clone()),
        Some("gzip") => {
            let mut out = Vec::new();
            flate2::read::GzDecoder::new(&fetched.body[..]).take(MAX_REPLY + 1).read_to_end(&mut out).map_err(|_| "bad gzip")?;
            if out.len() as u64 > MAX_REPLY {
                return Err("over 32 MB after decompression");
            }
            Ok(out)
        }
        Some(_) => Err("an encoding the shell didn't ask for"),
    }
}

pub fn handle(label: &str, binding: Binding, asked: &Asked, manifest: Option<&Manifest>, upstream: &dyn Upstream) -> Answer {
    if label != "editor" {
        return Answer::text(403, "Only the Editor window uses the editor scheme.");
    }
    let api = asked.path.starts_with("/api/");
    match binding {
        Binding::Current => {}
        Binding::Ended if api || asked.path == INPUT_PATH => return Answer { ended: true, ..Answer::text(401, ENDED) },
        Binding::Ended => return Answer::page(401, ENDED),
        Binding::Stale => return Answer::text(403, "This Editor window belongs to another session."),
    }
    if asked.method != "GET" && asked.method != "POST" {
        return Answer::text(405, "GET and POST only.");
    }
    if !path_ok(&asked.path) {
        return Answer::text(400, "Not a path the Editor uses.");
    }
    let only_path = asked.path.split(['?', '#']).next().unwrap_or("");
    if only_path == "/editor" || only_path.starts_with("/editor/") {
        return Answer::text(403, "Editor accounts are managed from the Editor menu.");
    }
    if only_path == INPUT_PATH {
        return Answer { status: 204, content_type: "application/json", body: vec![], ended: false, input: true };
    }
    if asked.body.len() > MAX_REQUEST {
        return Answer::text(413, "A request is 16 MB at most.");
    }
    if api {
        let body = (asked.method == "POST").then_some(asked.body.as_slice());
        let content_type = if asked.method == "POST" { header_ok(&asked.content_type) } else { None };
        let fetched = match upstream.send(&asked.method, &asked.path, content_type, header_ok(&asked.accept), body) {
            Ok(f) => f,
            // The world didn't answer: most often its container restarting. The page says "Restarting…" and keeps
            // asking (Editor/src: the restarting banner); it keeps its state meanwhile.
            Err(_) => return Answer::text(503, RESTARTING),
        };
        let data = match decoded(&fetched) {
            Ok(d) => d,
            Err(why) => return Answer::text(502, why),
        };
        // No API reply can run as script: JSON, or raw bytes for ground.
        let kind = if fetched.content_type.as_deref().is_some_and(|c| c.trim().eq_ignore_ascii_case("application/octet-stream")) {
            "application/octet-stream"
        } else {
            "application/json"
        };
        return Answer { status: fetched.status, content_type: kind, body: data, ended: fetched.status == 401, input: false };
    }
    // A page file: GET only, on the signed list, hashing to it.
    if asked.method != "GET" {
        return Answer::text(405, "Page files are read only.");
    }
    let (Some(manifest), Some(file)) = (manifest, file_of(&asked.path)) else { return Answer::page(404, NOT_OFFICIAL) };
    if !manifest.lists(&file) {
        return Answer::page(404, "Not part of this world's Editor.");
    }
    let Some(kind) = content_type(&file) else { return Answer::page(404, NOT_OFFICIAL) };
    let fetched = match upstream.send("GET", &asked.path, None, None, None) {
        Ok(f) => f,
        Err(_) => return Answer::restarting(),
    };
    if fetched.status == 401 {
        return Answer { ended: true, ..Answer::page(401, ENDED) };
    }
    if fetched.status != 200 {
        return Answer::page(fetched.status.clamp(400, 599), "The world couldn't send this part of its Editor.");
    }
    match decoded(&fetched) {
        Ok(data) if manifest.holds(&file, &data) => Answer { status: 200, content_type: kind, body: data, ended: false, input: false },
        _ => Answer::page(403, NOT_OFFICIAL),
    }
}

/// The reply the webview gets: the status, the shell's type, nosniff, the shell's CSP, never cached, never a referrer.
pub fn response(answer: Answer) -> tauri::http::Response<Vec<u8>> {
    tauri::http::Response::builder()
        .status(answer.status)
        .header("Content-Type", answer.content_type)
        .header("X-Content-Type-Options", "nosniff")
        .header("Content-Security-Policy", CSP)
        .header("Cache-Control", "no-store")
        .header("Referrer-Policy", "no-referrer")
        .body(answer.body)
        .unwrap_or_else(|_| tauri::http::Response::new(Vec::new()))
}

/// The request as the webview handed it, read into an `Asked` (only what may be passed on).
pub fn asked(request: &tauri::http::Request<Vec<u8>>) -> Asked {
    let header = |name: &str| request.headers().get(name).and_then(|v| v.to_str().ok()).map(str::to_string);
    Asked {
        method: request.method().as_str().to_string(),
        path: request.uri().path_and_query().map(|p| p.as_str().to_string()).unwrap_or_else(|| "/".into()),
        content_type: header("content-type"),
        accept: header("accept"),
        body: request.body().clone(),
    }
}

/// The real world: the pinned agent, the session's bearer token, `Accept-Encoding: gzip`.
pub struct World {
    pub agent: ureq::Agent,
    pub base: String,
    pub token: String,
}

impl Upstream for World {
    fn send(&self, method: &str, path: &str, content_type: Option<&str>, accept: Option<&str>, body: Option<&[u8]>) -> Result<Fetched, String> {
        let url = format!("{}{path}", self.base);
        let bearer = format!("Bearer {}", self.token);
        let result = match method {
            "GET" => {
                let mut r = self.agent.get(&url).header("Authorization", &bearer).header("Accept-Encoding", "gzip");
                if let Some(a) = accept {
                    r = r.header("Accept", a);
                }
                r.call()
            }
            "POST" => {
                let mut r = self.agent.post(&url).header("Authorization", &bearer).header("Accept-Encoding", "gzip");
                if let Some(a) = accept {
                    r = r.header("Accept", a);
                }
                if let Some(c) = content_type {
                    r = r.header("Content-Type", c);
                }
                // (ureq sets Content-Length from the body: the webviews give none, M0a.)
                r.send(body.unwrap_or(&[]))
            }
            _ => return Err("method".into()),
        };
        let response = result.map_err(|e| super::tls::error_text(&e))?;
        let status = response.status().as_u16();
        let header = |name: &str| response.headers().get(name).and_then(|v| v.to_str().ok()).map(str::to_string);
        let (content_type, encoding) = (header("content-type"), header("content-encoding"));
        let mut wire = Vec::new();
        response.into_body().into_reader().take(MAX_REPLY + 1).read_to_end(&mut wire).map_err(|e| e.to_string())?;
        Ok(Fetched { status, content_type, encoding, body: wire })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::editor::bundle::tests::signed;
    use crate::editor::bundle::{verify, OLDEST};
    use std::io::Write;
    use std::sync::Mutex;

    struct Fake {
        seen: Mutex<Vec<(String, String, Option<String>, Option<String>, usize)>>,
        reply: Box<dyn Fn(&str) -> Fetched + Send + Sync>,
    }

    impl Upstream for Fake {
        fn send(&self, method: &str, path: &str, content_type: Option<&str>, accept: Option<&str>, body: Option<&[u8]>) -> Result<Fetched, String> {
            self.seen.lock().unwrap().push((method.into(), path.into(), content_type.map(Into::into), accept.map(Into::into), body.map_or(0, |b| b.len())));
            Ok((self.reply)(path))
        }
    }

    fn fake(reply: impl Fn(&str) -> Fetched + Send + Sync + 'static) -> Fake {
        Fake { seen: Mutex::new(vec![]), reply: Box::new(reply) }
    }

    fn plain(status: u16, content_type: &str, body: &[u8]) -> Fetched {
        Fetched { status, content_type: Some(content_type.into()), encoding: None, body: body.to_vec() }
    }

    fn get(path: &str) -> Asked {
        Asked { method: "GET".into(), path: path.into(), ..Default::default() }
    }

    fn gzip(data: &[u8]) -> Vec<u8> {
        let mut e = flate2::write::GzEncoder::new(Vec::new(), flate2::Compression::fast());
        e.write_all(data).unwrap();
        e.finish().unwrap()
    }

    fn manifest() -> Manifest {
        let (bundle, keys) = signed(&[("index.html", b"<html>page</html>"), ("assets/app.js", b"run()")], OLDEST);
        verify(&bundle, &keys, OLDEST, 1).unwrap()
    }

    #[test]
    fn only_the_editor_window_bound_to_the_current_session() {
        let world = fake(|_| plain(200, "application/json", b"{}"));
        let m = manifest();
        for label in ["main", "editor2", ""] {
            assert_eq!(handle(label, Binding::Current, &get("/api/x"), Some(&m), &world).status, 403, "{label}");
        }
        assert_eq!(handle("editor", Binding::Stale, &get("/api/x"), Some(&m), &world).status, 403, "another session's window");
        let ended = handle("editor", Binding::Ended, &get("/api/x"), Some(&m), &world);
        assert_eq!((ended.status, ended.ended), (401, true), "an ended session: the page keeps its saves");
        assert!(world.seen.lock().unwrap().is_empty(), "none of them reached the world");
        assert_eq!(handle("editor", Binding::Current, &get("/api/x"), Some(&m), &world).status, 200);
    }

    #[test]
    fn the_page_adds_no_header_and_never_reaches_the_account_routes() {
        let world = fake(|_| plain(200, "application/json", b"{}"));
        let m = manifest();
        let post = Asked { method: "POST".into(), path: "/api/dm/act?x=1".into(), content_type: Some("application/json".into()),
            accept: Some("application/json".into()), body: b"{\"a\":1}".to_vec() };
        assert_eq!(handle("editor", Binding::Current, &post, Some(&m), &world).status, 200);
        let seen = world.seen.lock().unwrap().clone();
        assert_eq!(seen, vec![("POST".into(), "/api/dm/act?x=1".into(), Some("application/json".into()), Some("application/json".into()), 7)]);
        for path in ["/editor/accounts", "/editor/accounts/add", "/editor/login", "/editor", "/editor/keepalive?x"] {
            for method in ["GET", "POST"] {
                let a = Asked { method: method.into(), path: path.into(), ..Default::default() };
                assert_eq!(handle("editor", Binding::Current, &a, Some(&m), &world).status, 403, "{method} {path}");
            }
        }
        for method in ["PUT", "DELETE", "OPTIONS", "PATCH"] {
            let a = Asked { method: method.into(), path: "/api/x".into(), ..Default::default() };
            assert_eq!(handle("editor", Binding::Current, &a, Some(&m), &world).status, 405);
        }
        for path in ["//evil.example/x", "/api/../editor/accounts", "/a\\b", "/api/%2e%2e/x", "/api/.%2e/x", "/api/%2E./x", "/api/%2e/x", "relative"] {
            assert_eq!(handle("editor", Binding::Current, &get(path), Some(&m), &world).status, 400, "{path}");
        }
        assert_eq!(world.seen.lock().unwrap().len(), 1);
        // A header with a line break in it isn't passed on.
        let bad = Asked { method: "POST".into(), path: "/api/x".into(), content_type: Some("text/plain\r\nCookie: a=b".into()), ..Default::default() };
        handle("editor", Binding::Current, &bad, Some(&m), &world);
        assert_eq!(world.seen.lock().unwrap().last().unwrap().2, None);
    }

    #[test]
    fn a_world_that_doesnt_answer_is_restarting() {
        struct Down;
        impl Upstream for Down {
            fn send(&self, _: &str, _: &str, _: Option<&str>, _: Option<&str>, _: Option<&[u8]>) -> Result<Fetched, String> {
                Err("connection refused".into())
            }
        }
        let m = manifest();
        let api = handle("editor", Binding::Current, &get("/api/dm/live"), Some(&m), &Down);
        assert_eq!((api.status, api.content_type, api.ended), (503, "application/json", false), "not an ended session");
        assert_eq!(serde_json::from_slice::<serde_json::Value>(&api.body).unwrap()["error"], RESTARTING);
        let page = handle("editor", Binding::Current, &get("/"), Some(&m), &Down);
        let text = String::from_utf8(page.body).unwrap();
        assert_eq!(page.status, 503);
        assert!(text.contains("Restarting") && text.contains("http-equiv=\"refresh\""), "{text}");
    }

    #[test]
    fn sizes_are_capped_both_ways() {
        let m = manifest();
        let world = fake(|path| match path {
            "/api/big" => plain(200, "application/json", &vec![b' '; MAX_REPLY as usize + 1]),
            "/api/bomb" => Fetched { status: 200, content_type: None, encoding: Some("gzip".into()), body: gzip(&vec![b'a'; MAX_REPLY as usize + 1]) },
            "/api/gz" => Fetched { status: 200, content_type: Some("text/html".into()), encoding: Some("gzip".into()), body: gzip(&vec![b'a'; 1 << 20]) },
            "/api/br" => Fetched { status: 200, content_type: None, encoding: Some("br".into()), body: b"x".to_vec() },
            _ => plain(200, "application/json", b"{}"),
        });
        let post = |n: usize| Asked { method: "POST".into(), path: "/api/x".into(), body: vec![0; n], ..Default::default() };
        assert_eq!(handle("editor", Binding::Current, &post(MAX_REQUEST), Some(&m), &world).status, 200, "16 MB passes");
        assert_eq!(handle("editor", Binding::Current, &post(MAX_REQUEST + 1), Some(&m), &world).status, 413);
        assert_eq!(world.seen.lock().unwrap().last().unwrap().4, MAX_REQUEST, "the body went whole");
        assert_eq!(handle("editor", Binding::Current, &get("/api/big"), Some(&m), &world).status, 502, "over 32 MB on the wire");
        assert_eq!(handle("editor", Binding::Current, &get("/api/bomb"), Some(&m), &world).status, 502, "over 32 MB after gunzip");
        assert_eq!(handle("editor", Binding::Current, &get("/api/br"), Some(&m), &world).status, 502);
        let gz = handle("editor", Binding::Current, &get("/api/gz"), Some(&m), &world);
        assert_eq!((gz.status, gz.body.len(), gz.content_type), (200, 1 << 20, "application/json"), "gunzipped, and forced to JSON");
    }

    #[test]
    fn api_replies_never_run_as_script() {
        let m = manifest();
        let world = fake(|path| match path {
            "/api/ground" => plain(200, "application/octet-stream", b"\x00\x01"),
            "/api/script" => plain(200, "text/html", b"<script>alert(1)</script>"),
            "/api/gone" => plain(401, "application/json", b"{\"error\":\"signed-out\"}"),
            _ => plain(200, "text/javascript", b"run()"),
        });
        assert_eq!(handle("editor", Binding::Current, &get("/api/ground"), Some(&m), &world).content_type, "application/octet-stream");
        assert_eq!(handle("editor", Binding::Current, &get("/api/script"), Some(&m), &world).content_type, "application/json");
        assert_eq!(handle("editor", Binding::Current, &get("/api/js"), Some(&m), &world).content_type, "application/json");
        let gone = handle("editor", Binding::Current, &get("/api/gone"), Some(&m), &world);
        assert_eq!((gone.status, gone.ended), (401, true), "a 401 reaches the menu");
        let r = response(handle("editor", Binding::Current, &get("/api/script"), Some(&m), &world));
        assert_eq!(r.headers()["x-content-type-options"], "nosniff");
        assert_eq!(r.headers()["content-security-policy"], CSP);
        assert!(r.headers().get("set-cookie").is_none());
    }

    #[test]
    fn only_signed_page_files_pass() {
        let m = manifest();
        let world = fake(|path| match path {
            "/" | "/?suite=dm" => plain(200, "application/octet-stream", b"<html>page</html>"),
            "/assets/app.js" => Fetched { status: 200, content_type: Some("text/plain".into()), encoding: Some("gzip".into()), body: gzip(b"run()") },
            "/assets/evil.js" => plain(200, "text/javascript", b"evil()"),
            "/assets/changed.js" => plain(200, "text/javascript", b"evil()"),
            _ => plain(404, "application/json", b"{}"),
        });
        let page = handle("editor", Binding::Current, &get("/?suite=dm"), Some(&m), &world);
        assert_eq!((page.status, page.content_type), (200, "text/html; charset=utf-8"), "its type from its name, never the world's");
        let js = handle("editor", Binding::Current, &get("/assets/app.js"), Some(&m), &world);
        assert_eq!((js.status, js.content_type, js.body.as_slice()), (200, "text/javascript; charset=utf-8", &b"run()"[..]));
        assert_eq!(handle("editor", Binding::Current, &get("/assets/evil.js"), Some(&m), &world).status, 404, "not on the signed list");
        let mut changed = m.clone();
        changed.files.insert("assets/changed.js".into(), crate::editor::tls::sha256(b"run()"));
        let refused = handle("editor", Binding::Current, &get("/assets/changed.js"), Some(&changed), &world);
        assert_eq!(refused.status, 403);
        assert!(String::from_utf8_lossy(&refused.body).contains(NOT_OFFICIAL));
        assert_eq!(handle("editor", Binding::Current, &get("/"), None, &world).status, 404, "no signed bundle, no page");
        let post = Asked { method: "POST".into(), path: "/".into(), ..Default::default() };
        assert_eq!(handle("editor", Binding::Current, &post, Some(&m), &world).status, 405);
        // Never asked of the world: files not on the list.
        assert!(!world.seen.lock().unwrap().iter().any(|s| s.1 == "/assets/evil.js"));
    }

    #[test]
    fn the_input_ping_stays_in_the_shell() {
        let world = fake(|_| plain(200, "application/json", b"{}"));
        let a = Asked { method: "POST".into(), path: INPUT_PATH.into(), ..Default::default() };
        let r = handle("editor", Binding::Current, &a, None, &world);
        assert_eq!((r.status, r.input), (204, true));
        assert!(world.seen.lock().unwrap().is_empty());
        assert_eq!(handle("main", Binding::Current, &a, None, &world).input, false);
    }

    #[test]
    fn the_real_upstream_sends_the_bearer_and_content_length_and_no_page_headers() {
        use crate::editor::tls::tests::{ok_reply, pin_a, server_config, TestWorld, CERT_A, KEY_A};
        let world = TestWorld::start(Some(server_config(CERT_A, KEY_A, false)), ok_reply("{\"ok\":true}"));
        let (agent, _) = crate::editor::tls::agent(crate::editor::tls::Mode::Exact(pin_a()), std::time::Duration::from_secs(10));
        let up = World { agent, base: world.url(), token: "T0KEN".into() };
        let f = up.send("POST", "/api/x?y=1", Some("application/json"), Some("*/*"), Some(b"{\"a\":1}")).unwrap();
        assert_eq!(f.status, 200);
        let seen = world.requests();
        let h = |k: &str| seen[0].headers.iter().find(|(n, _)| n == k).map(|(_, v)| v.clone());
        assert_eq!(seen[0].path, "/api/x?y=1");
        assert_eq!(h("authorization").as_deref(), Some("Bearer T0KEN"));
        assert_eq!(h("content-length").as_deref(), Some("7"));
        assert_eq!(h("accept-encoding").as_deref(), Some("gzip"));
        for never in ["cookie", "origin", "referer"] {
            assert!(h(never).is_none(), "{never}");
        }
        assert!(!seen[0].headers.iter().any(|(k, _)| k.starts_with("sec-fetch")));
    }
}
