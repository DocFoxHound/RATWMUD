// The signed Editor bundle (Docs/Design/78, 6.6, Q6): only code the user signed runs in the client (I6).
//
// Package/stage.sh builds Editor/ into the world's image and writes `bundle.json` beside its files:
//   {"v": 1, "key": KEY ID, "manifest": base64(MANIFEST), "signature": base64(Ed25519 signature of MANIFEST)}
//   MANIFEST = {"v": 1, "version": N, "editorApi": 1, "files": {"index.html": sha256 hex, "assets/x.js": ..., ...}}
// signed with the release key (Ed25519; its private half never in the repository). The shell holds the public half. On
// a new session it fetches bundle.json, checks the signature and that the version is no older than OLDEST, and then
// checks each page file's hash as it passes; anything else is refused ("This world's Editor isn't an official RATW
// build.").
//
// The release key: `bash Package/stage.sh --make-release-key` makes one (kept outside the repository) and prints the line
// to put in RELEASE_KEYS. Until the user has made it the list is empty, and a release build opens no world's Editor.
// A debug build also trusts RATW_EDITOR_DEV_KEY ("ID:base64 public key"), for tests and the laptop's end-to-end run.
use std::collections::HashMap;

use serde_json::Value;

use super::tls::{hex, sha256, unb64};

/// (key id, the Ed25519 public key, base64.)
pub const RELEASE_KEYS: &[(&str, &str)] = &[];

/// The oldest bundle a world may serve (stage.sh's version: the commit's time). Raised by a client release when an old
/// bundle has a known bug, or when the release key is rotated.
pub const OLDEST: u64 = 1_794_000_000;

const MOST_FILES: usize = 4000;

#[derive(Debug, Clone, PartialEq)]
pub struct Manifest {
    pub key: String,
    pub version: u64,
    pub files: HashMap<String, [u8; 32]>,
}

pub fn trusted_keys() -> Vec<(String, Vec<u8>)> {
    let mut keys: Vec<(String, Vec<u8>)> = RELEASE_KEYS.iter().filter_map(|(id, k)| Some((id.to_string(), unb64(k)?))).collect();
    if cfg!(debug_assertions) {
        if let Ok(dev) = std::env::var("RATW_EDITOR_DEV_KEY") {
            if let Some((id, k)) = dev.split_once(':') {
                if let Some(k) = unb64(k.trim()) {
                    keys.push((id.trim().to_string(), k));
                }
            }
        }
    }
    keys.retain(|(_, k)| k.len() == 32);
    keys
}

/// A page file's path as the manifest names it: no leading slash, no query, `index.html` for the root.
pub fn file_of(path_and_query: &str) -> Option<String> {
    let path = path_and_query.split(['?', '#']).next().unwrap_or("");
    let path = path.strip_prefix('/')?;
    let path = if path.is_empty() { "index.html" } else { path };
    path_ok(path).then(|| path.to_string())
}

fn path_ok(path: &str) -> bool {
    !path.is_empty() && path.len() <= 200 && !path.starts_with('/') && !path.ends_with('/')
        && path.split('/').all(|part| !part.is_empty() && part != "." && part != "..")
        && path.bytes().all(|b| b.is_ascii_alphanumeric() || matches!(b, b'.' | b'_' | b'-' | b'/'))
}

/// The page file's type, from its name (never the world's word for it). None: not a kind of file the Editor has.
pub fn content_type(file: &str) -> Option<&'static str> {
    let ext = file.rsplit_once('.').map(|(_, e)| e.to_ascii_lowercase())?;
    Some(match ext.as_str() {
        "html" => "text/html; charset=utf-8",
        "js" | "mjs" => "text/javascript; charset=utf-8",
        "css" => "text/css; charset=utf-8",
        "json" => "application/json",
        "png" => "image/png",
        "svg" => "image/svg+xml",
        "ico" => "image/x-icon",
        "woff2" => "font/woff2",
        "ttf" => "font/ttf",
        "webp" => "image/webp",
        "txt" => "text/plain; charset=utf-8",
        _ => return None,
    })
}

#[derive(Debug, PartialEq)]
pub enum Refused {
    Unreadable,
    UnknownKey,
    BadSignature,
    TooOld,
    Api,
}

