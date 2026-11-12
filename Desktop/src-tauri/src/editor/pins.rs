// The pin store (Docs/Design/78, 6.4): `editor-worlds.json` in the app's data folder, 0600, keyed by the world's key,
// with the addresses that key was met at as aliases, its world id and name, and the last username. Never a password, a
// session token or a device token (those are in memory and the keychain).
//
// What a connection's key means (screen 3):
//   trusted   the key this client already trusts, at an address it knew it at;
//   moved     the same key at a new address ("Ashfall, now at 192.168.1.21"): the address is added;
//   rotated   a new key, signed by the old one (`ratw-editor new-certificate`): the pin moves over;
//   new       a key and a world this client has never met (trust on first use, or an Editor code first);
//   changed   any other new key at an address this client trusts, whatever world id or name the hello claims, or a known
//             world id under a new key: the red screen, with no way to sign in from it.
use std::path::PathBuf;
use std::sync::Mutex;

use serde::{Deserialize, Serialize};

use super::tls::{pin_from_text, pin_text, sha256, unb64, Pin};
use super::wire::Hello;

const ADDRESSES_PER_KEY: usize = 8;
const KEYS: usize = 64;

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Known {
    /// The key's pin (base64url SHA-256 of its SPKI).
    pub pin: String,
    /// The key itself (base64 SPKI), once a connection has shown it: a rotation is checked against it. An Editor code
    /// carries only the pin, so it is empty until the first connection.
    #[serde(default)]
    pub spki: String,
    pub world: String,
    #[serde(default)]
    pub name: String,
    #[serde(default)]
    pub addresses: Vec<String>,
    #[serde(default)]
    pub username: String,
    #[serde(default)]
    pub used: u64,
}

#[derive(Debug, Default, Clone, Serialize, Deserialize)]
pub struct PinFile {
    #[serde(default)]
    pub keys: Vec<Known>,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Verdict {
    Trusted,
    Moved,
    Rotated { old: Known },
    New,
    Changed { old: Known },
}

impl Verdict {
    pub fn word(&self) -> &'static str {
        match self {
            Verdict::Trusted => "trusted",
            Verdict::Moved => "moved",
            Verdict::Rotated { .. } => "rotated",
            Verdict::New => "new",
            Verdict::Changed { .. } => "changed",
        }
    }
}

/// The message the old key signs for a rotation (tools/world_admin.py, rotation_message).
pub fn rotation_message(world: &str, new_pin: &Pin) -> Vec<u8> {
    format!("RATW Editor key rotation v1\n{world}\n{}\n", pin_text(new_pin)).into_bytes()
}

/// An EC P-256 SPKI's public point (what ring verifies with), or None for any other key.
fn p256_point(spki: &[u8]) -> Option<&[u8]> {
    // SEQUENCE { SEQUENCE { OID id-ecPublicKey, OID prime256v1 }, BIT STRING (0 unused bits, 0x04 || X || Y) }: always 91
    // bytes, with this fixed 26-byte head.
    const HEAD: [u8; 26] = [0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce,
        0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00];
    (spki.len() == 91 && spki[..26] == HEAD && spki[26] == 0x04).then(|| &spki[26..])
}

/// Whether the hello's rotation moves `old`'s pin to `spki`: the hello names old's world, says it replaces old's pin, carries
/// exactly the key the connection showed, and old's key signed it.
pub fn rotation_ok(old: &Known, spki: &[u8], hello: &Hello) -> bool {
    let Some(rotation) = &hello.rotation else { return false };
    let Some(old_spki) = unb64(&old.spki) else { return false };
    let Some(point) = p256_point(&old_spki) else { return false };
    if hello.world != old.world || rotation.previous != old.pin || rotation.spki != spki || sha256(&old_spki) == sha256(spki) {
        return false;
    }
    let message = rotation_message(&old.world, &sha256(spki));
    ring::signature::UnparsedPublicKey::new(&ring::signature::ECDSA_P256_SHA256_ASN1, point).verify(&message, &rotation.signature).is_ok()
}

pub fn classify(file: &PinFile, address: &str, spki: &[u8], hello: &Hello) -> Verdict {
    let pin = pin_text(&sha256(spki));
    let at = file.keys.iter().find(|k| k.addresses.iter().any(|a| a == address));
    if let Some(known) = file.keys.iter().find(|k| k.pin == pin) {
        // A key this client trusts belongs to one world: the same key claiming another world is not that world.
        if known.world != hello.world {
            return Verdict::Changed { old: known.clone() };
        }
        return if at.is_some_and(|k| k.pin == pin) { Verdict::Trusted } else { Verdict::Moved };
    }
    if let Some(old) = at.or_else(|| file.keys.iter().find(|k| k.world == hello.world)) {
        return if rotation_ok(old, spki, hello) { Verdict::Rotated { old: old.clone() } } else { Verdict::Changed { old: old.clone() } };
    }
    Verdict::New
}

