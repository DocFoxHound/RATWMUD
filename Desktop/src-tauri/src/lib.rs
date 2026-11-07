// Phase 0 spike (doc 77, section 9 and Phase 0): a Tauri window with a bundled page, and three ways for that page to
// reach a plain ws:// world it can't open itself:
//   - the websocket plugin (tauri-plugin-websocket: frames cross as JSON arrays of numbers);
//   - "ipc relay": our own socket in Rust, binary frames handed to the page as raw bytes over a Tauri channel, and
//     sent from the page as a raw invoke body;
//   - "loopback relay": our own ws:// listener on 127.0.0.1 with a random token; the page opens it with its own
//     WebSocket and Rust pipes the frames to the world.
// Spike code only: not the shape of the real client.
use std::collections::HashMap;
use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::{Arc, Mutex};

use futures_util::{SinkExt, StreamExt};
use tauri::ipc::{Channel, InvokeBody, InvokeResponseBody, Request};
use tauri::{Manager, State};
use tokio::sync::mpsc;
use tokio_tungstenite::tungstenite::Message;
use tokio_tungstenite::Connector;

#[derive(Default)]
struct Relays {
    next: AtomicU32,
    open: Mutex<HashMap<u32, mpsc::UnboundedSender<Message>>>,
}

struct Loopback {
    port: u16,
    token: String,
}

// A TLS setup that also trusts the spike's own CA (SPIKE_CA=path to a PEM), for wss:// to the local test server.
fn connector() -> Option<Connector> {
    let path = std::env::var("SPIKE_CA").ok()?;
    let pem = std::fs::read(&path).ok()?;
    let mut roots = rustls::RootCertStore::empty();
    for cert in rustls::pki_types::pem::PemObject::pem_slice_iter(&pem) {
        let cert: rustls::pki_types::CertificateDer = cert.ok()?;
        roots.add(cert).ok()?;
    }
    let config = rustls::ClientConfig::builder_with_provider(Arc::new(rustls::crypto::ring::default_provider()))
        .with_safe_default_protocol_versions()
        .ok()?
        .with_root_certificates(roots)
        .with_no_client_auth();
    Some(Connector::Rustls(Arc::new(config)))
}

type Ws = tokio_tungstenite::WebSocketStream<tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>>;

async fn dial(url: &str) -> Result<Ws, String> {
    let (ws, _) = tokio_tungstenite::connect_async_tls_with_config(url, None, true, connector())
        .await
        .map_err(|e| e.to_string())?;
    Ok(ws)
}

#[tauri::command]
fn spike_report(line: String) {
    println!("SPIKE {line}");
    if let Ok(path) = std::env::var("SPIKE_REPORT") {
        use std::io::Write;
        if let Ok(mut f) = std::fs::OpenOptions::new().create(true).append(true).open(path) {
            let _ = writeln!(f, "{line}");
        }
    }
    if line.contains("\"done\":true") && std::env::var("SPIKE_QUIT").is_ok() {
        std::process::exit(0);
    }
}

// Opens a socket to `url` in Rust. Frames from the world reach the page on `on_frame`: binary as raw bytes
// (an ArrayBuffer in the page), text as a JSON string. Returns the relay's id.
#[tauri::command]
async fn relay_open(
    url: String,
    on_frame: Channel<InvokeResponseBody>,
    relays: State<'_, Relays>,
) -> Result<u32, String> {
    let ws = dial(&url).await?;
    let (mut write, mut read) = ws.split();
    let (tx, mut rx) = mpsc::unbounded_channel::<Message>();
    let id = relays.next.fetch_add(1, Ordering::Relaxed) + 1;
    relays.open.lock().unwrap().insert(id, tx);
    tauri::async_runtime::spawn(async move {
        while let Some(m) = rx.recv().await {
            let close = matches!(m, Message::Close(_));
            if write.send(m).await.is_err() || close {
                break;
            }
        }
    });
    tauri::async_runtime::spawn(async move {
        while let Some(Ok(m)) = read.next().await {
            let sent = match m {
                Message::Binary(b) => on_frame.send(InvokeResponseBody::Raw(b.to_vec())),
                Message::Text(t) => on_frame.send(InvokeResponseBody::Json(
                    serde_json::to_string(t.as_str()).unwrap_or_default(),
                )),
                Message::Close(_) => break,
                _ => Ok(()),
            };
            if sent.is_err() {
                break;
            }
        }
        let _ = on_frame.send(InvokeResponseBody::Json("null".into()));
    });
    Ok(id)
}

// A frame for relay `relay-id` (a header): a raw body is sent as binary, a JSON string as text.
#[tauri::command]
fn relay_send(request: Request<'_>, relays: State<'_, Relays>) -> Result<(), String> {
    let id: u32 = request
        .headers()
        .get("relay-id")
        .and_then(|v| v.to_str().ok())
        .and_then(|v| v.parse().ok())
        .ok_or("no relay-id")?;
    let message = match request.body() {
        InvokeBody::Raw(bytes) => Message::Binary(bytes.clone().into()),
        InvokeBody::Json(serde_json::Value::String(s)) => Message::Text(s.clone().into()),
        InvokeBody::Json(v) => Message::Text(v.to_string().into()),
    };
    let open = relays.open.lock().unwrap();
    open.get(&id).ok_or("no such relay")?.send(message).map_err(|e| e.to_string())
}

