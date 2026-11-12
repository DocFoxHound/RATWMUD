// The Editor's device tokens (Docs/Design/78, 6.4-6.5, Q5): a world gives a device token on a good sign-in, and a
// sign-in that shows it later has its own lane, which strangers' failures never fill. They are credentials, so they go in
// the OS keychain, one entry per world key and account (`editor-device@PIN/USERNAME`), as central's refresh token does
// (tokens.rs: keychain first, a file only this user can read where there is none). Never in editor-worlds.json.
//
// By the key's pin, not the world id a hello or a code claims (doc 78's R1 review, security 4): a hostile world naming
// another world's id was handed that world's token. A signed rotation carries them over (editor.rs, connect).
use std::collections::HashMap;
use std::path::PathBuf;
use std::sync::Mutex;

use super::tls::{pin_text, Pin};
use crate::tokens::{FileStore, KeychainStore, TokenStore};

pub struct Devices {
    kind: String,
    folder: PathBuf,
    memory: Mutex<HashMap<String, String>>,
}

impl Devices {
    /// `kind` is the shell's --token-store: keychain (default), file or memory.
    pub fn new(kind: &str, folder: PathBuf) -> Self {
        Devices { kind: kind.to_string(), folder, memory: Mutex::new(HashMap::new()) }
    }

    fn store(&self, pin: &Pin, username: &str) -> Option<Box<dyn TokenStore>> {
        // (A pin is base64url, and the account name is checked: nothing a path could be made of.)
        if !super::wire::username_ok(username) {
            return None;
        }
        let key = pin_text(pin);
        let file = || Box::new(FileStore::at(self.folder.join(format!("editor-device-{key}-{username}.token")))) as Box<dyn TokenStore>;
        match self.kind.as_str() {
            "memory" => None,
            "file" => Some(file()),
            _ => match KeychainStore::open_account(&format!("editor-device@{key}/{username}")) {
                Ok(k) => Some(Box::new(k)),
                Err(_) => Some(file()),
            },
        }
    }

    pub fn kind(&self, pin: &Pin, username: &str) -> &'static str {
        if self.kind == "memory" {
            return "memory";
        }
        self.store(pin, username).map(|s| s.kind()).unwrap_or("none")
    }

    pub fn load(&self, pin: &Pin, username: &str) -> Option<String> {
        if !super::wire::username_ok(username) {
            return None;
        }
        if self.kind == "memory" {
            return self.memory.lock().unwrap().get(&format!("{}/{username}", pin_text(pin))).cloned();
        }
        self.store(pin, username)?.load()
    }

    pub fn save(&self, pin: &Pin, username: &str, token: &str) {
        if !super::wire::username_ok(username) {
            return;
        }
        if self.kind == "memory" {
            self.memory.lock().unwrap().insert(format!("{}/{username}", pin_text(pin)), token.to_string());
            return;
        }
        if let Some(store) = self.store(pin, username) {
            if let Err(e) = store.save(token) {
                eprintln!("ratw-desktop: the Editor's device token could not be kept: {e}");
            }
        }
    }

    pub fn clear(&self, pin: &Pin, username: &str) {
        if !super::wire::username_ok(username) {
            return;
        }
        if self.kind == "memory" {
            self.memory.lock().unwrap().remove(&format!("{}/{username}", pin_text(pin)));
            return;
        }
        if let Some(store) = self.store(pin, username) {
            store.clear();
        }
    }

    /// A signed rotation (the world's new key, signed by the old): the account's token goes with the pin.
    pub fn carry(&self, from: &Pin, to: &Pin, username: &str) {
        if let Some(token) = self.load(from, username) {
            self.save(to, username, &token);
            self.clear(from, username);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const A: Pin = [1u8; 32];
    const B: Pin = [2u8; 32];

    #[test]
    fn device_tokens_are_kept_per_key_and_account() {
        let dir = std::env::temp_dir().join(format!("ratw-desktop-test-devices-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        for kind in ["memory", "file"] {
            let d = Devices::new(kind, dir.clone());
            assert_eq!(d.load(&A, "pell"), None);
            d.save(&A, "pell", "dev-token-1");
            assert_eq!(d.load(&A, "pell").as_deref(), Some("dev-token-1"));
            assert_eq!(d.load(&A, "host-k7q2"), None, "another account's");
            assert_eq!(d.load(&B, "pell"), None, "another key's, whatever world it claims");
            d.carry(&A, &B, "pell");
            assert_eq!((d.load(&A, "pell"), d.load(&B, "pell").as_deref()), (None, Some("dev-token-1")), "a rotation carries it");
            d.clear(&B, "pell");
            assert_eq!(d.load(&B, "pell"), None);
            // Nothing that could name another file.
            d.save(&A, "../../x", "x");
            assert_eq!(d.load(&A, "../../x"), None);
        }
        let leftovers: Vec<_> = std::fs::read_dir(&dir).map(|r| r.flatten().map(|e| e.file_name()).collect()).unwrap_or_default();
        assert!(leftovers.is_empty(), "{leftovers:?}");
        let _ = std::fs::remove_dir_all(&dir);
    }
}
