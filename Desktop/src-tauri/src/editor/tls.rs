// The pinned TLS client for a world's Editor port (Docs/Design/78, 6.4; M0a measured it on both webviews' platforms).
//
// - `PinnedVerifier`: the SHA-256 of the end entity's SPKI against the pin, ignoring the name and the dates (a world's
//   certificate is self-signed and its address is whatever the host typed). The handshake's signature is still checked
//   (rustls::crypto::verify_tls13_signature), so the server must hold the key: without that a pin would mean nothing.
//   In "record" mode it accepts any key and records it: used only for a first connection's /editor/hello, on which
//   nothing secret is ever sent.
// - TLS 1.3 only (TLS 1.2, compiled in for central, is never offered here), no session resumption.
// - An HTTP client: ureq 3.5.1's `Agent::with_parts` with a copy of its RustlsConnector (src/tls/rustls.rs) holding this
//   config. ureq is pinned exactly (=3.5.1) because `Connector` sits in `ureq::unversioned`.
use std::io::{Read, Write};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use base64::Engine;
use rustls::client::danger::{HandshakeSignatureValid, ServerCertVerified, ServerCertVerifier};
use rustls::client::Resumption;
use rustls::crypto::WebPkiSupportedAlgorithms;
use rustls::pki_types::{CertificateDer, ServerName, UnixTime};
use rustls::{ClientConfig, ClientConnection, DigitallySignedStruct, SignatureScheme, StreamOwned};
use ureq::unversioned::resolver::DefaultResolver;
use ureq::unversioned::transport::{Buffers, ConnectionDetails, Connector, Either, LazyBuffers, NextTimeout, TcpConnector, Transport, TransportAdapter};

/// The full SHA-256 of a key's SPKI: what the client pins (and an Editor code carries, as base64url).
pub type Pin = [u8; 32];

pub fn sha256(data: &[u8]) -> Pin {
    let digest = ring::digest::digest(&ring::digest::SHA256, data);
    let mut out = [0u8; 32];
    out.copy_from_slice(digest.as_ref());
    out
}

pub fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|b| format!("{b:02x}")).collect()
}

pub fn b64url(bytes: &[u8]) -> String {
    base64::engine::general_purpose::URL_SAFE_NO_PAD.encode(bytes)
}

/// base64url, with or without its padding.
pub fn unb64url(text: &str) -> Option<Vec<u8>> {
    base64::engine::general_purpose::URL_SAFE_NO_PAD.decode(text.trim_end_matches('=')).ok()
}

pub fn b64(bytes: &[u8]) -> String {
    base64::engine::general_purpose::STANDARD.encode(bytes)
}

pub fn unb64(text: &str) -> Option<Vec<u8>> {
    base64::engine::general_purpose::STANDARD.decode(text).ok()
}

/// The pin as text (base64url, as the world's files and codes carry it).
pub fn pin_text(pin: &Pin) -> String {
    b64url(pin)
}

pub fn pin_from_text(text: &str) -> Option<Pin> {
    unb64url(text).and_then(|b| <[u8; 32]>::try_from(b.as_slice()).ok())
}

/// The fingerprint people compare: the pin's first 128 bits as 8 groups of 4 hex characters, always all eight, in the
/// same layout as `manage/editor-certificate.txt` and the world's log (doc 78, 3.4).
pub fn fingerprint(pin: &Pin) -> String {
    let h = hex(&pin[..16]).to_ascii_uppercase();
    (0..8).map(|i| &h[i * 4..i * 4 + 4]).collect::<Vec<_>>().join(" ")
}

