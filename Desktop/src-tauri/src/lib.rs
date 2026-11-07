// Run Against the World, the desktop client's shell (Docs/Design/76-the-desktop-client.md, as amended by
// Docs/Design/77-central-accounts-and-hosted-worlds.md, sections 3.1 and 9). The game's page (Client/dist) is bundled
// in the app, so no world can swap it. The shell:
//   - makes the page's calls to central and keeps the tokens (central.rs), with "remember me" in the OS's keychain
//     (tokens.rs);
//   - asks a world's /status for the world list (central.rs, world_status);
//   - keeps the window's size and place between launches (the window-state plugin), and the page on its own origin
//     (the navigation rule);
//   - with --relay, runs the loopback relay for worlds the webview can't reach itself (relay.rs).
//
// Arguments:
//   --central URL          central's address (default: central.rs BUILT_IN); https://, or http:// to this computer
//   --token-store KIND     keychain (default; a file when there is none), file, or memory
//   --data-dir PATH        where the app keeps its files (default: the OS's app data folder); for tests
//   --relay                run the loopback relay
mod central;
#[cfg(feature = "e2e")]
mod e2e;
mod relay;
mod tokens;

use std::path::PathBuf;
use std::sync::Arc;

use serde_json::{json, Value};
use tauri::{Manager, State};

use central::Central;

/// What the shell was started with.
#[derive(Debug, PartialEq)]
pub struct Options {
    pub central: String,
    pub token_store: String,
    pub data_dir: Option<PathBuf>,
    pub relay: bool,
}

pub fn options(args: &[String]) -> Result<Options, String> {
    let mut o = Options { central: central::BUILT_IN.into(), token_store: "keychain".into(), data_dir: None, relay: false };
    let mut i = 0;
    while i < args.len() {
        let (name, inline) = match args[i].split_once('=') {
            Some((n, v)) if n.starts_with("--") => (n.to_owned(), Some(v.to_owned())),
            _ => (args[i].clone(), None),
        };
        let mut value = || -> Result<String, String> {
            if let Some(v) = inline.clone() {
                return Ok(v);
            }
            i += 1;
            args.get(i).cloned().ok_or(format!("{name} needs a value"))
        };
        match name.as_str() {
            "--central" => o.central = value()?,
            "--token-store" => o.token_store = value()?,
            "--data-dir" => o.data_dir = Some(PathBuf::from(value()?)),
            "--relay" => o.relay = true,
            other if other.starts_with("--") => return Err(format!("{other}: not an option (--central, --token-store, --data-dir, --relay)")),
            _ => {} // (A file the OS hands the app, say: nothing to do with it.)
        }
        i += 1;
    }
    Ok(o)
}

/// The page's own origin, and nothing else: a link or a script can't take the window to another site, where the
/// shell's commands would answer it.
pub fn page_origin(url: &tauri::Url) -> bool {
    match (url.scheme(), url.host_str()) {
        ("tauri", Some("localhost")) => true,                               // Linux, macOS.
        ("http" | "https", Some("tauri.localhost")) => true,                // Windows (WebView2).
        ("about", _) => url.as_str() == "about:blank",
        _ => false,
    }
}

#[tauri::command]
fn central_info(central: State<'_, Arc<Central>>) -> Value {
    json!({"where": central.shown, "development": central.development, "remembered": central.remembered(), "store": central.store_kind()})
}

#[tauri::command]
async fn central_call(method: String, path: String, body: Option<Value>, central: State<'_, Arc<Central>>) -> Result<Value, String> {
    let central = central.inner().clone();
    tauri::async_runtime::spawn_blocking(move || central.call(&method, &path, body)).await.map_err(|e| e.to_string())
}

#[tauri::command]
async fn central_resume(central: State<'_, Arc<Central>>) -> Result<Value, String> {
    let central = central.inner().clone();
    tauri::async_runtime::spawn_blocking(move || central.resume()).await.map_err(|e| e.to_string())
}

#[tauri::command]
async fn world_status(host: String, port: u16, secure: bool) -> Result<Value, String> {
    tauri::async_runtime::spawn_blocking(move || central::world_status(&host, port, secure)).await.map_err(|e| e.to_string())
}

#[tauri::command]
fn relay_address(relay: State<'_, Option<relay::Relay>>) -> Option<String> {
    relay.as_ref().map(|r| r.address())
}

