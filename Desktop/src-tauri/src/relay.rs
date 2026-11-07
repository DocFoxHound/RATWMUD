// The loopback relay (Docs/Design/77-central-accounts-and-hosted-worlds.md, section 9 and D1), only with `--relay`: a
// ws:// listener on 127.0.0.1 that the page opens with its own WebSocket, piped by the shell to a world. On Linux the
// page reaches plain ws:// worlds itself (D1), so this is the fallback for a webview that refuses them (Windows
// WebView2 is still to check). The page asks for `ws://127.0.0.1:PORT/TOKEN?to=<the world's ws:// or wss:// address>`.
//
// The random token in the path keeps other programs on this computer (and web pages in a browser, which can open
// loopback sockets too) off it: the world sees no Origin from the relay, so the relay must be the page's alone.
use futures_util::{SinkExt, StreamExt};
use tokio_tungstenite::tungstenite::handshake::server::{ErrorResponse, Request, Response};
use tokio_tungstenite::tungstenite::http::StatusCode;

pub struct Relay {
    pub port: u16,
    pub token: String,
}

impl Relay {
    pub fn address(&self) -> String {
        format!("ws://127.0.0.1:{}/{}", self.port, self.token)
    }
}

/// 32 random bytes, as hex.
pub fn random_token() -> Result<String, String> {
    let mut bytes = [0u8; 32];
    getrandom::fill(&mut bytes).map_err(|e| e.to_string())?;
    Ok(bytes.iter().map(|b| format!("{b:02x}")).collect())
}

/// Where a relayed socket may go: a world's ws:// or wss:// address at /ws, with no user name.
pub fn target(query: Option<&str>) -> Option<String> {
    let given = percent_decode(query?.strip_prefix("to=")?);
    let url = tauri::Url::parse(&given).ok()?;
    let fine = matches!(url.scheme(), "ws" | "wss")
        && url.host_str().is_some()
        && url.username().is_empty()
        && url.password().is_none()
        && url.path() == "/ws"
        && url.query().is_none()
        && url.fragment().is_none();
    fine.then(|| url.to_string())
}

pub async fn serve(listener: tokio::net::TcpListener, token: String) {
    loop {
        let Ok((stream, _)) = listener.accept().await else { continue };
        let _ = stream.set_nodelay(true);
        let token = token.clone();
        tauri::async_runtime::spawn(async move {
            let mut to = None;
            let check = |req: &Request, resp: Response| {
                let uri = req.uri();
                to = if uri.path() == format!("/{token}") { target(uri.query()) } else { None };
                if to.is_some() {
                    Ok(resp)
                } else {
                    let mut refuse = ErrorResponse::new(None);
                    *refuse.status_mut() = StatusCode::FORBIDDEN;
                    Err(refuse)
                }
            };
            let Ok(page) = tokio_tungstenite::accept_hdr_async(stream, check).await else { return };
            let Some(url) = to else { return };
            let Ok((world, _)) = tokio_tungstenite::connect_async_tls_with_config(url.as_str(), None, true, None).await else { return };
            let (mut page_tx, mut page_rx) = page.split();
            let (mut world_tx, mut world_rx) = world.split();
            let up = async {
                while let Some(Ok(m)) = page_rx.next().await {
                    if world_tx.send(m).await.is_err() {
                        break;
                    }
                }
                let _ = world_tx.close().await;
            };
            let down = async {
                while let Some(Ok(m)) = world_rx.next().await {
                    if page_tx.send(m).await.is_err() {
                        break;
                    }
                }
                let _ = page_tx.close().await;
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
        if bytes[i] == b'%' && i + 3 <= bytes.len() {
            if let Some(v) = s.get(i + 1..i + 3).and_then(|h| u8::from_str_radix(h, 16).ok()) {
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_relay_goes_only_to_a_world() {
        assert_eq!(target(Some("to=ws%3A%2F%2F192.168.1.20%3A7788%2Fws")).as_deref(), Some("ws://192.168.1.20:7788/ws"));
        assert_eq!(target(Some("to=wss%3A%2F%2Fworld.example.net%2Fws")).as_deref(), Some("wss://world.example.net/ws"));
        for bad in [None, Some(""), Some("to=http%3A%2F%2Fa.net%2Fws"), Some("to=ws%3A%2F%2Fa.net%2Fother"), Some("to=ws%3A%2F%2Fu%3Ap%40a.net%2Fws"),
            Some("to=ws%3A%2F%2Fa.net%2Fws%3Fx%3D1"), Some("from=ws%3A%2F%2Fa.net%2Fws"), Some("to=%zz")] {
            assert_eq!(target(bad), None, "{bad:?}");
        }
    }

    #[test]
    fn the_token_is_long_and_fresh() {
        let a = random_token().unwrap();
        assert_eq!(a.len(), 64);
        assert_ne!(a, random_token().unwrap());
    }

    /// A world on this computer that echoes, reached through the relay; a wrong token is refused.
    #[tokio::test]
    async fn frames_cross_the_relay_both_ways() {
        use futures_util::{SinkExt, StreamExt};
        use tokio_tungstenite::tungstenite::Message;
        let world = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
        let world_port = world.local_addr().unwrap().port();
        tokio::spawn(async move {
            while let Ok((stream, _)) = world.accept().await {
                tokio::spawn(async move {
                    let mut ws = tokio_tungstenite::accept_async(stream).await.unwrap();
                    while let Some(Ok(m)) = ws.next().await {
                        if m.is_binary() || m.is_text() {
                            ws.send(m).await.unwrap();
                        }
                    }
                });
            }
        });
        let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
        let relay = Relay { port: listener.local_addr().unwrap().port(), token: random_token().unwrap() };
        tokio::spawn(serve(listener, relay.token.clone()));
        let to = format!("ws%3A%2F%2F127.0.0.1%3A{world_port}%2Fws");
        let (mut page, _) = tokio_tungstenite::connect_async(format!("{}?to={to}", relay.address())).await.unwrap();
        page.send(Message::Binary(vec![1u8, 2, 3].into())).await.unwrap();
        assert_eq!(page.next().await.unwrap().unwrap(), Message::Binary(vec![1u8, 2, 3].into()));
        page.send(Message::Text("hello".into())).await.unwrap();
        assert_eq!(page.next().await.unwrap().unwrap(), Message::Text("hello".into()));
        let wrong = format!("ws://127.0.0.1:{}/{}?to={to}", relay.port, "0".repeat(64));
        assert!(tokio_tungstenite::connect_async(wrong).await.is_err(), "a wrong token is refused");
    }
}
