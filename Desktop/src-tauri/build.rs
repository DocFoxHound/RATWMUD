fn main() {
    // Our own commands need an AppManifest entry, and a grant in the capability (Tauri 2.11.1+, PR #15266). The
    // end-to-end hook's command exists only in an `e2e` build, and is granted at run time only when a test asks.
    // (The Editor's: editor.rs, COMMANDS.)
    const SHELL: &[&str] = &["central_info", "central_call", "central_resume", "world_status", "relay_address", "editor_state", "editor_connect",
        "editor_use_code", "editor_trust", "editor_forget", "editor_sign_in", "editor_change_password", "editor_step_up", "editor_accounts",
        "editor_sign_out", "editor_status", "editor_open"];
    const E2E: &[&str] = &["central_info", "central_call", "central_resume", "world_status", "relay_address", "editor_state", "editor_connect",
        "editor_use_code", "editor_trust", "editor_forget", "editor_sign_in", "editor_change_password", "editor_step_up", "editor_accounts",
        "editor_sign_out", "editor_status", "editor_open", "e2e_result"];
    let commands = if std::env::var_os("CARGO_FEATURE_E2E").is_some() { E2E } else { SHELL };
    let mut attributes = tauri_build::Attributes::new().app_manifest(tauri_build::AppManifest::new().commands(commands));
    // Windows (MSVC): the Common Controls v6 manifest goes into every binary this crate links, the test harness too, not
    // only the app (tauri-build's own way): without it `cargo test` stops before any test with
    // STATUS_ENTRYPOINT_NOT_FOUND, since the Editor (doc 78, M4) links an entry point only Common Controls v6 has.
    let os = std::env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    let env = std::env::var("CARGO_CFG_TARGET_ENV").unwrap_or_default();
    if os == "windows" && env == "msvc" {
        let manifest = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("windows-app-manifest.xml");
        println!("cargo:rerun-if-changed={}", manifest.display());
        println!("cargo:rustc-link-arg=/MANIFEST:EMBED");
        println!("cargo:rustc-link-arg=/MANIFESTINPUT:{}", manifest.display());
        attributes = attributes.windows_attributes(tauri_build::WindowsAttributes::new_without_app_manifest());
    }
    tauri_build::try_build(attributes).expect("tauri build script");
}
