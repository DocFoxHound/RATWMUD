// The Editor's page files (Docs/Design/78, 6.6, as changed for SLAPorSLOP): built into the client, never fetched from a
// world. build.rs embeds Editor/dist (built in the release like Client/dist) and the portraits the page shows
// (Data/Portraits/*.png, as portraits/); the `editor` scheme serves them from here, and only /api/ (data) goes to the
// world (editor/proxy.rs). So the Editor window runs only code that came in the client's own release: a world, or
// anyone between, can't hand it a script (SLAPorSLOP's rule 1; doc 78's I6, which a signed bundle fetched from the
// world used to keep).
include!(concat!(env!("OUT_DIR"), "/editor_pages.rs"));

/// The page file at `file` (as `file_of` names it), if the client has one.
pub fn get(file: &str) -> Option<&'static [u8]> {
    FILES.iter().find(|(name, _)| *name == file).map(|(_, body)| *body)
}

/// Whether this build has the Editor's page at all (a debug build made before the Editor was built has none).
pub fn built() -> bool {
    get("index.html").is_some()
}

/// A page file's path as the client names it: no leading slash, no query, `index.html` for the root.
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

/// The page file's type, from its name. None: not a kind of file the Editor has.
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

#[cfg(test)]
mod tests {
    use super::*;

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
    fn the_built_in_page_is_the_editors_build() {
        // (Built before the tests, as the release builds it: Editor/dist and Data/Portraits.)
        if !built() {
            eprintln!("the Editor's page isn't built: npm --prefix Editor run build");
            return;
        }
        let index = String::from_utf8_lossy(get("index.html").unwrap());
        assert!(index.contains("<script"), "the Editor's index.html");
        assert!(FILES.iter().any(|(n, _)| n.starts_with("assets/") && n.ends_with(".js")));
        assert!(FILES.iter().any(|(n, _)| n.starts_with("portraits/") && n.ends_with(".png")), "the portraits the page shows");
        for (name, _) in FILES {
            assert!(file_of(&format!("/{name}")).as_deref() == Some(*name) && content_type(name).is_some(), "{name}");
        }
        assert!(get("bundle.json").is_none() && get("../x").is_none());
    }
}