pub fn now() -> u64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0)
}

impl PinFile {
    pub fn by_address(&self, address: &str) -> Option<&Known> {
        self.keys.iter().find(|k| k.addresses.iter().any(|a| a == address))
    }

    pub fn by_pin(&self, pin: &Pin) -> Option<&Known> {
        let text = pin_text(pin);
        self.keys.iter().find(|k| k.pin == text)
    }

    /// The address belongs to `pin`'s key alone, newest first, at most 8.
    fn alias(&mut self, pin: &str, address: &str) {
        for k in self.keys.iter_mut() {
            if k.pin != pin {
                k.addresses.retain(|a| a != address);
            }
        }
        if let Some(k) = self.keys.iter_mut().find(|k| k.pin == pin) {
            k.addresses.retain(|a| a != address);
            k.addresses.insert(0, address.to_string());
            k.addresses.truncate(ADDRESSES_PER_KEY);
        }
    }

    fn cap(&mut self) {
        if self.keys.len() > KEYS {
            self.keys.sort_by(|a, b| b.used.cmp(&a.used));
            self.keys.truncate(KEYS);
        }
    }

    /// After a connection that wasn't refused: what it learned is kept (the key's SPKI and the world's name, the new
    /// address, the rotated pin). `new` and `changed` keep nothing (trusting is the host's choice, editor_trust).
    pub fn apply(&mut self, verdict: &Verdict, address: &str, spki: &[u8], hello: &Hello) {
        let pin = pin_text(&sha256(spki));
        match verdict {
            Verdict::Trusted | Verdict::Moved => {}
            Verdict::Rotated { old } => {
                if let Some(k) = self.keys.iter_mut().find(|k| k.pin == old.pin) {
                    k.pin = pin.clone();
                }
            }
            Verdict::New | Verdict::Changed { .. } => return,
        }
        self.alias(&pin, address);
        if let Some(k) = self.keys.iter_mut().find(|k| k.pin == pin) {
            k.spki = super::tls::b64(spki);
            if !hello.name.is_empty() {
                k.name = hello.name.clone();
            }
            k.used = now();
        }
    }

    /// The host trusted a new key (screen 3's TRUST AND CONTINUE).
    pub fn trust(&mut self, address: &str, spki: &[u8], hello: &Hello) {
        let pin = pin_text(&sha256(spki));
        self.keys.retain(|k| k.pin != pin);
        self.keys.push(Known { pin: pin.clone(), spki: super::tls::b64(spki), world: hello.world.clone(), name: hello.name.clone(),
            addresses: vec![], username: String::new(), used: now() });
        self.alias(&pin, address);
        self.cap();
    }

    /// An Editor code pins its world's key ahead of the first connection. A code is the owner's word: it replaces any
    /// other key this client kept for that world (which also clears a red screen, doc 78 screen 3).
    pub fn pin_code(&mut self, world: &str, pin: &Pin, username: &str, address: Option<&str>) {
        let text = pin_text(pin);
        self.keys.retain(|k| k.world != world || k.pin == text);
        if let Some(k) = self.keys.iter_mut().find(|k| k.pin == text) {
            if k.world != world {
                // (The same key under another world id: the code's word wins; its addresses go with the old claim.)
                *k = Known { pin: text.clone(), spki: String::new(), world: world.into(), name: String::new(), addresses: vec![], username: String::new(), used: 0 };
            }
            k.username = username.into();
            k.used = now();
        } else {
            self.keys.push(Known { pin: text.clone(), spki: String::new(), world: world.into(), name: String::new(), addresses: vec![],
                username: username.into(), used: now() });
        }
        if let Some(a) = address {
            self.alias(&text, a);
        }
        self.cap();
    }

    /// Forgetting a changed key: only when the name typed is the one this client knew the world by (or its id, for a
    /// world it never learned a name for).
    pub fn forget(&mut self, pin: &str, typed: &str) -> bool {
        let typed = typed.trim().to_lowercase();
        let Some(at) = self.keys.iter().position(|k| k.pin == pin) else { return false };
        let k = &self.keys[at];
        let expected = if k.name.trim().is_empty() { k.world.to_lowercase() } else { k.name.trim().to_lowercase() };
        if typed.is_empty() || typed != expected {
            return false;
        }
        self.keys.remove(at);
        true
    }

