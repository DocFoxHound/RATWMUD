// Where "remember me" keeps central's refresh token (Docs/Design/77-central-accounts-and-hosted-worlds.md, 3.1 and 9):
// the OS's keychain (Secret Service on Linux, Credential Manager on Windows, Keychain on macOS); where there is none
// (a Linux desktop with no Secret Service running), a file only this user can read, in the app's data folder. The
// password is never kept, and the access token lives only in memory.
//
// One entry per central (its address), so a development central and the real one never share a sign-in.
use std::path::PathBuf;
use std::sync::Mutex;

pub trait TokenStore: Send + Sync {
    /// "keychain", "file" or "memory": shown by `central_info`, for the how-to and the tests.
    fn kind(&self) -> &'static str;
    fn load(&self) -> Option<String>;
    fn save(&self, token: &str) -> Result<(), String>;
    fn clear(&self);
}

/// Nothing kept past the window: for the tests, and `--token-store memory`.
#[derive(Default)]
pub struct MemoryStore {
    token: Mutex<Option<String>>,
}

impl TokenStore for MemoryStore {
    fn kind(&self) -> &'static str {
        "memory"
    }
    fn load(&self) -> Option<String> {
        self.token.lock().unwrap().clone()
    }
    fn save(&self, token: &str) -> Result<(), String> {
        *self.token.lock().unwrap() = Some(token.to_owned());
        Ok(())
    }
    fn clear(&self) {
        *self.token.lock().unwrap() = None;
    }
}

pub const SERVICE: &str = "net.ratw.client";

/// The OS's keychain, one entry named after central's address.
pub struct KeychainStore {
    entry: keyring::Entry,
}

impl KeychainStore {
    /// The keychain, if this computer has one that answers: a read that finds nothing is an answer; a platform failure
    /// (no Secret Service, a locked-out keychain) is not.
    pub fn open(central: &str) -> Result<Self, String> {
        Self::open_account(&format!("refresh@{central}"))
    }

    /// Any one entry of the app's (the Editor's device tokens: `editor-device@WORLD/USERNAME`, doc 78, 6.4).
    pub fn open_account(account: &str) -> Result<Self, String> {
        let entry = keyring::Entry::new(SERVICE, account).map_err(|e| e.to_string())?;
        match entry.get_password() {
            Ok(_) | Err(keyring::Error::NoEntry) => Ok(KeychainStore { entry }),
            Err(e) => Err(e.to_string()),
        }
    }
}

impl TokenStore for KeychainStore {
    fn kind(&self) -> &'static str {
        "keychain"
    }
    fn load(&self) -> Option<String> {
        self.entry.get_password().ok().filter(|t| valid(t))
    }
    fn save(&self, token: &str) -> Result<(), String> {
        self.entry.set_password(token).map_err(|e| e.to_string())
    }
    fn clear(&self) {
        let _ = self.entry.delete_credential();
    }
}

/// A file only this user can read (0600 on Unix; on Windows the user's own AppData, which other users can't read).
pub struct FileStore {
    path: PathBuf,
}

impl FileStore {
    /// `folder` is the app's data folder; the file is named after central's address.
    pub fn new(folder: PathBuf, central: &str) -> Self {
        let name: String = central.chars().map(|c| if c.is_ascii_alphanumeric() || c == '.' || c == '-' { c } else { '_' }).collect();
        FileStore { path: folder.join(format!("signin-{name}.token")) }
    }

    /// A file of its own (the Editor's device tokens where there is no keychain).
    pub fn at(path: PathBuf) -> Self {
        FileStore { path }
    }
}

impl TokenStore for FileStore {
    fn kind(&self) -> &'static str {
        "file"
    }
    fn load(&self) -> Option<String> {
        let text = std::fs::read_to_string(&self.path).ok()?;
        let token = text.trim().to_owned();
        valid(&token).then_some(token)
    }
    fn save(&self, token: &str) -> Result<(), String> {
        if !valid(token) {
            return Err("not a token".into());
        }
        if let Some(folder) = self.path.parent() {
            std::fs::create_dir_all(folder).map_err(|e| e.to_string())?;
        }
        // Written beside, then renamed over: never half a token, and the mode is set before a byte is written.
        let fresh = self.path.with_extension("token.new");
        let _ = std::fs::remove_file(&fresh);
        {
            use std::io::Write;
            let mut options = std::fs::OpenOptions::new();
            options.write(true).create_new(true);
            #[cfg(unix)]
            {
                use std::os::unix::fs::OpenOptionsExt;
                options.mode(0o600);
            }
            let mut f = options.open(&fresh).map_err(|e| e.to_string())?;
            f.write_all(token.as_bytes()).map_err(|e| e.to_string())?;
            f.sync_all().map_err(|e| e.to_string())?;
        }
        std::fs::rename(&fresh, &self.path).map_err(|e| e.to_string())
    }
    fn clear(&self) {
        let _ = std::fs::remove_file(&self.path);
    }
}

/// A refresh token as central makes them: short, printable, one line.
fn valid(token: &str) -> bool {
    !token.is_empty() && token.len() <= 512 && token.bytes().all(|b| b.is_ascii_graphic())
}

/// Which store: `keychain` (falls back to a file, saying so), `file`, or `memory`.
pub fn choose(wanted: &str, data_folder: PathBuf, central: &str) -> Result<Box<dyn TokenStore>, String> {
    match wanted {
        "memory" => Ok(Box::new(MemoryStore::default())),
        "file" => Ok(Box::new(FileStore::new(data_folder, central))),
        "keychain" => match KeychainStore::open(central) {
            Ok(store) => Ok(Box::new(store)),
            Err(e) => {
                eprintln!("ratw-desktop: no keychain ({e}); a remembered sign-in is kept in a file only you can read");
                Ok(Box::new(FileStore::new(data_folder, central)))
            }
        },
        other => Err(format!("--token-store {other}: keychain, file or memory")),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn folder(name: &str) -> PathBuf {
        let p = std::env::temp_dir().join(format!("ratw-desktop-test-{name}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&p);
        p
    }

    #[test]
    fn a_file_store_keeps_one_token_for_this_user_only() {
        let dir = folder("file");
        let store = FileStore::new(dir.clone(), "127.0.0.1:7790");
        assert_eq!(store.load(), None);
        store.save("R1").unwrap();
        store.save("R2").unwrap();
        assert_eq!(store.load().as_deref(), Some("R2"));
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            let mode = std::fs::metadata(&store.path).unwrap().permissions().mode() & 0o777;
            assert_eq!(mode, 0o600);
        }
        assert!(store.path.file_name().unwrap().to_str().unwrap().contains("127.0.0.1_7790"));
        assert!(store.save("two words").is_err());
        assert!(store.save("").is_err());
        store.clear();
        assert_eq!(store.load(), None);
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_tampered_file_is_not_a_token() {
        let dir = folder("tampered");
        let store = FileStore::new(dir.clone(), "c");
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(&store.path, "line one\nline two").unwrap();
        assert_eq!(store.load(), None);
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn the_store_is_chosen_by_name() {
        let dir = folder("choose");
        assert_eq!(choose("memory", dir.clone(), "c").unwrap().kind(), "memory");
        assert_eq!(choose("file", dir.clone(), "c").unwrap().kind(), "file");
        assert!(choose("cloud", dir, "c").is_err());
    }
}
