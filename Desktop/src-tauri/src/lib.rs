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
//   --e2e-check            (an e2e build only) print "ratw-desktop e2e" and exit: how the tests know the hook is in
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
    page_origin_on(url, cfg!(windows))
}

/// The rule for one platform: `tauri://localhost` on Linux and macOS; `http://tauri.localhost` on Windows (WebView2,
/// without `useHttpsScheme`). Never a port: `http://tauri.localhost:7788` is whatever answers on this computer's 7788.
pub fn page_origin_on(url: &tauri::Url, windows: bool) -> bool {
    if url.as_str() == "about:blank" {
        return true;
    }
    let wanted = if windows { "http" } else { "tauri" };
    let host = if windows { "tauri.localhost" } else { "localhost" };
    url.scheme() == wanted && url.host_str() == Some(host) && url.port().is_none() && url.username().is_empty() && url.password().is_none()
}

/// Async (on the blocking pool): reading the keychain can wait on an unlock prompt, and must never hold the window.
#[tauri::command]
async fn central_info(central: State<'_, Arc<Central>>) -> Result<Value, String> {
    let central = central.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        json!({"where": central.shown, "development": central.development, "remembered": central.remembered(), "store": central.store_kind()})
    })
    .await
    .map_err(|e| e.to_string())
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
    // (Shown first: a hidden window has no monitor on Wayland. With none found at all, maximised, the safe side.)
    let monitor = window.current_monitor().ok().flatten()
        .or_else(|| window.primary_monitor().ok().flatten())
        .or_else(|| window.available_monitors().ok().and_then(|m| m.into_iter().next()));
    let Some(monitor) = monitor else {
        let _ = window.maximize();
        return;
    };
    let area = monitor.work_area().size.to_logical::<f64>(monitor.scale_factor());
    if area.width < 1280.0 * 1.05 || area.height < 800.0 * 1.05 {
        let _ = window.maximize();
    } else {
        let _ = window.center();
    }
}

/// macOS's menu bar, built by hand (doc 76, 4.4): Tauri's default menu has a Quit that ends the app at once, without
/// the page's question in the world. Ours closes the window instead, which asks first. Edit keeps copy and paste.
#[cfg_attr(not(target_os = "macos"), allow(dead_code))]
fn mac_menu(app: &tauri::AppHandle) -> tauri::Result<tauri::menu::Menu<tauri::Wry>> {
    use tauri::menu::{MenuBuilder, MenuItemBuilder, SubmenuBuilder};
    let quit = MenuItemBuilder::with_id("quit", "Quit Run Against the World").accelerator("CmdOrCtrl+Q").build(app)?;
    let ours = SubmenuBuilder::new(app, "Run Against the World").hide().hide_others().show_all().separator().item(&quit).build()?;
    let edit = SubmenuBuilder::new(app, "Edit").undo().redo().separator().cut().copy().paste().select_all().build()?;
    let window = SubmenuBuilder::new(app, "Window").minimize().separator().close_window().build()?;
    MenuBuilder::new(app).items(&[&ours, &edit, &window]).build()
}

/// Closing goes through the window (the page asks first in the world), whatever asked: our Quit, the Dock's, the OS.
fn close_main(app: &tauri::AppHandle) -> bool {
    match app.get_webview_window("main") {
        Some(window) => window.close().is_ok(),
        None => false,
    }
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    #[cfg(feature = "e2e")]
    if args.iter().any(|a| a == "--e2e-check") {
        println!("ratw-desktop e2e");
        return;
    }
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
    // With --data-dir (tests), the window's size and place are kept there too, never over the real ones.
    let state_file = opts.data_dir.as_ref().map(|d| d.join("window-state.json"));
    let mut window_state = tauri_plugin_window_state::Builder::default().with_state_flags(state_flags);
    if let Some(f) = &state_file {
        window_state = window_state.with_filename(f.to_string_lossy());
    }
    let builder = tauri::Builder::default()
        .plugin(window_state.build())
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
                let kept = match &state_file {
                    Some(f) => f.exists(),
                    None => app.path().app_config_dir().map(|d| d.join(tauri_plugin_window_state::DEFAULT_FILENAME).exists()).unwrap_or(false),
                };
                let _ = window.show();
                if !kept {
                    first_size(&window);
                }
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

    #[cfg(target_os = "macos")]
    let builder = builder.menu(mac_menu).on_menu_event(|app, event| {
        if event.id() == "quit" {
            close_main(app);
        }
    });

    let app = builder.build(tauri::generate_context!()).expect("the window could not be opened");
    app.run(|handle, event| {
        // An exit asked of the app while the window is open (the Dock's Quit, the OS logging out) closes the window
        // instead, so the page asks first in the world; once the window is gone, the app exits. app.exit(code), which
        // has a code, is let through.
        if let tauri::RunEvent::ExitRequested { code: None, api, .. } = &event {
            if close_main(handle) {
                api.prevent_exit();
            }
        }
    });
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
        let on = |url: &str, windows: bool| page_origin_on(&tauri::Url::parse(url).unwrap(), windows);
        for ok in ["tauri://localhost/", "tauri://localhost/index.html?x=1", "about:blank"] {
            assert!(on(ok, false), "{ok}");
        }
        for ok in ["http://tauri.localhost/", "http://tauri.localhost/index.html", "about:blank"] {
            assert!(on(ok, true), "{ok} on Windows");
        }
        for bad in ["https://evil.example.net/", "http://127.0.0.1:7788/", "tauri://evil/", "file:///etc/passwd", "http://tauri.localhost.evil.net/",
            "http://tauri.localhost/", "https://tauri.localhost/", "tauri://localhost:7788/", "about:srcdoc"] {
            assert!(!on(bad, false), "{bad}");
        }
        for bad in ["tauri://localhost/", "http://tauri.localhost:7788/", "https://tauri.localhost/", "http://localhost/", "http://u@tauri.localhost/"] {
            assert!(!on(bad, true), "{bad} on Windows");
        }
    }
}