/// The end entity's SubjectPublicKeyInfo (the whole SEQUENCE, as `openssl` and tools/world_admin.py hash it).
pub fn spki_of(cert: &CertificateDer<'_>) -> Result<Vec<u8>, rustls::Error> {
    let ee = webpki::EndEntityCert::try_from(cert).map_err(|_| rustls::Error::InvalidCertificate(rustls::CertificateError::BadEncoding))?;
    Ok(ee.subject_public_key_info().as_ref().to_vec())
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Mode {
    /// Only the key with this pin.
    Exact(Pin),
    /// Any key, recorded: a first connection's hello only.
    Record,
}

#[derive(Debug)]
pub struct PinnedVerifier {
    mode: Mode,
    /// The SPKI the server presented (in either mode): a rotation is checked against it, and a code's pin is completed
    /// with it.
    seen: Mutex<Option<Vec<u8>>>,
    algs: WebPkiSupportedAlgorithms,
}

impl PinnedVerifier {
    pub fn seen(&self) -> Option<Vec<u8>> {
        self.seen.lock().unwrap().clone()
    }
}

impl ServerCertVerifier for PinnedVerifier {
    fn verify_server_cert(&self, end_entity: &CertificateDer<'_>, _intermediates: &[CertificateDer<'_>], _server_name: &ServerName<'_>,
        _ocsp: &[u8], _now: UnixTime) -> Result<ServerCertVerified, rustls::Error> {
        let spki = spki_of(end_entity)?;
        let got = sha256(&spki);
        *self.seen.lock().unwrap() = Some(spki);
        match self.mode {
            Mode::Exact(want) if want == got => Ok(ServerCertVerified::assertion()),
            Mode::Exact(_) => Err(rustls::Error::InvalidCertificate(rustls::CertificateError::ApplicationVerificationFailure)),
            Mode::Record => Ok(ServerCertVerified::assertion()),
        }
    }

    fn verify_tls12_signature(&self, _message: &[u8], _cert: &CertificateDer<'_>, _dss: &DigitallySignedStruct) -> Result<HandshakeSignatureValid, rustls::Error> {
        Err(rustls::Error::General("TLS 1.2 is never offered to a world's Editor port".into()))
    }

    fn verify_tls13_signature(&self, message: &[u8], cert: &CertificateDer<'_>, dss: &DigitallySignedStruct) -> Result<HandshakeSignatureValid, rustls::Error> {
        rustls::crypto::verify_tls13_signature(message, cert, dss, &self.algs)
    }

    fn supported_verify_schemes(&self) -> Vec<SignatureScheme> {
        self.algs.supported_schemes()
    }
}

/// A TLS 1.3-only rustls config with the pinned verifier, and the verifier (to read what it saw).
pub fn client_config(mode: Mode) -> (Arc<ClientConfig>, Arc<PinnedVerifier>) {
    let provider = Arc::new(rustls::crypto::ring::default_provider());
    let verifier = Arc::new(PinnedVerifier { mode, seen: Mutex::new(None), algs: provider.signature_verification_algorithms });
    let mut config = ClientConfig::builder_with_provider(provider)
        .with_protocol_versions(&[&rustls::version::TLS13])
        .expect("TLS 1.3 with ring")
        .dangerous()
        .with_custom_certificate_verifier(verifier.clone())
        .with_no_client_auth();
    // Every connection is a full handshake with the key checked (never a ticket from an earlier one).
    config.resumption = Resumption::disabled();
    (Arc::new(config), verifier)
}

// ---------------------------------------------------------------------------------------------------------------------
// A copy of ureq 3.5.1's RustlsConnector (src/tls/rustls.rs) holding our own config. Only `set_timeout_at` is private
// to ureq; `set_timeout` does the same job here (M0a).

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

/// The pinned client: no redirects, no proxy (never HTTP_PROXY from the environment), 15 s to connect, `whole` for the
/// whole exchange, and no Accept-Encoding of its own (the proxy asks for gzip and counts both sizes itself).
pub fn agent(mode: Mode, whole: Duration) -> (ureq::Agent, Arc<PinnedVerifier>) {
    let (config, verifier) = client_config(mode);
    let agent_config = ureq::Agent::config_builder()
        .http_status_as_error(false)
        .max_redirects(0)
        .proxy(None)
        .timeout_global(Some(whole))
        .timeout_connect(Some(Duration::from_secs(15)))
        .accept_encoding("")
        .user_agent(concat!("ratw-desktop/", env!("CARGO_PKG_VERSION")))
        .build();
    let connector = ().chain(TcpConnector::default()).chain(PinnedTls { config });
    (ureq::Agent::with_parts(agent_config, connector, DefaultResolver::default()), verifier)
}

