// Doc 78, Phase M0a: the Editor's spike. Not the client, and not shipped.
//
// What it measures, one JSON line a check (SPIKE_REPORT, a .jsonl file):
// - the pinned client: ureq 3.5.1's Agent::with_parts with a copy of its RustlsConnector holding a rustls config with a
//   custom ServerCertVerifier (the SHA-256 of the end entity's SPKI against a pin; TLS 1.3 only; the handshake's
//   signature checked by rustls::crypto::verify_tls13_signature) against world_stub.py's self-signed P-256 key: the
//   right pin, a wrong pin, a TLS 1.2-only port, and "record" mode (any key, its pin recorded);
// - the `editor` URI scheme (register_asynchronous_uri_scheme_protocol): webview_label() in the handler (the main window
//   is refused), the built studio app (Editor/dist) proxied through it from the stand-in, POST bodies up to 16 MB with
//   Content-Length, replies up to 32 MB raw and gzipped (decompressed here, both sizes capped), incognito windows
//   (storage gone with the window; a persistent pair as the control), on_download, and the Editor window's lack of IPC.
//
// Environment: SPIKE_URL (https://127.0.0.1:47891), SPIKE_URL12 (the TLS 1.2-only port), SPIKE_PIN (hex), SPIKE_REPORT,
// SPIKE_QUIT=1 to quit when done (and after SPIKE_DEADLINE seconds, 300 by default, whatever happened).
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use std::fs::{File, OpenOptions};
use std::io::{Read, Write};
use std::sync::{Arc, Mutex, OnceLock};
use std::time::{Duration, Instant};

use rustls::client::danger::{HandshakeSignatureValid, ServerCertVerified, ServerCertVerifier};
use rustls::crypto::WebPkiSupportedAlgorithms;
use rustls::pki_types::{CertificateDer, ServerName, UnixTime};
use rustls::{ClientConfig, ClientConnection, DigitallySignedStruct, SignatureScheme, StreamOwned};
use serde_json::{json, Value};
use tauri::webview::DownloadEvent;
use tauri::{AppHandle, Manager, WebviewUrl, WebviewWindowBuilder};
use ureq::unversioned::resolver::DefaultResolver;
use ureq::unversioned::transport::{
    Buffers, ConnectionDetails, Connector, Either, LazyBuffers, NextTimeout, TcpConnector, Transport, TransportAdapter,
};

const MAX_REQUEST: usize = 16 * 1024 * 1024;
const MAX_REPLY: u64 = 32 * 1024 * 1024;
const ROUND_JS: &str = include_str!("../page/editor_round.js");
// The doc's CSP for the Editor window (section 6.6), set by the shell whatever the world sends.
const CSP: &str = "default-src 'self'; script-src 'self'; connect-src 'self'; img-src 'self' blob: data:; \
                   form-action 'none'; base-uri 'none'; object-src 'none'; frame-src 'none'; frame-ancestors 'none'";
// Runs before the page (a WebKit user script / WebView2 AddScriptToExecuteOnDocumentCreated, outside the CSP): it only
// collects CSP violations, so the report can say whether the studio runs under the doc's CSP.
const INIT_JS: &str = "window.__spikeViolations = []; document.addEventListener('securitypolicyviolation', (e) => \
                       window.__spikeViolations.push(e.violatedDirective + ' ' + (e.blockedURI || '') + ' ' + (e.sample || '')));";

// ---------------------------------------------------------------------------------------------------------------------
// The report.

static REPORT: OnceLock<Mutex<Option<File>>> = OnceLock::new();
static STARTED: OnceLock<Instant> = OnceLock::new();

fn report(mut line: Value) {
    if let Some(map) = line.as_object_mut() {
        map.insert("t".into(), json!(STARTED.get().map(|s| s.elapsed().as_millis() as u64).unwrap_or(0)));
        map.insert("os".into(), json!(std::env::consts::OS));
    }
    let text = line.to_string();
    println!("{text}");
    if let Some(slot) = REPORT.get() {
        if let Some(file) = slot.lock().unwrap().as_mut() {
            let _ = writeln!(file, "{text}");
            let _ = file.flush();
        }
    }
}

#[tauri::command]
fn spike_report(line: Value) {
    report(json!({"from": "main-window", "line": line}));
}

fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|b| format!("{b:02x}")).collect()
}

