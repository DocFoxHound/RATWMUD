// The Editor's device tokens (Docs/Design/78, 6.4-6.5, Q5): a world gives a device token on a good sign-in, and a
// sign-in that shows it later has its own lane, which strangers' failures never fill. They are credentials, so they go in
// the OS keychain, one entry per world and account (`editor-device@WORLD/USERNAME`), as central's refresh token does
// (tokens.rs: keychain first, a file only this user can read where there is none). Never in editor-worlds.json.
use std::collections::HashMap;
use std::path::PathBuf;
use std::sync::Mutex;

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

    fn store(&self, world: &str, username: &str) -> Option<Box<dyn TokenStore>> {
        // (Both are checked by the callers: a world id and an account name, nothing a path could be made of.)
        if !super::wire::world_id(world) || !super::wire::username_ok(username) {
            return None;
        }
        let file = || Box::new(FileStore::at(self.folder.join(format!("editor-device-{world}-{username}.token")))) as Box<dyn TokenStore>;
        match self.kind.as_str() {
            "memory" => None,
            "file" => Some(file()),
            _ => match KeychainStore::open_account(&format!("editor-device@{world}/{username}")) {
                Ok(k) => Some(Box::new(k)),
                Err(_) => Some(file()),
            },
        }
    }

    pub fn kind(&self, world: &str, username: &str) -> &'static str {
        if self.kind == "memory" {
            return "memory";
        }
        self.store(world, username).map(|s| s.kind()).unwrap_or("none")
    }

    pub fn load(&self, world: &str, username: &str) -> Option<String> {
        if !super::wire::world_id(world) || !super::wire::username_ok(username) {
            return None;
        }
        if self.kind == "memory" {
            return self.memory.lock().unwrap().get(&format!("{world}/{username}")).cloned();
        }
        self.store(world, username)?.load()
    }

    pub fn save(&self, world: &str, username: &str, token: &str) {
        if !super::wire::world_id(world) || !super::wire::username_ok(username) {
            return;
        }
        if self.kind == "memory" {
            self.memory.lock().unwrap().insert(format!("{world}/{username}"), token.to_string());
            return;
        }
        if let Some(store) = self.store(world, username) {
            if let Err(e) = store.save(token) {
                eprintln!("ratw-desktop: the Editor's device token could not be kept: {e}");
            }
        }
    }

    pub fn clear(&self, world: &str, username: &str) {
        if !super::wire::world_id(world) || !super::wire::username_ok(username) {
            return;
        }
        if self.kind == "memory" {
            self.memory.lock().unwrap().remove(&format!("{world}/{username}"));
            return;
        }
        if let Some(store) = self.store(world, username) {
            store.clear();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const W: &str = "world-11111111111111111111111111111111";

    #[test]
    fn device_tokens_are_kept_per_world_and_account() {
        let dir = std::env::temp_dir().join(format!("ratw-desktop-test-devices-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        for kind in ["memory", "file"] {
            let d = Devices::new(kind, dir.clone());
            assert_eq!(d.load(W, "pell"), None);
            d.save(W, "pell", "dev-token-1");
            assert_eq!(d.load(W, "pell").as_deref(), Some("dev-token-1"));
            assert_eq!(d.load(W, "host-k7q2"), None, "another account's");
            d.clear(W, "pell");
            assert_eq!(d.load(W, "pell"), None);
            // Nothing that could name another file.
            d.save("../evil", "pell", "x");
            d.save(W, "../../x", "x");
            assert_eq!(d.load("../evil", "pell"), None);
        }
        let leftovers: Vec<_> = std::fs::read_dir(&dir).map(|r| r.flatten().map(|e| e.file_name()).collect()).unwrap_or_default();
        assert!(leftovers.is_empty(), "{leftovers:?}");
        let _ = std::fs::remove_dir_all(&dir);
    }
}