/// Whether an error is "nothing answered" (refused, timed out, no route) rather than "something answered, not as an
/// Editor does" (a TLS alert, a plain HTTP reply, a wrong key).
pub fn unreachable(error: &ureq::Error) -> bool {
    use std::io::ErrorKind::*;
    match error {
        ureq::Error::Io(e) => matches!(e.kind(), ConnectionRefused | TimedOut | HostUnreachable | NetworkUnreachable | AddrNotAvailable | NotFound | WouldBlock),
        ureq::Error::Timeout(_) | ureq::Error::HostNotFound | ureq::Error::ConnectionFailed => true,
        _ => false,
    }
}

/// The error's text with its causes (for the log; never shown to the page as is).
pub fn error_text(e: &ureq::Error) -> String {
    let mut text = e.to_string();
    let mut source = std::error::Error::source(e);
    while let Some(s) = source {
        text.push_str(" <- ");
        text.push_str(&s.to_string());
        source = s.source();
    }
    text
}

#[cfg(test)]
pub mod tests {
    use super::*;
    use rustls::pki_types::PrivateKeyDer;
    use rustls::server::{ClientHello, ResolvesServerCert};
    use rustls::sign::CertifiedKey;
    use std::io::{BufRead, BufReader};
    use std::net::TcpListener;
    use std::sync::atomic::{AtomicUsize, Ordering};

    // Fixtures (src/editor/testdata, made once with openssl; test keys only, trusted by nothing): key A with a certificate valid for a
    // century naming "RATW Editor world-a", key A again in a certificate long expired naming someone else, and key B.
    pub const CERT_A: &[u8] = include_bytes!("testdata/a.cert.der");
    pub const KEY_A: &[u8] = include_bytes!("testdata/a.key.der");
    pub const CERT_A_OLD: &[u8] = include_bytes!("testdata/a-expired.cert.der");
    pub const CERT_B: &[u8] = include_bytes!("testdata/b.cert.der");
    pub const KEY_B: &[u8] = include_bytes!("testdata/b.key.der");

    pub fn pin_a() -> Pin {
        sha256(&spki_of(&CertificateDer::from(CERT_A)).unwrap())
    }

    pub fn pin_b() -> Pin {
        sha256(&spki_of(&CertificateDer::from(CERT_B)).unwrap())
    }

    #[derive(Debug)]
    struct Fixed(Arc<CertifiedKey>);

    impl ResolvesServerCert for Fixed {
        fn resolve(&self, _hello: ClientHello<'_>) -> Option<Arc<CertifiedKey>> {
            Some(self.0.clone())
        }
    }

    /// A server config presenting `cert` and signing with `key` (they needn't match: the "certificate without its key"
    /// case), at TLS 1.3, or 1.2 only.
    pub fn server_config(cert: &[u8], key: &[u8], tls12_only: bool) -> Arc<rustls::ServerConfig> {
        let provider = Arc::new(rustls::crypto::ring::default_provider());
        let signer = provider.key_provider.load_private_key(PrivateKeyDer::Pkcs8(key.to_vec().into())).unwrap();
        let certified = Arc::new(CertifiedKey::new(vec![CertificateDer::from(cert.to_vec())], signer));
        let versions: &[&rustls::SupportedProtocolVersion] = if tls12_only { &[&rustls::version::TLS12] } else { &[&rustls::version::TLS13] };
        let config = rustls::ServerConfig::builder_with_provider(provider)
            .with_protocol_versions(versions)
            .unwrap()
            .with_no_client_auth()
            .with_cert_resolver(Arc::new(Fixed(certified)));
        Arc::new(config)
    }

    /// One request as the test world saw it.
    #[derive(Debug, Clone)]
    pub struct Seen {
        pub method: String,
        pub path: String,
        pub headers: Vec<(String, String)>,
        pub body: Vec<u8>,
    }