// ---------------------------------------------------------------------------------------------------------------------
// The pinned verifier (doc 78 section 6.4's PinnedVerifier, and its record mode).

#[derive(Debug)]
enum Pin {
    Exact(String),
    Record(Mutex<Option<String>>),
}

#[derive(Debug)]
struct PinnedVerifier {
    pin: Pin,
    algs: WebPkiSupportedAlgorithms,
}

fn spki_sha256(cert: &CertificateDer<'_>) -> Result<String, rustls::Error> {
    let ee = webpki::EndEntityCert::try_from(cert)
        .map_err(|_| rustls::Error::InvalidCertificate(rustls::CertificateError::BadEncoding))?;
    let spki = ee.subject_public_key_info();
    Ok(hex(ring::digest::digest(&ring::digest::SHA256, spki.as_ref()).as_ref()))
}

impl ServerCertVerifier for PinnedVerifier {
    fn verify_server_cert(
        &self,
        end_entity: &CertificateDer<'_>,
        _intermediates: &[CertificateDer<'_>],
        _server_name: &ServerName<'_>,
        _ocsp: &[u8],
        _now: UnixTime,
    ) -> Result<ServerCertVerified, rustls::Error> {
        let got = spki_sha256(end_entity)?;
        match &self.pin {
            Pin::Exact(want) if *want == got => Ok(ServerCertVerified::assertion()),
            Pin::Exact(_) => Err(rustls::Error::InvalidCertificate(rustls::CertificateError::ApplicationVerificationFailure)),
            Pin::Record(slot) => {
                *slot.lock().unwrap() = Some(got);
                Ok(ServerCertVerified::assertion())
            }
        }
    }

    fn verify_tls12_signature(
        &self,
        _message: &[u8],
        _cert: &CertificateDer<'_>,
        _dss: &DigitallySignedStruct,
    ) -> Result<HandshakeSignatureValid, rustls::Error> {
        Err(rustls::Error::General("TLS 1.2 is never offered to a world's Editor port".into()))
    }

    fn verify_tls13_signature(
        &self,
        message: &[u8],
        cert: &CertificateDer<'_>,
        dss: &DigitallySignedStruct,
    ) -> Result<HandshakeSignatureValid, rustls::Error> {
        rustls::crypto::verify_tls13_signature(message, cert, dss, &self.algs)
    }

    fn supported_verify_schemes(&self) -> Vec<SignatureScheme> {
        self.algs.supported_schemes()
    }
}

fn client_config(pin: Pin) -> (Arc<ClientConfig>, Arc<PinnedVerifier>) {
    let provider = Arc::new(rustls::crypto::ring::default_provider());
    let verifier = Arc::new(PinnedVerifier { pin, algs: provider.signature_verification_algorithms });
    let config = ClientConfig::builder_with_provider(provider)
        .with_protocol_versions(&[&rustls::version::TLS13])
        .expect("TLS 1.3 with ring")
        .dangerous()
        .with_custom_certificate_verifier(verifier.clone())
        .with_no_client_auth();
    (Arc::new(config), verifier)
}

// ---------------------------------------------------------------------------------------------------------------------
// A copy of ureq 3.5.1's RustlsConnector (src/tls/rustls.rs) holding our own config.

#[derive(Debug)]
struct PinnedTls {
    config: Arc<ClientConfig>,
}

struct PinnedTransport {
    buffers: LazyBuffers,
    stream: StreamOwned<ClientConnection, TransportAdapter>,
}

impl std::fmt::Debug for PinnedTransport {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("PinnedTransport").finish()
    }
}

impl<In: Transport> Connector<In> for PinnedTls {
    type Out = Either<In, PinnedTransport>;

    fn connect(&self, details: &ConnectionDetails, chained: Option<In>) -> Result<Option<Self::Out>, ureq::Error> {
        let Some(transport) = chained else { return Ok(None) };
        if !details.needs_tls() || transport.is_tls() {
            return Ok(Some(Either::A(transport)));
        }
        let host = details.uri.host().unwrap_or("").trim_start_matches('[').trim_end_matches(']').to_string();
        let name = ServerName::try_from(host).map_err(|_| ureq::Error::Tls("invalid server name"))?;
        let mut conn = ClientConnection::new(self.config.clone(), name).map_err(|e| ureq::Error::Io(std::io::Error::other(e)))?;
        let mut sock = TransportAdapter::new(transport.boxed(), details.current_time.clone());
        sock.set_timeout(details.timeout);
        conn.complete_io(&mut sock)?;
        let buffers = LazyBuffers::new(details.config.input_buffer_size(), details.config.output_buffer_size());
        Ok(Some(Either::B(PinnedTransport { buffers, stream: StreamOwned::new(conn, sock) })))
    }
}