pub fn verify(body: &[u8], keys: &[(String, Vec<u8>)], oldest: u64, api: u64) -> Result<Manifest, Refused> {
    if body.len() > 2 * 1024 * 1024 {
        return Err(Refused::Unreadable);
    }
    let outer: Value = serde_json::from_slice(body).map_err(|_| Refused::Unreadable)?;
    if outer.get("v").and_then(Value::as_u64) != Some(1) {
        return Err(Refused::Unreadable);
    }
    let key_id = outer.get("key").and_then(Value::as_str).ok_or(Refused::Unreadable)?;
    let manifest_bytes = outer.get("manifest").and_then(Value::as_str).and_then(unb64).ok_or(Refused::Unreadable)?;
    let signature = outer.get("signature").and_then(Value::as_str).and_then(unb64).ok_or(Refused::Unreadable)?;
    let (_, key) = keys.iter().find(|(id, _)| id == key_id).ok_or(Refused::UnknownKey)?;
    ring::signature::UnparsedPublicKey::new(&ring::signature::ED25519, key).verify(&manifest_bytes, &signature).map_err(|_| Refused::BadSignature)?;
    // Signed: now read.
    let manifest: Value = serde_json::from_slice(&manifest_bytes).map_err(|_| Refused::Unreadable)?;
    if manifest.get("v").and_then(Value::as_u64) != Some(1) {
        return Err(Refused::Unreadable);
    }
    let version = manifest.get("version").and_then(Value::as_u64).ok_or(Refused::Unreadable)?;
    if version < oldest {
        return Err(Refused::TooOld);
    }
    if manifest.get("editorApi").and_then(Value::as_u64) != Some(api) {
        return Err(Refused::Api);
    }
    let listed = manifest.get("files").and_then(Value::as_object).ok_or(Refused::Unreadable)?;
    if listed.is_empty() || listed.len() > MOST_FILES {
        return Err(Refused::Unreadable);
    }
    let mut files = HashMap::new();
    for (path, hash) in listed {
        let hash = hash.as_str().filter(|h| h.len() == 64).and_then(|h| {
            let mut out = [0u8; 32];
            for (i, chunk) in h.as_bytes().chunks(2).enumerate() {
                out[i] = u8::from_str_radix(std::str::from_utf8(chunk).ok()?, 16).ok()?;
            }
            Some(out)
        });
        match hash {
            Some(h) if path_ok(path) && content_type(path).is_some() => files.insert(path.clone(), h),
            _ => return Err(Refused::Unreadable),
        };
    }
    if !files.contains_key("index.html") {
        return Err(Refused::Unreadable);
    }
    Ok(Manifest { key: key_id.to_string(), version, files })
}

impl Manifest {
    /// Whether `body` is the signed file at `file`.
    pub fn holds(&self, file: &str, body: &[u8]) -> bool {
        self.files.get(file).is_some_and(|want| *want == sha256(body))
    }

    pub fn lists(&self, file: &str) -> bool {
        self.files.contains_key(file)
    }
}

/// For the tests and stage.sh's check: the manifest's hex hash of a file.
pub fn file_hash(body: &[u8]) -> String {
    hex(&sha256(body))
}

#[cfg(test)]
pub mod tests {
    use super::*;
    use crate::editor::tls::b64;
    use ring::signature::KeyPair;
    use serde_json::json;

    /// A bundle signed with a key made here: (bundle.json, the key list that trusts it).
    pub fn signed(files: &[(&str, &[u8])], version: u64) -> (Vec<u8>, Vec<(String, Vec<u8>)>) {
        let rng = ring::rand::SystemRandom::new();
        let pkcs8 = ring::signature::Ed25519KeyPair::generate_pkcs8(&rng).unwrap();
        let key = ring::signature::Ed25519KeyPair::from_pkcs8(pkcs8.as_ref()).unwrap();
        let listed: serde_json::Map<String, Value> = files.iter().map(|(p, b)| (p.to_string(), Value::String(file_hash(b)))).collect();
        let manifest = json!({"v": 1, "version": version, "editorApi": 1, "files": listed}).to_string();
        let signature = key.sign(manifest.as_bytes());
        let bundle = json!({"v": 1, "key": "test", "manifest": b64(manifest.as_bytes()), "signature": b64(signature.as_ref())});
        (bundle.to_string().into_bytes(), vec![("test".into(), key.public_key().as_ref().to_vec())])
    }

