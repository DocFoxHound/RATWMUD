// The end-to-end hook, only in a build with `--features e2e` (never in a release): a test on this computer drives the
// real app's page the way tests/client.browser.test.mjs drives Chromium (Desktop/tests/app.test.mjs).
//
// RATW_E2E=127.0.0.1:PORT: the shell connects to the test there. The test sends one JSON line per step:
//   {"id": 1, "js": "<an expression>"}   evaluated in the page (awaited); the reply {"id": 1, "ok": true, "value": ...}
//   {"quit": 0}                          the app exits with that code
//   {"close": true}                      the window is closed as by its X (the page's close question, then the exit)
use std::io::{BufRead, BufReader, Write};
use std::net::TcpStream;
use std::sync::Mutex;

use tauri::{AppHandle, Manager, State};

pub struct E2e {
    to_test: Mutex<Option<TcpStream>>,
}

pub fn wanted() -> Option<String> {
    std::env::var("RATW_E2E").ok().filter(|a| a.starts_with("127.0.0.1:"))
}

pub fn start(app: &AppHandle, address: String) -> Result<(), String> {
    let stream = TcpStream::connect(&address).map_err(|e| format!("RATW_E2E {address}: {e}"))?;
    let reader = stream.try_clone().map_err(|e| e.to_string())?;
    app.manage(E2e { to_test: Mutex::new(Some(stream)) });
    let app = app.clone();
    std::thread::spawn(move || {
        for line in BufReader::new(reader).lines() {
            let Ok(line) = line else { break };
            let Ok(step) = serde_json::from_str::<serde_json::Value>(&line) else { continue };
            if let Some(code) = step.get("quit").and_then(|c| c.as_i64()) {
                app.exit(code as i32);
                return;
            }
            if step.get("close").is_some() {
                if let Some(window) = app.get_webview_window("main") {
                    let _ = window.close();
                }
                continue;
            }
            let (Some(id), Some(js)) = (step.get("id").and_then(|i| i.as_u64()), step.get("js").and_then(|j| j.as_str())) else { continue };
            if let Some(window) = app.get_webview_window("main") {
                let _ = window.eval(script(id, js));
            }
        }
        app.exit(3); // The test went away.
    });
    Ok(())
}

/// The step, wrapped: awaited, its value (or error) sent back through `e2e_result`. No eval(): the page's CSP refuses it,
/// and a script the shell injects is not bound by it.
fn script(id: u64, js: &str) -> String {
    format!(
        "(async () => {{ let r; try {{ const v = await (async () => (\n{js}\n))(); r = {{id: {id}, ok: true, value: v === undefined ? null : v}}; }} \
         catch (e) {{ r = {{id: {id}, ok: false, value: String((e && e.stack) || e)}}; }} let line; \
         try {{ line = JSON.stringify(r); }} catch (e) {{ line = JSON.stringify({{id: {id}, ok: true, value: null}}); }} \
         window.__TAURI__.core.invoke('e2e_result', {{reply: line}}); }})();"
    )
}

#[tauri::command]
pub fn e2e_result(reply: String, e2e: State<'_, E2e>) {
    if let Some(stream) = e2e.to_test.lock().unwrap().as_mut() {
        let _ = stream.write_all(reply.replace('\n', " ").as_bytes());
        let _ = stream.write_all(b"\n");
    }
}