impl Transport for PinnedTransport {
    fn buffers(&mut self) -> &mut dyn Buffers {
        &mut self.buffers
    }

    fn transmit_output(&mut self, amount: usize, timeout: NextTimeout) -> Result<(), ureq::Error> {
        self.stream.get_mut().set_timeout(timeout);
        let output = &self.buffers.output()[..amount];
        self.stream.write_all(output)?;
        Ok(())
    }

    fn await_input(&mut self, timeout: NextTimeout) -> Result<bool, ureq::Error> {
        self.stream.get_mut().set_timeout(timeout);
        let input = self.buffers.input_append_buf();
        let amount = self.stream.read(input)?;
        self.buffers.input_appended(amount);
        Ok(amount > 0)
    }

    fn is_open(&mut self) -> bool {
        self.stream.get_mut().get_mut().is_open()
    }

    fn is_tls(&self) -> bool {
        true
    }
}

fn pinned_agent(config: Arc<ClientConfig>) -> ureq::Agent {
    let agent_config = ureq::Agent::config_builder()
        .http_status_as_error(false)
        .max_redirects(0)
        .proxy(None)
        .timeout_global(Some(Duration::from_secs(60)))
        .timeout_connect(Some(Duration::from_secs(15)))
        .accept_encoding("")
        .user_agent("ratw-editor-spike")
        .build();
    let connector = ().chain(TcpConnector::default()).chain(PinnedTls { config });
    ureq::Agent::with_parts(agent_config, connector, DefaultResolver::default())
}

fn error_text(e: &ureq::Error) -> String {
    let mut text = e.to_string();
    let mut source = std::error::Error::source(e);
    while let Some(s) = source {
        text.push_str(" <- ");
        text.push_str(&s.to_string());
        source = s.source();
    }
    text
}

// The Rust-side checks of the pinned client, before any window.
fn tls_checks(url: &str, url12: &str, pin: &str) {
    let hello = format!("{url}/spike/hello");
    let (good, _) = client_config(Pin::Exact(pin.to_string()));
    let agent = pinned_agent(good);
    for n in 0..3 {
        let t = Instant::now();
        let r = agent.get(&hello).call();
        report(match r {
            Ok(mut resp) => json!({"test": "pinned-right-pin", "n": n, "ok": true, "status": resp.status().as_u16(),
                                   "us": t.elapsed().as_micros() as u64,
                                   "body": resp.body_mut().read_to_string().unwrap_or_default()}),
            Err(e) => json!({"test": "pinned-right-pin", "n": n, "ok": false, "error": error_text(&e)}),
        });
    }
    let (bad, _) = client_config(Pin::Exact("00".repeat(32)));
    report(match pinned_agent(bad).get(&hello).call() {
        Ok(resp) => json!({"test": "pinned-wrong-pin", "refused": false, "status": resp.status().as_u16()}),
        Err(e) => json!({"test": "pinned-wrong-pin", "refused": true, "error": error_text(&e)}),
    });
    let (good12, _) = client_config(Pin::Exact(pin.to_string()));
    report(match pinned_agent(good12).get(&format!("{url12}/spike/hello")).call() {
        Ok(resp) => json!({"test": "pinned-tls12-port", "refused": false, "status": resp.status().as_u16()}),
        Err(e) => json!({"test": "pinned-tls12-port", "refused": true, "error": error_text(&e)}),
    });
    let (rec, verifier) = client_config(Pin::Record(Mutex::new(None)));
    let result = pinned_agent(rec).get(&hello).call();
    let recorded = match &verifier.pin {
        Pin::Record(slot) => slot.lock().unwrap().clone(),
        _ => None,
    };
    report(json!({"test": "record-mode", "ok": result.is_ok(), "recorded": recorded,
                  "matches_openssl_pin": recorded.as_deref() == Some(pin)}));
    // A 32 MB gzipped reply straight through the pinned client (no webview): the time it takes.
    let t = Instant::now();
    let (cfg, _) = client_config(Pin::Exact(pin.to_string()));
    match fetch(&pinned_agent(cfg), "GET", &format!("{url}/spike/big?mb=32&gz=1"), None, None) {
        Ok(f) => report(json!({"test": "pinned-32mb-gzip-direct", "status": f.status, "wire": f.wire, "bytes": f.body.len(),
                               "ms": t.elapsed().as_millis() as u64})),
        Err(e) => report(json!({"test": "pinned-32mb-gzip-direct", "error": e})),
    }
}

