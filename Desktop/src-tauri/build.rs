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
    editor_pages();
}

/// The Editor's page (Docs/Design/78, 6.6, as changed for SLAPorSLOP): Editor/dist and the portraits it shows
/// (Data/Portraits/*.png, as portraits/), built into the program, so the Editor window runs only code that came with the
/// client (editor/pages.rs). A release needs them (build the Editor first: npm --prefix ../Editor run build); a debug
/// build without them has an Editor window that says so.
fn editor_pages() {
    use std::path::{Path, PathBuf};
    fn walk(dir: &Path, prefix: &str, out: &mut Vec<(String, PathBuf)>) {
        let Ok(entries) = std::fs::read_dir(dir) else { return };
        let mut entries: Vec<_> = entries.flatten().collect();
        entries.sort_by_key(|e| e.file_name());
        for entry in entries {
            let name = entry.file_name().to_string_lossy().into_owned();
            let path = entry.path();
            let kind = entry.file_type().expect("a file's type");
            if kind.is_dir() {
                walk(&path, &format!("{prefix}{name}/"), out);
            } else if kind.is_file() && name != "bundle.json" {
                out.push((format!("{prefix}{name}"), path));
            }
        }
    }
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("..").join("..");
    let dist = root.join("Editor").join("dist");
    let portraits = root.join("Data").join("Portraits");
    println!("cargo:rerun-if-changed={}", dist.display());
    println!("cargo:rerun-if-changed={}", portraits.display());
    let mut files = Vec::new();
    if dist.join("index.html").is_file() {
        walk(&dist, "", &mut files);
        let mut pngs = Vec::new();
        walk(&portraits, "portraits/", &mut pngs);
        files.extend(pngs.into_iter().filter(|(name, _)| name.ends_with(".png") && !name["portraits/".len()..].contains('/')));
    } else if std::env::var("PROFILE").as_deref() == Ok("release") {
        panic!("the Editor's page isn't built ({}): npm --prefix ../Editor ci && npm --prefix ../Editor run build", dist.display());
    } else {
        println!("cargo:warning=the Editor's page isn't built ({}): this build's Editor window says so", dist.display());
    }
    let mut code = String::from("pub static FILES: &[(&str, &[u8])] = &[\n");
    for (name, path) in &files {
        code.push_str(&format!("    ({name:?}, include_bytes!({:?})),\n", path.to_string_lossy()));
    }
    code.push_str("];\n");
    let out = PathBuf::from(std::env::var("OUT_DIR").expect("OUT_DIR")).join("editor_pages.rs");
    std::fs::write(out, code).expect("editor_pages.rs");
}