#[tauri::command]
fn relay_close(id: u32, relays: State<'_, Relays>) {
    if let Some(tx) = relays.open.lock().unwrap().remove(&id) {
        let _ = tx.send(Message::Close(None));
    }
}

// The loopback relay's address: ws://127.0.0.1:PORT/TOKEN?to=<world url>.
#[tauri::command]
fn loopback_relay(loopback: State<'_, Loopback>) -> String {
    format!("ws://127.0.0.1:{}/{}", loopback.port, loopback.token)
}

async fn loopback_serve(listener: tokio::net::TcpListener, token: String) {
    loop {
        let Ok((stream, _)) = listener.accept().await else { continue };
        let _ = stream.set_nodelay(true);
        let token = token.clone();
        tauri::async_runtime::spawn(async move {
            let mut target = None;
            let callback = |req: &tokio_tungstenite::tungstenite::handshake::server::Request,
                            resp: tokio_tungstenite::tungstenite::handshake::server::Response| {
                let uri = req.uri();
                let ok = uri.path() == format!("/{token}");
                target = uri.query().and_then(|q| q.strip_prefix("to=")).map(percent_decode);
                if ok && target.is_some() {
                    Ok(resp)
                } else {
                    let mut refuse = tokio_tungstenite::tungstenite::handshake::server::ErrorResponse::new(None);
                    *refuse.status_mut() = tokio_tungstenite::tungstenite::http::StatusCode::FORBIDDEN;
                    Err(refuse)
                }
            };
            let Ok(page) = tokio_tungstenite::accept_hdr_async(stream, callback).await else { return };
            let Some(url) = target else { return };
            let Ok(world) = dial(&url).await else { return };
            let (mut page_tx, mut page_rx) = page.split();
            let (mut world_tx, mut world_rx) = world.split();
            let up = async {
                while let Some(Ok(m)) = page_rx.next().await {
                    if world_tx.send(m).await.is_err() {
                        break;
                    }
                }
            };
            let down = async {
                while let Some(Ok(m)) = world_rx.next().await {
                    if page_tx.send(m).await.is_err() {
                        break;
                    }
                }
            };
            tokio::select! { _ = up => {}, _ = down => {} }
        });
    }
}

fn percent_decode(s: &str) -> String {
    let bytes = s.as_bytes();
    let mut out = Vec::with_capacity(bytes.len());
    let mut i = 0;
    while i < bytes.len() {
        if bytes[i] == b'%' && i + 2 < bytes.len() {
            if let Ok(v) = u8::from_str_radix(&s[i + 1..i + 3], 16) {
                out.push(v);
                i += 3;
                continue;
            }
        }
        out.push(bytes[i]);
        i += 1;
    }
    String::from_utf8_lossy(&out).into_owned()
}

fn random_token() -> String {
    // Spike only: good enough to keep other local programs off the relay by accident.
    use std::hash::{BuildHasher, Hasher};
    let mut out = String::new();
    for _ in 0..2 {
        let mut h = std::collections::hash_map::RandomState::new().build_hasher();
        h.write_u128(std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_nanos());
        out.push_str(&format!("{:016x}", h.finish()));
    }
    out
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let _ = rustls::crypto::ring::default_provider().install_default();
    tauri::Builder::default()
        .plugin(tauri_plugin_websocket::init())
        .manage(Relays::default())
        .setup(|app| {
            let listener = tauri::async_runtime::block_on(tokio::net::TcpListener::bind("127.0.0.1:0"))?;
            let port = listener.local_addr()?.port();
            let token = random_token();
            app.manage(Loopback { port, token: token.clone() });
            tauri::async_runtime::spawn(loopback_serve(listener, token));

            // Let the page's own WebSocket trust the spike's self-signed certificate (SPIKE_CA) on 127.0.0.1, so the
            // direct wss:// test means something. Linux only (WebKitGTK).
            #[cfg(target_os = "linux")]
            if let Ok(path) = std::env::var("SPIKE_CA") {
                if let Some(window) = app.get_webview_window("main") {
                    let _ = window.with_webview(move |webview| {
                        use webkit2gtk::{WebContextExt, WebViewExt};
                        if let Some(context) = webview.inner().context() {
                            match webkit2gtk::gio::TlsCertificate::from_file(&path) {
                                Ok(cert) => {
                                    context.allow_tls_certificate_for_host(&cert, "127.0.0.1");
                                    println!("SPIKE-NOTE allowed the spike certificate for 127.0.0.1");
                                }
                                Err(e) => println!("SPIKE-NOTE certificate not loaded: {e}"),
                            }
                        }
                    });
                }
            }
            if let Some(window) = app.get_webview_window("main") {
                if let Ok(url) = window.url() {
                    println!("SPIKE-NOTE page origin: {}", url.origin().ascii_serialization());
                }
            }
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![spike_report, relay_open, relay_send, relay_close, loopback_relay])
        .run(tauri::generate_context!())
        .expect("error while running the spike");
}