struct Fetched {
    status: u16,
    content_type: Option<String>,
    spike_headers: Vec<(String, String)>,
    wire: u64,
    body: Vec<u8>,
}

// The proxy's one request: GET or POST, Accept-Encoding gzip, both the wire and the decompressed size capped.
fn fetch(agent: &ureq::Agent, method: &str, url: &str, content_type: Option<&str>, body: Option<&[u8]>) -> Result<Fetched, String> {
    let result = match method {
        "GET" => agent.get(url).header("Accept-Encoding", "gzip").header("Authorization", "Bearer spike-session").call(),
        "POST" => {
            let mut req = agent.post(url).header("Accept-Encoding", "gzip").header("Authorization", "Bearer spike-session");
            if let Some(ct) = content_type {
                req = req.header("Content-Type", ct);
            }
            req.send(body.unwrap_or(&[]))
        }
        _ => return Err("method".into()),
    };
    let resp = result.map_err(|e| error_text(&e))?;
    let status = resp.status().as_u16();
    let header = |name: &str| resp.headers().get(name).and_then(|v| v.to_str().ok()).map(str::to_string);
    let content_type = header("content-type");
    let gzipped = header("content-encoding").is_some_and(|v| v.eq_ignore_ascii_case("gzip"));
    let spike_headers = resp
        .headers()
        .iter()
        .filter(|(k, _)| k.as_str().starts_with("x-spike-"))
        .map(|(k, v)| (k.to_string(), v.to_str().unwrap_or("").to_string()))
        .collect();
    let mut wire_bytes = Vec::new();
    resp.into_body().into_reader().take(MAX_REPLY + 1).read_to_end(&mut wire_bytes).map_err(|e| e.to_string())?;
    if wire_bytes.len() as u64 > MAX_REPLY {
        return Err("reply over 32 MB on the wire".into());
    }
    let wire = wire_bytes.len() as u64;
    let body = if gzipped {
        let mut out = Vec::new();
        flate2::read::GzDecoder::new(&wire_bytes[..]).take(MAX_REPLY + 1).read_to_end(&mut out).map_err(|e| e.to_string())?;
        if out.len() as u64 > MAX_REPLY {
            return Err("reply over 32 MB after decompression".into());
        }
        out
    } else {
        wire_bytes
    };
    Ok(Fetched { status, content_type, spike_headers, wire, body })
}

// ---------------------------------------------------------------------------------------------------------------------
// The `editor` scheme.

struct Spike {
    url: String,
    agent: ureq::Agent,
    round: Mutex<usize>,
}

fn respond(status: u16, ctype: &str, body: Vec<u8>) -> tauri::http::Response<Vec<u8>> {
    tauri::http::Response::builder()
        .status(status)
        .header("Content-Type", ctype)
        .header("X-Content-Type-Options", "nosniff")
        .header("Content-Security-Policy", CSP)
        .body(body)
        .unwrap()
}

