fn main() {
    // spike_report is granted to the main window only (capabilities/main.json): the Editor window must be refused.
    tauri_build::try_build(tauri_build::Attributes::new().app_manifest(tauri_build::AppManifest::new().commands(&["spike_report"])))
        .expect("tauri build script");
}
