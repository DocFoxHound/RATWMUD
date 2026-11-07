fn main() {
    // Our own commands need an AppManifest entry, and a grant in the capability (Tauri 2.11.1+, PR #15266). The
    // end-to-end hook's command exists only in an `e2e` build, and is granted at run time only when a test asks.
    const SHELL: &[&str] = &["central_info", "central_call", "central_resume", "world_status", "relay_address"];
    const E2E: &[&str] = &["central_info", "central_call", "central_resume", "world_status", "relay_address", "e2e_result"];
    let commands = if std::env::var_os("CARGO_FEATURE_E2E").is_some() { E2E } else { SHELL };
    tauri_build::try_build(tauri_build::Attributes::new().app_manifest(tauri_build::AppManifest::new().commands(commands)))
        .expect("tauri build script");
}