    pub fn signed_in(&mut self, pin: &Pin, username: &str) {
        let text = pin_text(pin);
        if let Some(k) = self.keys.iter_mut().find(|k| k.pin == text) {
            k.username = username.into();
            k.used = now();
        }
    }

    /// Recent worlds (screen 2): the name, the newest address and the last username, newest first.
    pub fn recent(&self) -> Vec<serde_json::Value> {
        let mut keys: Vec<&Known> = self.keys.iter().filter(|k| !k.addresses.is_empty()).collect();
        keys.sort_by(|a, b| b.used.cmp(&a.used));
        keys.iter().take(12).map(|k| serde_json::json!({"world": k.world, "name": k.name, "address": k.addresses[0],
            "username": k.username, "pin": k.pin})).collect()
    }

    /// Removes a world from Recent worlds (its FORGET): the key and every address.
    pub fn remove(&mut self, pin: &str) -> bool {
        let before = self.keys.len();
        self.keys.retain(|k| k.pin != pin);
        before != self.keys.len()
    }

    pub fn pin_of(k: &Known) -> Option<Pin> {
        pin_from_text(&k.pin)
    }
}

/// The store: the file (None in memory, for tests), read once, written whole on every change.
pub struct PinStore {
    path: Option<PathBuf>,
    pub file: Mutex<PinFile>,
}

impl PinStore {
    pub fn memory() -> Self {
        PinStore { path: None, file: Mutex::new(PinFile::default()) }
    }

    pub fn open(folder: PathBuf) -> Self {
        let path = folder.join("editor-worlds.json");
        let file = match std::fs::read(&path) {
            Ok(bytes) => serde_json::from_slice::<PinFile>(&bytes).unwrap_or_else(|_| {
                // (Unreadable: kept aside, never trusted, never silently emptied in place.)
                let _ = std::fs::rename(&path, path.with_extension("json.unreadable"));
                PinFile::default()
            }),
            Err(_) => PinFile::default(),
        };
        PinStore { path: Some(path), file: Mutex::new(file) }
    }

    pub fn read<T>(&self, f: impl FnOnce(&PinFile) -> T) -> T {
        f(&self.file.lock().unwrap())
    }

    /// A change, then the file written (beside, then renamed over; 0600 before a byte is written).
    pub fn change<T>(&self, f: impl FnOnce(&mut PinFile) -> T) -> T {
        let mut file = self.file.lock().unwrap();
        let out = f(&mut file);
        if let Some(path) = &self.path {
            if let Err(e) = write_private(path, &serde_json::to_vec_pretty(&*file).unwrap_or_default()) {
                eprintln!("ratw-desktop: the Editor's known worlds could not be saved: {e}");
            }
        }
        out
    }
}