    /// A stand-in world: TLS (or plain HTTP with `config` None) on 127.0.0.1, HTTP/1.1, one request a connection, each
    /// answered by `reply`. Records what it was sent. Stops when dropped (its thread ends with the process otherwise).
    pub struct TestWorld {
        pub port: u16,
        pub seen: Arc<Mutex<Vec<Seen>>>,
        pub handshakes: Arc<AtomicUsize>,
    }

    pub type Reply = Box<dyn Fn(&Seen) -> (u16, Vec<(String, String)>, Vec<u8>) + Send + Sync>;

    impl TestWorld {
        pub fn start(config: Option<Arc<rustls::ServerConfig>>, reply: Reply) -> TestWorld {
            let listener = TcpListener::bind("127.0.0.1:0").unwrap();
            let port = listener.local_addr().unwrap().port();
            let seen = Arc::new(Mutex::new(Vec::new()));
            let handshakes = Arc::new(AtomicUsize::new(0));
            let reply = Arc::new(reply);
            let (s, h) = (seen.clone(), handshakes.clone());
            std::thread::spawn(move || {
                for stream in listener.incoming() {
                    let Ok(mut stream) = stream else { continue };
                    let (config, reply, seen, handshakes) = (config.clone(), reply.clone(), s.clone(), h.clone());
                    std::thread::spawn(move || {
                        stream.set_read_timeout(Some(Duration::from_secs(10))).ok();
                        match config {
                            Some(config) => {
                                let conn = rustls::ServerConnection::new(config).unwrap();
                                let mut tls = StreamOwned::new(conn, stream);
                                if tls.conn.complete_io(&mut tls.sock).is_err() {
                                    return;
                                }
                                handshakes.fetch_add(1, Ordering::SeqCst);
                                serve_one(&mut tls, &reply, &seen);
                            }
                            None => serve_one(&mut stream, &reply, &seen),
                        }
                    });
                }
            });
            TestWorld { port, seen, handshakes }
        }

        pub fn url(&self) -> String {
            format!("https://127.0.0.1:{}", self.port)
        }

        pub fn requests(&self) -> Vec<Seen> {
            self.seen.lock().unwrap().clone()
        }
    }

    fn serve_one<S: Read + Write>(stream: &mut S, reply: &Reply, seen: &Mutex<Vec<Seen>>) {
        let mut reader = BufReader::new(&mut *stream);
        let mut line = String::new();
        if reader.read_line(&mut line).unwrap_or(0) == 0 {
            return;
        }
        let mut parts = line.split_whitespace();
        let (method, path) = (parts.next().unwrap_or("").to_string(), parts.next().unwrap_or("").to_string());
        let mut headers = Vec::new();
        loop {
            let mut h = String::new();
            if reader.read_line(&mut h).unwrap_or(0) == 0 || h == "\r\n" {
                break;
            }
            if let Some((k, v)) = h.trim_end().split_once(':') {
                headers.push((k.trim().to_ascii_lowercase(), v.trim().to_string()));
            }
        }
        let length = headers.iter().find(|(k, _)| k == "content-length").and_then(|(_, v)| v.parse::<usize>().ok()).unwrap_or(0);
        let mut body = vec![0u8; length];
        if reader.read_exact(&mut body).is_err() {
            return;
        }
        let request = Seen { method, path, headers, body };
        seen.lock().unwrap().push(request.clone());
        let (status, extra, out) = reply(&request);
        let mut head = format!("HTTP/1.1 {status} X\r\nContent-Length: {}\r\nConnection: close\r\n", out.len());
        for (k, v) in extra {
            head.push_str(&format!("{k}: {v}\r\n"));
        }
        head.push_str("\r\n");
        let _ = stream.write_all(head.as_bytes());
        let _ = stream.write_all(&out);
        let _ = stream.flush();
    }

    pub fn ok_reply(body: &'static str) -> Reply {
        Box::new(move |_| (200, vec![("Content-Type".into(), "application/json".into())], body.as_bytes().to_vec()))
    }

