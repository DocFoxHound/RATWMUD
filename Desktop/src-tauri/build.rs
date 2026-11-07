fn main() {
    // Our own commands need an AppManifest entry, and a grant in the capability (Tauri 2.11.1+, PR #15266).
    tauri_build::try_build(tauri_build::Attributes::new().app_manifest(tauri_build::AppManifest::new().commands(&[
        "spike_config",
        "spike_report",
        "relay_open",
        "relay_send",
        "relay_close",
        "loopback_relay",
    ])))
    .expect("tauri build script");
}