/// The first launch's window: 1280 x 800 (the config) where the screen has room, else maximised. Later launches get
/// the size and place the window-state plugin kept.
fn first_size(window: &tauri::WebviewWindow) {
    let Ok(Some(monitor)) = window.current_monitor() else { return };
    let area = monitor.work_area().size.to_logical::<f64>(monitor.scale_factor());
    if area.width < 1280.0 * 1.05 || area.height < 800.0 * 1.05 {
        let _ = window.maximize();
    } else {
        let _ = window.center();
    }
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let opts = match options(&args) {
        Ok(o) => o,
        Err(e) => {
            eprintln!("ratw-desktop: {e}");
            std::process::exit(2);
        }
    };
    let (base, shown) = match central::parse_central(&opts.central) {
        Ok(b) => b,
        Err(e) => {
            eprintln!("ratw-desktop: --central {}: {e}", opts.central);
            std::process::exit(2);
        }
    };
    let _ = rustls::crypto::ring::default_provider().install_default();

    let state_flags = tauri_plugin_window_state::StateFlags::all() - tauri_plugin_window_state::StateFlags::VISIBLE;
    let builder = tauri::Builder::default()
        .plugin(tauri_plugin_window_state::Builder::default().with_state_flags(state_flags).build())
        .plugin(
            tauri::plugin::Builder::<tauri::Wry, ()>::new("page-only")
                .on_navigation(|_, url| {
                    let ok = page_origin(url);
                    if !ok {
                        eprintln!("ratw-desktop: refused to leave the page for {url}");
                    }
                    ok
                })
                .build(),
        )
        .setup(move |app| {
            let data_dir = match &opts.data_dir {
                Some(d) => d.clone(),
                None => app.path().app_data_dir()?,
            };
            let store = tokens::choose(&opts.token_store, data_dir.clone(), &shown)?;
            let development = central::is_loopback(tauri::Url::parse(&base)?.host_str().unwrap_or_default());
            let central = Central::new(base.clone(), shown.clone(), development, Box::new(central::UreqHttp::new()), Arc::from(store));
            app.manage(Arc::new(central));

            let relay = if opts.relay {
                let listener = tauri::async_runtime::block_on(tokio::net::TcpListener::bind("127.0.0.1:0"))?;
                let r = relay::Relay { port: listener.local_addr()?.port(), token: relay::random_token()? };
                tauri::async_runtime::spawn(relay::serve(listener, r.token.clone()));
                Some(r)
            } else {
                None
            };
            app.manage(relay);

            if let Some(window) = app.get_webview_window("main") {
                let kept = app.path().app_config_dir().map(|d| d.join(tauri_plugin_window_state::DEFAULT_FILENAME).exists()).unwrap_or(false);
                if !kept {
                    first_size(&window);
                }
                let _ = window.show();
            }

            #[cfg(feature = "e2e")]
            if let Some(address) = e2e::wanted() {
                app.add_capability(
                    tauri::ipc::CapabilityBuilder::new("e2e").window("main").permission("allow-e2e-result"),
                )?;
                e2e::start(app.handle(), address)?;
            }
            Ok(())
        });

    #[cfg(not(feature = "e2e"))]
    let builder = builder.invoke_handler(tauri::generate_handler![central_info, central_call, central_resume, world_status, relay_address]);
    #[cfg(feature = "e2e")]
    let builder = builder.invoke_handler(tauri::generate_handler![central_info, central_call, central_resume, world_status, relay_address, e2e::e2e_result]);

    builder.run(tauri::generate_context!()).expect("the window could not be opened");
}

#[cfg(test)]
mod tests {
    use super::*;

    fn args(a: &[&str]) -> Vec<String> {
        a.iter().map(|s| s.to_string()).collect()
    }

    #[test]
    fn the_options() {
        let o = options(&[]).unwrap();
        assert_eq!(o.central, central::BUILT_IN);
        assert_eq!(o.token_store, "keychain");
        assert!(!o.relay);
        let o = options(&args(&["--central", "https://c.example.net", "--token-store=file", "--data-dir", "/tmp/x", "--relay"])).unwrap();
        assert_eq!(o, Options { central: "https://c.example.net".into(), token_store: "file".into(), data_dir: Some("/tmp/x".into()), relay: true });
        assert!(options(&args(&["--central"])).is_err());
        assert!(options(&args(&["--server", "x"])).is_err());
    }

    #[test]
    fn the_window_stays_on_the_page() {
        for ok in ["tauri://localhost/", "tauri://localhost/index.html?x=1", "http://tauri.localhost/", "https://tauri.localhost/a"] {
            assert!(page_origin(&tauri::Url::parse(ok).unwrap()), "{ok}");
        }
        for bad in ["https://evil.example.net/", "http://127.0.0.1:7788/", "tauri://evil/", "file:///etc/passwd", "http://tauri.localhost.evil.net/"] {
            assert!(!page_origin(&tauri::Url::parse(bad).unwrap()), "{bad}");
        }
    }
}