    fn get(mode: Mode, url: &str) -> Result<u16, ureq::Error> {
        let (agent, _) = agent(mode, Duration::from_secs(10));
        agent.get(url).call().map(|r| r.status().as_u16())
    }

    #[test]
    fn the_fingerprint_is_eight_groups_of_the_pins_first_half() {
        let pin = pin_a();
        let f = fingerprint(&pin);
        assert_eq!(f.len(), 39);
        assert_eq!(f.split(' ').count(), 8);
        assert_eq!(f.replace(' ', ""), hex(&pin[..16]).to_ascii_uppercase());
        assert_eq!(pin_from_text(&pin_text(&pin)), Some(pin));
        assert_eq!(pin_from_text("short"), None);
    }

    #[test]
    fn the_pinned_key_passes_whatever_the_name_and_dates() {
        let world = TestWorld::start(Some(server_config(CERT_A, KEY_A, false)), ok_reply("{}"));
        assert_eq!(get(Mode::Exact(pin_a()), &format!("{}/x", world.url())).unwrap(), 200);
        // The same key in a certificate that expired long ago and names someone else: still the pinned key.
        let old = TestWorld::start(Some(server_config(CERT_A_OLD, KEY_A, false)), ok_reply("{}"));
        assert_eq!(get(Mode::Exact(pin_a()), &format!("{}/x", old.url())).unwrap(), 200);
        // And by a name rather than an address.
        assert_eq!(get(Mode::Exact(pin_a()), &format!("https://localhost:{}/x", world.port)).unwrap(), 200);
    }

    #[test]
    fn a_different_key_is_refused_before_a_byte_is_sent() {
        let world = TestWorld::start(Some(server_config(CERT_B, KEY_B, false)), ok_reply("{}"));
        let error = get(Mode::Exact(pin_a()), &format!("{}/x", world.url())).unwrap_err();
        assert!(!unreachable(&error), "{}", error_text(&error));
        assert!(world.requests().is_empty(), "no request reached the wrong key");
    }

    #[test]
    fn the_pinned_certificate_without_its_key_fails_the_handshake() {
        // Certificate A presented, signed with key B: the pin matches, the handshake's signature doesn't.
        let world = TestWorld::start(Some(server_config(CERT_A, KEY_B, false)), ok_reply("{}"));
        assert!(get(Mode::Exact(pin_a()), &format!("{}/x", world.url())).is_err());
        assert!(get(Mode::Record, &format!("{}/x", world.url())).is_err(), "record mode checks the signature too");
        assert!(world.requests().is_empty());
    }

    #[test]
    fn tls_12_is_never_offered() {
        let world = TestWorld::start(Some(server_config(CERT_A, KEY_A, true)), ok_reply("{}"));
        assert!(get(Mode::Exact(pin_a()), &format!("{}/x", world.url())).is_err());
        assert!(get(Mode::Record, &format!("{}/x", world.url())).is_err());
        assert!(world.requests().is_empty());
    }

    #[test]
    fn record_mode_takes_any_key_and_records_it() {
        let world = TestWorld::start(Some(server_config(CERT_B, KEY_B, false)), ok_reply("{}"));
        let (agent, verifier) = agent(Mode::Record, Duration::from_secs(10));
        assert_eq!(agent.get(&format!("{}/editor/hello", world.url())).call().unwrap().status().as_u16(), 200);
        assert_eq!(verifier.seen().map(|s| sha256(&s)), Some(pin_b()));
    }

    #[test]
    fn nothing_listening_is_unreachable() {
        let port = TcpListener::bind("127.0.0.1:0").unwrap().local_addr().unwrap().port();
        let error = get(Mode::Record, &format!("https://127.0.0.1:{port}/x")).unwrap_err();
        assert!(unreachable(&error), "{}", error_text(&error));
        // Plain HTTP where TLS was expected: something answered, not as an Editor does.
        let plain = TestWorld::start(None, ok_reply("{}"));
        let error = get(Mode::Record, &format!("https://127.0.0.1:{}/x", plain.port)).unwrap_err();
        assert!(!unreachable(&error), "{}", error_text(&error));
    }
}