fn handle(app: &AppHandle, spike: &Spike, label: &str, request: tauri::http::Request<Vec<u8>>) -> tauri::http::Response<Vec<u8>> {
    let method = request.method().as_str().to_string();
    let path = request.uri().path_and_query().map(|p| p.as_str().to_string()).unwrap_or_else(|| "/".into());
    let header = |name: &str| request.headers().get(name).and_then(|v| v.to_str().ok()).map(str::to_string);
    let seen = json!({"test": "scheme-request", "label": label, "method": method, "path": path,
                      "uri": request.uri().to_string(), "body": request.body().len(),
                      "content_length": header("content-length"), "content_type": header("content-type"),
                      "origin": header("origin"), "cookie": header("cookie").is_some(),
                      "headers": request.headers().keys().map(|k| k.as_str()).collect::<Vec<_>>()});
    if label != "editor" {
        report(json!({"test": "scheme-refused-label", "label": label, "path": path}));
        return respond(403, "application/json", br#"{"error":"not the Editor window"}"#.to_vec());
    }
    if path == "/spike/report" {
        let line: Value = serde_json::from_slice(request.body()).unwrap_or(json!({"bad": true}));
        let done = line.get("test").and_then(Value::as_str) == Some("round-done");
        report(json!({"from": "editor-window", "line": line}));
        if done {
            next_round(app, spike);
        }
        return respond(204, "application/json", Vec::new());
    }
    if !(path.starts_with("/spike/big") || path.starts_with("/spike/echo")) {
        report(seen.clone());
    } else {
        report(json!({"test": "scheme-request-big", "label": label, "method": method, "path": path, "body": request.body().len(),
                      "content_length": header("content-length")}));
    }
    if method != "GET" && method != "POST" {
        return respond(405, "application/json", br#"{"error":"GET and POST only"}"#.to_vec());
    }
    if path.starts_with("/editor/") {
        return respond(403, "application/json", br#"{"error":"refused"}"#.to_vec());
    }
    if request.body().len() > MAX_REQUEST {
        return respond(413, "application/json", br#"{"error":"request over 16 MB"}"#.to_vec());
    }
    let url = format!("{}{}", spike.url, path);
    let body = (method == "POST").then(|| request.body().as_slice());
    let t = Instant::now();
    match fetch(&spike.agent, &method, &url, header("content-type").as_deref(), body) {
        Ok(f) => {
            if path.starts_with("/spike/big") {
                report(json!({"test": "proxy-big", "path": path, "wire": f.wire, "bytes": f.body.len(),
                              "ms": t.elapsed().as_millis() as u64}));
            }
            let ctype = if path.starts_with("/api/") || path.starts_with("/spike/") {
                "application/json".to_string()
            } else {
                f.content_type.unwrap_or_else(|| "application/octet-stream".into())
            };
            let mut builder = tauri::http::Response::builder()
                .status(f.status)
                .header("Content-Type", ctype)
                .header("X-Content-Type-Options", "nosniff")
                .header("Content-Security-Policy", CSP);
            for (k, v) in f.spike_headers {
                builder = builder.header(k, v);
            }
            if path.starts_with("/spike/download") {
                builder = builder.header("Content-Disposition", "attachment");
            }
            builder.body(f.body).unwrap()
        }
        Err(e) => {
            report(json!({"test": "proxy-error", "path": path, "error": e}));
            respond(502, "application/json", json!({"error": e}).to_string().into_bytes())
        }
    }
}

fn editor_url(path: &str) -> tauri::Url {
    let origin = if cfg!(windows) { "http://editor.localhost" } else { "editor://localhost" };
    format!("{origin}{path}").parse().unwrap()
}

// Rounds: the first does the full checks; each later one opens a new window and reads what the one before stored.
// Default: incognito, incognito (storage must be gone), persistent, persistent (the control: storage kept).
// SPIKE_ROUNDS=ppii runs the persistent pair first (p = persistent, i = incognito, d = a persistent window with a data
// directory of its own, new each round: the fallback if incognito fails). Each window is closed before the next
// opens; a round that hasn't said it's done in 100 s is reported and skipped.
fn rounds() -> Vec<(char, bool, &'static str)> {
    let order = std::env::var("SPIKE_ROUNDS").unwrap_or_else(|_| "iipp".into());
    order
        .chars()
        .enumerate()
        .map(|(n, c)| (c, n == 0, if n % 2 == 0 { "/?suite=dm" } else { "/?suite=atlas" }))
        .collect()
}

fn open_round(app: &AppHandle, round: usize) {
    let (kind, full, path) = rounds()[round];
    let incognito = kind == 'i';
    let downloads = app.clone();
    let mut builder = WebviewWindowBuilder::new(app, "editor", WebviewUrl::CustomProtocol(editor_url(path)));
    if kind == 'd' {
        let dir = std::env::temp_dir().join(format!("ratw-editor-spike-{}-{round}", std::process::id()));
        report(json!({"test": "data-directory", "round": round, "dir": dir.display().to_string()}));
        builder = builder.data_directory(dir);
    }
    let made = builder
        .title(format!("RATW Editor spike, round {round}"))
        .inner_size(1100.0, 760.0)
        .incognito(incognito)
        .initialization_script(INIT_JS)
        .on_download(move |_webview, event| {
            let _ = &downloads;
            match event {
                DownloadEvent::Requested { url, destination } => {
                    report(json!({"test": "on-download-requested", "url": url.to_string(),
                                  "destination": destination.display().to_string()}));
                    false
                }
                DownloadEvent::Finished { url, path, success } => {
                    report(json!({"test": "on-download-finished", "url": url.to_string(),
                                  "path": path.map(|p| p.display().to_string()), "success": success}));
                    true
                }
                _ => true,
            }
        })
        .on_navigation(|url| {
            report(json!({"test": "editor-navigation", "url": url.to_string()}));
            true
        })
        .build();
    match made {
        Ok(window) => {
            report(json!({"test": "editor-window-open", "round": round, "kind": kind.to_string(), "incognito": incognito, "path": path}));
            let watch = app.clone();
            std::thread::spawn(move || {
                std::thread::sleep(Duration::from_secs(100));
                let spike = watch.state::<Arc<Spike>>();
                if *spike.round.lock().unwrap() == round {
                    report(json!({"test": "round-timeout", "round": round}));
                    next_round(&watch, &spike);
                }
            });
            std::thread::spawn(move || {
                std::thread::sleep(Duration::from_secs(8));
                let script = format!("const ROUND = {round}; const FULL = {full};\n{ROUND_JS}");
                if let Err(e) = window.eval(&script) {
                    report(json!({"test": "eval-error", "round": round, "error": e.to_string()}));
                }
            });
        }
        Err(e) => report(json!({"test": "editor-window-open", "round": round, "error": e.to_string()})),
    }
}

fn next_round(app: &AppHandle, spike: &Spike) {
    let mut round = spike.round.lock().unwrap();
    *round += 1;
    let next = *round;
    let app = app.clone();
    std::thread::spawn(move || {
        if let Some(w) = app.get_webview_window("editor") {
            let _ = w.destroy();
        }
        std::thread::sleep(Duration::from_millis(1500));
        if next < rounds().len() {
            let handle = app.clone();
            let _ = app.run_on_main_thread(move || open_round(&handle, next));
        } else {
            finish(&app, "all rounds done");
        }
    });
}

fn finish(app: &AppHandle, why: &str) {
    report(json!({"test": "finished", "why": why}));
    if std::env::var("SPIKE_QUIT").as_deref() == Ok("1") {
        app.exit(0);
        std::thread::sleep(Duration::from_secs(3));
        std::process::exit(0);
    }
}

fn main() {
    STARTED.set(Instant::now()).ok();
    let path = std::env::var("SPIKE_REPORT").unwrap_or_else(|_| "spike-editor.jsonl".into());
    let file = OpenOptions::new().create(true).append(true).open(&path).ok();
    REPORT.set(Mutex::new(file)).ok();
    let url = std::env::var("SPIKE_URL").unwrap_or_else(|_| "https://127.0.0.1:47891".into());
    let url12 = std::env::var("SPIKE_URL12").unwrap_or_else(|_| "https://127.0.0.1:47892".into());
    let pin = std::env::var("SPIKE_PIN").expect("SPIKE_PIN").trim().to_lowercase();
    report(json!({"test": "start", "url": url, "pin": pin, "tauri": tauri::VERSION}));
    tls_checks(&url, &url12, &pin);

    let (config, _) = client_config(Pin::Exact(pin.clone()));
    let spike = Arc::new(Spike { url, agent: pinned_agent(config), round: Mutex::new(0) });
    let for_scheme = spike.clone();
    let managed = spike.clone();
    let deadline: u64 = std::env::var("SPIKE_DEADLINE").ok().and_then(|s| s.parse().ok()).unwrap_or(300);

    tauri::Builder::default()
        .register_asynchronous_uri_scheme_protocol("editor", move |ctx, request, responder| {
            let label = ctx.webview_label().to_string();
            let app = ctx.app_handle().clone();
            let spike = for_scheme.clone();
            std::thread::spawn(move || responder.respond(handle(&app, &spike, &label, request)));
        })
        .invoke_handler(tauri::generate_handler![spike_report])
        .setup(move |app| {
            app.manage(managed);
            let handle = app.handle().clone();
            WebviewWindowBuilder::new(app, "main", WebviewUrl::App("index.html".into()))
                .title("RATW Editor spike")
                .inner_size(600.0, 300.0)
                .build()?;
            open_round(&handle, 0);
            let late = handle.clone();
            std::thread::spawn(move || {
                std::thread::sleep(Duration::from_secs(deadline));
                finish(&late, "deadline");
            });
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("the spike app");
}
