// Slop Pulse (SLAPorSLOP's player counts, Docs/Publishing-on-SLAPorSLOP.md): when the SLAPorSLOP app starts the client it
// sets SLOP_PULSE_TOKEN, SLOP_PULSE_URL and SLOP_PULSE_INTERVAL. The client then posts {"token": ...} to URL/start once,
// URL/heartbeat every INTERVAL seconds, and URL/end when it quits; a 422 (an unknown or ended token) stops it. Nothing
// else is ever sent: no device id, no account, nothing about the game. Without the three variables (not started from
// SLAPorSLOP, or the player turned "Share my playtime" off) it does nothing.
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;

pub struct Pulse {
    url: String,
    body: Vec<u8>,
    agent: ureq::Agent,
    stopped: AtomicBool,
}

/// The three variables, read: (url without a trailing slash, token, seconds). None unless all three are set.
pub fn settings(token: Option<String>, url: Option<String>, interval: Option<String>) -> Option<(String, String, u64)> {
    let (token, url, interval) = (token?, url?, interval?);
    let url = url.trim().trim_end_matches('/').to_string();
    if token.trim().is_empty() || !(url.starts_with("https://") || url.starts_with("http://")) {
        return None;
    }
    let every = interval.trim().parse::<u64>().ok().filter(|s| *s > 0).unwrap_or(60).clamp(10, 3600);
    Some((url, token.trim().to_string(), every))
}

impl Pulse {
    fn post(&self, path: &str) -> u16 {
        match self.agent.post(&format!("{}/{path}", self.url)).header("Content-Type", "application/json").send(&self.body[..]) {
            Ok(r) => r.status().as_u16(),
            Err(_) => 0, // (Offline: try again at the next heartbeat.)
        }
    }

    /// When the client quits (skipped once the token was refused).
    pub fn end(&self) {
        if !self.stopped.swap(true, Ordering::SeqCst) {
            self.post("end");
        }
    }
}

/// Starts the pulse if the SLAPorSLOP app asked for one: `start` now, then a heartbeat every interval, on a thread.
pub fn start() -> Option<Arc<Pulse>> {
    let var = |k: &str| std::env::var(k).ok();
    let (url, token, every) = settings(var("SLOP_PULSE_TOKEN"), var("SLOP_PULSE_URL"), var("SLOP_PULSE_INTERVAL"))?;
    let agent = ureq::Agent::config_builder()
        .timeout_global(Some(Duration::from_secs(10)))
        .http_status_as_error(false)
        .max_redirects(0)
        .user_agent(concat!("ratw-desktop/", env!("CARGO_PKG_VERSION")))
        .build()
        .into();
    let body = serde_json::json!({"token": token}).to_string().into_bytes();
    let pulse = Arc::new(Pulse { url, body, agent, stopped: AtomicBool::new(false) });
    let beating = pulse.clone();
    std::thread::spawn(move || {
        if beating.post("start") == 422 {
            beating.stopped.store(true, Ordering::SeqCst);
            return;
        }
        loop {
            std::thread::sleep(Duration::from_secs(every));
            if beating.stopped.load(Ordering::SeqCst) {
                return;
            }
            if beating.post("heartbeat") == 422 {
                beating.stopped.store(true, Ordering::SeqCst);
                return;
            }
        }
    });
    Some(pulse)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn s(v: &str) -> Option<String> {
        Some(v.to_string())
    }

    #[test]
    fn only_with_all_three_variables() {
        assert_eq!(settings(s("t0k"), s("https://slaporslop.games/v1/pulse/"), s("60")),
            Some(("https://slaporslop.games/v1/pulse".into(), "t0k".into(), 60)));
        assert_eq!(settings(None, s("https://x.example/v1/pulse"), s("60")), None);
        assert_eq!(settings(s("t"), None, s("60")), None);
        assert_eq!(settings(s("t"), s("https://x.example/v1/pulse"), None), None);
        assert_eq!(settings(s(" "), s("https://x.example/v1/pulse"), s("60")), None);
        assert_eq!(settings(s("t"), s("file:///etc/passwd"), s("60")), None);
        assert_eq!(settings(s("t"), s("https://x.example"), s("soon")).map(|v| v.2), Some(60));
        assert_eq!(settings(s("t"), s("https://x.example"), s("1")).map(|v| v.2), Some(10));
    }

    #[test]
    fn only_the_token_is_sent() {
        use std::io::{BufRead, BufReader, Read, Write};
        let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
        let url = format!("http://{}/v1/pulse", listener.local_addr().unwrap());
        let server = std::thread::spawn(move || {
            let mut seen = Vec::new();
            for status in [204, 204, 422] {
                let (stream, _) = listener.accept().unwrap();
                let mut reader = BufReader::new(stream.try_clone().unwrap());
                let mut head = String::new();
                let mut length = 0;
                loop {
                    let mut line = String::new();
                    reader.read_line(&mut line).unwrap();
                    if let Some(v) = line.to_ascii_lowercase().strip_prefix("content-length:") {
                        length = v.trim().parse().unwrap();
                    }
                    if line == "\r\n" {
                        break;
                    }
                    head.push_str(&line);
                }
                let mut body = vec![0; length];
                reader.read_exact(&mut body).unwrap();
                let reason = if status == 204 { "No Content" } else { "Unprocessable" };
                (&stream).write_all(format!("HTTP/1.1 {status} {reason}\r\ncontent-length: 0\r\nconnection: close\r\n\r\n").as_bytes()).unwrap();
                seen.push((head.lines().next().unwrap().to_string(), String::from_utf8(body).unwrap()));
            }
            seen
        });
        let agent = ureq::Agent::config_builder().http_status_as_error(false).build().into();
        let pulse = Pulse { url, body: br#"{"token":"t0k"}"#.to_vec(), agent, stopped: AtomicBool::new(false) };
        assert_eq!(pulse.post("start"), 204);
        assert_eq!(pulse.post("heartbeat"), 204);
        pulse.end();
        pulse.end(); // (Once only.)
        let seen = server.join().unwrap();
        assert_eq!(seen.iter().map(|(l, _)| l.split(' ').nth(1).unwrap().to_string()).collect::<Vec<_>>(),
            ["/v1/pulse/start", "/v1/pulse/heartbeat", "/v1/pulse/end"]);
        assert!(seen.iter().all(|(l, b)| l.starts_with("POST ") && b == r#"{"token":"t0k"}"#));
    }
}