pub fn write_private(path: &std::path::Path, bytes: &[u8]) -> std::io::Result<()> {
    use std::io::Write;
    if let Some(folder) = path.parent() {
        std::fs::create_dir_all(folder)?;
    }
    let fresh = path.with_extension("new");
    let _ = std::fs::remove_file(&fresh);
    {
        let mut options = std::fs::OpenOptions::new();
        options.write(true).create_new(true);
        #[cfg(unix)]
        {
            use std::os::unix::fs::OpenOptionsExt;
            options.mode(0o600);
        }
        let mut f = options.open(&fresh)?;
        f.write_all(bytes)?;
        f.sync_all()?;
    }
    std::fs::rename(&fresh, path)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::editor::tls::tests::{CERT_A, CERT_B, KEY_A};
    use crate::editor::tls::{b64, spki_of};
    use crate::editor::wire::{Progress, Rotation};
    use rustls::pki_types::CertificateDer;

    fn spki(cert: &[u8]) -> Vec<u8> {
        spki_of(&CertificateDer::from(cert)).unwrap()
    }

    fn hello(world: &str, name: &str) -> Hello {
        Hello { world: world.into(), name: name.into(), package: String::new(), editor_api: 1, official: false, server_state: "running".into(),
            progress: Progress { code: "ready".into(), percent: 100 }, setup_pending: false, rotation: None }
    }

    const W1: &str = "world-11111111111111111111111111111111";
    const W2: &str = "world-22222222222222222222222222222222";

    /// A rotation from A to B, signed by A's key (as `ratw-editor new-certificate` signs, with openssl: the same ECDSA
    /// P-256 SHA-256 DER signature ring makes here).
    fn rotation(world: &str, signer: &[u8]) -> Rotation {
        let rng = ring::rand::SystemRandom::new();
        let key = ring::signature::EcdsaKeyPair::from_pkcs8(&ring::signature::ECDSA_P256_SHA256_ASN1_SIGNING, signer, &rng).unwrap();
        let signature = key.sign(&rng, &rotation_message(world, &sha256(&spki(CERT_B)))).unwrap();
        Rotation { spki: spki(CERT_B), signature: signature.as_ref().to_vec(), previous: pin_text(&sha256(&spki(CERT_A))) }
    }

    #[test]
    fn a_pin_is_kept_by_key_with_its_addresses() {
        let (a, mut f) = (spki(CERT_A), PinFile::default());
        assert_eq!(classify(&f, "127.0.0.1:17789", &a, &hello(W1, "Ashfall")), Verdict::New);
        f.trust("127.0.0.1:17789", &a, &hello(W1, "Ashfall"));
        assert_eq!(classify(&f, "127.0.0.1:17789", &a, &hello(W1, "Ashfall")), Verdict::Trusted);
        // The same key at a new address: moved, and the address added to it.
        let v = classify(&f, "192.168.1.21:17789", &a, &hello(W1, "Ashfall"));
        assert_eq!(v, Verdict::Moved);
        f.apply(&v, "192.168.1.21:17789", &a, &hello(W1, "Ashfall Vale"));
        assert_eq!(f.keys.len(), 1);
        assert_eq!(f.keys[0].addresses, vec!["192.168.1.21:17789", "127.0.0.1:17789"]);
        assert_eq!(f.keys[0].name, "Ashfall Vale", "the name follows the world's");
        assert_eq!(classify(&f, "192.168.1.21:17789", &a, &hello(W1, "x")), Verdict::Trusted);
    }

    #[test]
    fn a_new_key_at_a_trusted_address_is_red_whatever_the_hello_claims() {
        let (a, b, mut f) = (spki(CERT_A), spki(CERT_B), PinFile::default());
        f.trust("127.0.0.1:17789", &a, &hello(W1, "Ashfall"));
        for claimed in [hello(W1, "Ashfall"), hello(W2, "Somewhere New"), hello(W2, "")] {
            assert!(matches!(classify(&f, "127.0.0.1:17789", &b, &claimed), Verdict::Changed { .. }), "{claimed:?}");
        }
        // A known world id under a new key, at an address never seen: red too.
        assert!(matches!(classify(&f, "10.0.0.9:17789", &b, &hello(W1, "Ashfall")), Verdict::Changed { .. }));
        // The trusted key claiming to be another world: red.
        assert!(matches!(classify(&f, "127.0.0.1:17789", &a, &hello(W2, "Ashfall")), Verdict::Changed { .. }));
        // Nothing was kept from any of them.
        let before = f.keys.clone();
        f.apply(&Verdict::Changed { old: before[0].clone() }, "127.0.0.1:17789", &b, &hello(W2, "x"));
        assert_eq!(f.keys, before);
    }

    #[test]
    fn a_rotation_signed_by_the_old_key_moves_the_pin() {
        let (a, b, mut f) = (spki(CERT_A), spki(CERT_B), PinFile::default());
        f.trust("127.0.0.1:17789", &a, &hello(W1, "Ashfall"));
        let mut h = hello(W1, "Ashfall");
        h.rotation = Some(rotation(W1, KEY_A));
        let v = classify(&f, "127.0.0.1:17789", &b, &h);
        assert!(matches!(v, Verdict::Rotated { .. }), "{v:?}");
        // From a new address too (the world's id and the old key's signature carry it).
        assert!(matches!(classify(&f, "192.168.1.30:17789", &b, &h), Verdict::Rotated { .. }));
        f.apply(&v, "127.0.0.1:17789", &b, &h);
        assert_eq!(f.keys.len(), 1);
        assert_eq!(f.keys[0].pin, pin_text(&sha256(&b)));
        assert_eq!(f.keys[0].spki, b64(&b));
        assert_eq!(classify(&f, "127.0.0.1:17789", &b, &hello(W1, "Ashfall")), Verdict::Trusted);
        // Refused: signed for another world, signed by the new key itself, naming another previous pin, carrying another
        // key than the one shown, or an old key this client never saw (a code pinned it, no connection yet).
        let mut g = PinFile::default();
        g.trust("127.0.0.1:17789", &a, &hello(W1, "Ashfall"));
        let bad = |r: Rotation| { let mut h = hello(W1, "Ashfall"); h.rotation = Some(r); h };
        assert!(matches!(classify(&g, "127.0.0.1:17789", &b, &bad(rotation(W2, KEY_A))), Verdict::Changed { .. }));
        assert!(matches!(classify(&g, "127.0.0.1:17789", &b, &bad(rotation(W1, crate::editor::tls::tests::KEY_B))), Verdict::Changed { .. }));
        let mut r = rotation(W1, KEY_A);
        r.previous = "x".into();
        assert!(matches!(classify(&g, "127.0.0.1:17789", &b, &bad(r)), Verdict::Changed { .. }));
        let mut r = rotation(W1, KEY_A);
        r.spki = a.clone();
        assert!(matches!(classify(&g, "127.0.0.1:17789", &b, &bad(r)), Verdict::Changed { .. }));
        let mut coded = PinFile::default();
        coded.pin_code(W1, &sha256(&a), "pell", Some("127.0.0.1:17789"));
        assert!(matches!(classify(&coded, "127.0.0.1:17789", &b, &bad(rotation(W1, KEY_A))), Verdict::Changed { .. }));
    }

    #[test]
    fn an_editor_code_pins_without_a_prompt_and_clears_a_changed_key() {
        let (a, b, mut f) = (spki(CERT_A), spki(CERT_B), PinFile::default());
        f.pin_code(W1, &sha256(&a), "pell", None);
        // The first connection, at whatever address: the code's key, so no screen; the key and the address are kept.
        let v = classify(&f, "203.0.113.7:17789", &a, &hello(W1, "Ashfall"));
        assert_eq!(v, Verdict::Moved);
        f.apply(&v, "203.0.113.7:17789", &a, &hello(W1, "Ashfall"));
        assert_eq!(f.keys[0].spki, b64(&a));
        assert_eq!(f.keys[0].username, "pell");
        // A new key for that world: red, until the owner's new code.
        assert!(matches!(classify(&f, "203.0.113.7:17789", &b, &hello(W1, "Ashfall")), Verdict::Changed { .. }));
        f.pin_code(W1, &sha256(&b), "pell", Some("203.0.113.7:17789"));
        assert_eq!(f.keys.len(), 1);
        assert_eq!(classify(&f, "203.0.113.7:17789", &b, &hello(W1, "Ashfall")), Verdict::Trusted);
    }

    #[test]
    fn forgetting_needs_the_worlds_name() {
        let (a, mut f) = (spki(CERT_A), PinFile::default());
        f.trust("127.0.0.1:17789", &a, &hello(W1, "Ashfall"));
        let pin = f.keys[0].pin.clone();
        assert!(!f.forget(&pin, ""));
        assert!(!f.forget(&pin, "Ashfal"));
        assert!(!f.forget("other", "Ashfall"));
        assert!(f.forget(&pin, "  ashfall "));
        assert!(f.keys.is_empty());
    }

    #[test]
    fn the_file_holds_no_secret_and_is_this_users_alone() {
        let dir = std::env::temp_dir().join(format!("ratw-desktop-test-pins-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        let store = PinStore::open(dir.clone());
        store.change(|f| {
            f.trust("127.0.0.1:17789", &spki(CERT_A), &hello(W1, "Ashfall"));
            f.signed_in(&sha256(&spki(CERT_A)), "host-k7q2");
        });
        let text = std::fs::read_to_string(dir.join("editor-worlds.json")).unwrap();
        let v: serde_json::Value = serde_json::from_str(&text).unwrap();
        let fields: Vec<&String> = v["keys"][0].as_object().unwrap().keys().collect();
        assert_eq!(fields, ["addresses", "name", "pin", "spki", "used", "username", "world"]);
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            assert_eq!(std::fs::metadata(dir.join("editor-worlds.json")).unwrap().permissions().mode() & 0o777, 0o600);
        }
        let again = PinStore::open(dir.clone());
        assert_eq!(again.read(|f| f.recent())[0]["username"], "host-k7q2");
        std::fs::write(dir.join("editor-worlds.json"), "{not json").unwrap();
        assert!(PinStore::open(dir.clone()).read(|f| f.keys.is_empty()), "an unreadable file trusts nothing");
        let _ = std::fs::remove_dir_all(&dir);
    }
}