    #[test]
    fn a_signed_bundle_is_read_and_its_files_checked() {
        let (bundle, keys) = signed(&[("index.html", b"<html>"), ("assets/app.js", b"js")], OLDEST + 5);
        let m = verify(&bundle, &keys, OLDEST, 1).unwrap();
        assert!(m.holds("index.html", b"<html>"));
        assert!(!m.holds("index.html", b"<html> "), "a changed file");
        assert!(!m.holds("assets/evil.js", b"js"), "a file not on the list");
        assert!(m.lists("assets/app.js"));
    }

    #[test]
    fn an_unsigned_or_old_bundle_is_refused() {
        let (bundle, keys) = signed(&[("index.html", b"<html>")], OLDEST);
        assert!(verify(&bundle, &keys, OLDEST, 1).is_ok());
        assert_eq!(verify(&bundle, &keys, OLDEST + 1, 1), Err(Refused::TooOld), "older than this client accepts");
        assert_eq!(verify(&bundle, &keys, OLDEST, 2), Err(Refused::Api));
        let (_, others) = signed(&[("index.html", b"<html>")], OLDEST);
        assert_eq!(verify(&bundle, &others, OLDEST, 1), Err(Refused::BadSignature), "another key with the same id");
        assert_eq!(verify(&bundle, &[], OLDEST, 1), Err(Refused::UnknownKey));
        // The manifest changed after signing.
        let mut outer: Value = serde_json::from_slice(&bundle).unwrap();
        outer["manifest"] = Value::String(b64(json!({"v": 1, "version": OLDEST, "editorApi": 1, "files": {"index.html": file_hash(b"evil")}}).to_string().as_bytes()));
        assert_eq!(verify(outer.to_string().as_bytes(), &keys, OLDEST, 1), Err(Refused::BadSignature));
        assert_eq!(verify(b"<html>", &keys, OLDEST, 1), Err(Refused::Unreadable));
        // Paths the shell would never serve, and kinds of file the Editor has none of.
        for bad in ["../x.js", "/abs.js", "a//b.js", "run.exe", "x.html?y"] {
            let (b, k) = signed(&[("index.html", b"<html>"), (bad, b"x")], OLDEST);
            assert_eq!(verify(&b, &k, OLDEST, 1), Err(Refused::Unreadable), "{bad}");
        }
    }

    #[test]
    fn a_bundle_signed_as_stage_sh_signs_it() {
        // Made with stage.sh's own commands (openssl genpkey -algorithm ed25519; pkeyutl -sign -rawin) and a throwaway key.
        let bundle = include_bytes!("testdata/bundle.json");
        let public = include_str!("testdata/bundle.pub").trim();
        let keys = vec![("ratw-75e727b867f743a3".to_string(), unb64(public).unwrap())];
        let m = verify(bundle, &keys, 0, 1).unwrap();
        assert!(m.holds("index.html", include_bytes!("testdata/index.html")));
    }

    #[test]
    fn page_files_and_their_types() {
        assert_eq!(file_of("/").as_deref(), Some("index.html"));
        assert_eq!(file_of("/?suite=dm").as_deref(), Some("index.html"));
        assert_eq!(file_of("/assets/studio-x1.js?v=2").as_deref(), Some("assets/studio-x1.js"));
        assert_eq!(file_of("/a/../b.js"), None);
        assert_eq!(file_of("relative.js"), None);
        assert_eq!(content_type("assets/x.js"), Some("text/javascript; charset=utf-8"));
        assert_eq!(content_type("index.html"), Some("text/html; charset=utf-8"));
        assert_eq!(content_type("x.exe"), None);
        assert_eq!(content_type("noextension"), None);
    }

    #[test]
    fn the_dev_key_is_never_trusted_by_a_release_build() {
        // (cfg!(debug_assertions) gates it; this test runs in a debug build, where it is read.)
        std::env::set_var("RATW_EDITOR_DEV_KEY", format!("dev:{}", b64(&[1u8; 32])));
        let keys = trusted_keys();
        std::env::remove_var("RATW_EDITOR_DEV_KEY");
        assert_eq!(keys.iter().any(|(id, _)| id == "dev"), cfg!(debug_assertions));
    }
}
