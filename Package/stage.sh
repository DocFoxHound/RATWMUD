#!/usr/bin/env bash
# Stages the world image's build context (Docs/Design/77, section 8, and Package/README.md), and optionally builds it.
#
#   bash Package/stage.sh [--world EXPORT_DIR] [--out DIR] [--build-id ID] [--build] [--tag TAG]
#                         [--release-key PEM] [--editor-dist DIR | --no-editor]
#   bash Package/stage.sh --make-release-key [PEM]        the Editor's release key, once (below)
#   bash Package/stage.sh --sign-bundle DIR [--release-key PEM]   signs a built Editor in DIR (DIR/bundle.json)
#
# The repository is about 90 GB (artifacts/, .claude/worktrees/, Saved/), so the image is never built from its root:
# this copies exactly what the image needs into DIR (default build-package/context, under /build*/ and so ignored by
# git), checks the size, and with --build runs `docker build` there. DIR is replaced: it must be missing, empty, or a
# context this script staged before (it holds .ratw-context, written before anything else and kept while DIR is
# emptied, so a run that failed halfway can be run again); anything else is refused, never deleted.
# Only files git knows about, or would add (tracked or untracked but not ignored), are copied, from the working tree as
# it is now.
#   DIR/Dockerfile                 Package/Dockerfile
#   DIR/src/                       what the server is built from: CMakeLists.txt, Core/, Server/, Tests/ (CMake
#                                  configures every target, even when only ratw_server is built), Client/wasm/walk.cpp
#                                  and Data/Terrain/terrain.json (the client-data hash)
#   DIR/app/                       what runs: Data/ (the server's directories only), Database/migrations/, tools/*.py
#                                  (not the tests), the Package's entrypoint, ratw-editor and the breached-password list
#   DIR/world/                     the world export (tools/world_package.py export) given with --world; empty without
#                                  one, and the container then starts only with RATW_PACKAGE_IMPORT=skip
#   DIR/app/Editor/dist/           the Editor (Docs/Design/78, 6.6): Editor/ built (or --editor-dist), with its signed
#                                  bundle.json; Data/Portraits beside it (the page's /portraits/, signed as portraits/)
# The Editor bundle is signed with the release key (Ed25519; openssl): --release-key, else $RATW_EDITOR_RELEASE_KEY,
# else ~/.config/ratw/editor-release-key.pem. Without one, staging stops (or --no-editor stages no Editor at all: such
# an image runs the game, and its Editor window never opens). The private key never goes in the repository or the image;
# the client holds the public half (Desktop/src-tauri/src/editor/bundle.rs, RELEASE_KEYS). --make-release-key makes a
# key there (0600) and prints the line for RELEASE_KEYS; keep a copy of the file somewhere safe, beside central's keys.
# The build id is the commit (12 characters), with +dirty if any staged file differs from it; --build-id overrides it.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$root/build-package/context" world="" build_id="" build=0 tag="ratw-world"
release_key="${RATW_EDITOR_RELEASE_KEY:-${XDG_CONFIG_HOME:-$HOME/.config}/ratw/editor-release-key.pem}"
editor=1 editor_dist="" make_key="" sign_only=""
while (($#)); do
  case "$1" in
    --release-key) release_key="$2"; shift 2 ;;
    --no-editor) editor=0; shift ;;
    --editor-dist) editor_dist="$2"; shift 2 ;;
    --make-release-key) if (($# > 1)) && [[ "$2" != --* ]]; then make_key="$2"; shift 2; else make_key="default"; shift; fi ;;
    --sign-bundle) sign_only="$(realpath -- "$2")"; shift 2 ;;
    --world) world="$2"; shift 2 ;;
    --out) out="$2"; shift 2 ;;
    --build-id) build_id="$2"; shift 2 ;;
    --build) build=1; shift ;;
    --tag) tag="$2"; shift 2 ;;
    -h|--help) sed -n '2,32p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) echo "stage.sh: unknown argument $1 (see --help)" >&2; exit 2 ;;
  esac
done

# ----------------------------------------------------------------------------------- the Editor's release key and bundle
key_id() {   # The key's id: "ratw-" and the first 16 hex characters of the SHA-256 of its raw public key.
  echo "ratw-$(openssl pkey -in "$1" -pubout -outform DER | tail -c 32 | sha256sum | cut -c1-16)"
}
key_public() {   # The raw 32-byte public key, base64.
  openssl pkey -in "$1" -pubout -outform DER | tail -c 32 | base64 -w0
}
# Signs the built Editor in $1: every file under it (bundle.json aside) with its SHA-256, the version (the commit's time,
# never older than the client's OLDEST) and the Editor API, signed with the key in $2; writes $1/bundle.json.
sign_bundle() {
  local dist="$1" key="$2" version scratch
  [[ -f "$dist/index.html" ]] || { echo "stage.sh: $dist has no index.html: not a built Editor" >&2; return 2; }
  [[ -f "$key" ]] || { echo "stage.sh: no Editor release key at $key (--release-key, or --make-release-key to make one; --no-editor to stage without the Editor)" >&2; return 2; }
  openssl pkey -in "$key" -noout -text 2>/dev/null | grep -q ED25519 || { echo "stage.sh: $key isn't an Ed25519 private key" >&2; return 2; }
  version="${RATW_BUNDLE_VERSION:-$(git -C "$root" log -1 --format=%ct HEAD)}"
  scratch="$(mktemp -d)"
  python3 - "$dist" "$version" "$scratch/manifest" <<'PY'
import hashlib, json, os, sys
dist, version, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
files = {}
for folder, _, names in os.walk(dist):
    for name in names:
        path = os.path.relpath(os.path.join(folder, name), dist).replace(os.sep, '/')
        if path != 'bundle.json':
            with open(os.path.join(folder, name), 'rb') as f:
                files[path] = hashlib.sha256(f.read()).hexdigest()
with open(out, 'w', encoding='ascii') as f:
    f.write(json.dumps({'v': 1, 'version': version, 'editorApi': 1, 'files': dict(sorted(files.items()))}, separators=(',', ':')))
PY
  openssl pkeyutl -sign -inkey "$key" -rawin -in "$scratch/manifest" -out "$scratch/signature"
  python3 - "$scratch/manifest" "$scratch/signature" "$(key_id "$key")" "$dist/bundle.json" <<'PY'
import base64, json, sys
manifest, signature, key, out = sys.argv[1:5]
with open(manifest, 'rb') as m, open(signature, 'rb') as s, open(out, 'w', encoding='ascii') as f:
    f.write(json.dumps({'v': 1, 'key': key, 'manifest': base64.b64encode(m.read()).decode(), 'signature': base64.b64encode(s.read()).decode()}))
PY
  rm -rf "$scratch"
  echo "stage.sh: the Editor bundle in $dist is signed with $(key_id "$key") (version $version)" >&2
}

if [[ -n "$make_key" ]]; then
  [[ "$make_key" != default ]] || make_key="$release_key"
  if [[ -e "$make_key" ]]; then echo "stage.sh: $make_key exists: not replacing a release key" >&2; exit 2; fi
  case "$(realpath -m -- "$make_key")/" in "$root"/*) echo "stage.sh: the release key never goes in the repository" >&2; exit 2 ;; esac
  mkdir -p "$(dirname "$make_key")"
  (umask 077 && openssl genpkey -algorithm ed25519 -out "$make_key")
  chmod 600 "$make_key"
  echo "stage.sh: made the Editor's release key: $make_key (keep a copy somewhere safe; never in the repository)." >&2
  echo "Put this line in Desktop/src-tauri/src/editor/bundle.rs, RELEASE_KEYS, and release the client:" >&2
  echo "    (\"$(key_id "$make_key")\", \"$(key_public "$make_key")\"),"
  exit 0
fi
if [[ -n "$sign_only" ]]; then
  cd "$root"
  sign_bundle "$sign_only" "$release_key"
  exit $?
fi

[[ -n "$out" ]] || { echo "stage.sh: --out needs a directory" >&2; exit 2; }
case "$out" in
  /*) ;;
  *) out="$PWD/$out" ;;
esac
out="$(realpath -m -- "$out")"
if [[ "$out" == / || "$root/" == "$out"/* ]]; then
  echo "stage.sh: --out $out holds the repository; give a directory of its own" >&2; exit 2
fi
if [[ -e "$out" && ! -d "$out" ]]; then
  echo "stage.sh: --out $out is a file, not a directory" >&2; exit 2
fi
# Only a directory that is missing, empty, or a context staged here before is replaced (emptied below). (Contexts
# staged before .ratw-context was written hold BUILD_ID and Dockerfile.)
marker="$out/.ratw-context"
if [[ -d "$out" && -n "$(ls -A -- "$out")" && ! -f "$marker" && ! ( -f "$out/BUILD_ID" && -f "$out/Dockerfile" ) ]]
then
  echo "stage.sh: --out $out isn't empty and isn't a staged context (no .ratw-context): not replacing it" >&2
  exit 2
fi
# Everything that can be refused is checked before DIR is touched.
if [[ -n "$world" && ! -f "$world/package.json" ]]; then
  echo "stage.sh: $world has no package.json: not a world export" >&2; exit 2
fi
if ((editor)); then
  [[ -f "$release_key" ]] || { echo "stage.sh: no Editor release key at $release_key (--release-key, or --make-release-key to make one; --no-editor to stage without the Editor)" >&2; exit 2; }
  if [[ -n "$editor_dist" && ! -f "$editor_dist/index.html" ]]; then
    echo "stage.sh: $editor_dist has no index.html: not a built Editor" >&2; exit 2
  fi
  editor_dist="${editor_dist:+$(realpath -- "$editor_dist")}"
fi
if [[ -n "$build_id" && ! "$build_id" =~ ^[A-Za-z0-9._+-]+$ ]]; then
  echo "stage.sh: a build id is letters, digits and . _ + - only" >&2; exit 2
fi
world="${world:+$(realpath -- "$world")}"   # (Before the cd below.)

cd "$root"
# The server's Data directories (what Core reads: RATW_DATA_DIR); Portraits, Fonts, Cells, Characters and Worlds are
# the client's or the tools', not the server's.
data_dirs=(Economy Fame Gifts Items Lore Progression Social Storylines Talk Terrain Together Town Voice Wild)
src_paths=(CMakeLists.txt Core Server Tests Client/wasm/walk.cpp Data/Terrain/terrain.json)
app_paths=(Database/migrations Package/entrypoint.py Package/ratw-editor Package/breached-passwords.txt)
for d in "${data_dirs[@]}"; do app_paths+=("Data/$d"); done

listed() {   # The files git tracks or would add under the paths given (deleted ones left out), NUL-separated.
  git ls-files -z --cached --others --exclude-standard -- "$@" | while IFS= read -r -d '' f; do
    if [[ -f "$f" ]]; then printf '%s\0' "$f"; fi
  done
}

if [[ -z "$build_id" ]]; then
  build_id="$(git rev-parse --short=12 HEAD)"
  if [[ -n "$(git status --porcelain -- "${src_paths[@]}" "${app_paths[@]}" tools 2>/dev/null)" ]]; then
    build_id+="+dirty"
  fi
fi

# The marker first, then everything else goes: if this stops halfway (a failed copy, ^C), DIR is still a context.
mkdir -p "$out"
echo "A build context staged by Package/stage.sh, which replaces this directory whenever it runs." > "$marker"
find "$out" -mindepth 1 -maxdepth 1 ! -name .ratw-context -exec rm -rf -- {} +
mkdir -p "$out/src" "$out/app/tools" "$out/world"
listed "${src_paths[@]}" | xargs -0 cp --parents -t "$out/src"
listed "${app_paths[@]}" | xargs -0 cp --parents -t "$out/app"
# tools/*.py, not the tests (tools/worldgen and the other subdirectories are Atlas's, not a world's).
listed tools | while IFS= read -r -d '' f; do
  if [[ "$f" == tools/*.py && "$f" != tools/*/* && "$(basename "$f")" != test_* ]]; then printf '%s\0' "$f"; fi
done | xargs -0 cp -t "$out/app/tools"
cp Package/Dockerfile "$out/Dockerfile"
if ((editor)); then
  # The Editor, built from the working tree as it is now (or given), signed file by file; the page's portraits beside it
  # where the studio serves them (/portraits/), and in the list as portraits/.
  if [[ -z "$editor_dist" ]]; then
    [[ -d Editor/node_modules ]] || npm --prefix Editor ci --no-audit --no-fund >&2
    npm --prefix Editor run build >&2
    editor_dist="$root/Editor/dist"
  fi
  mkdir -p "$out/app/Editor/dist" "$out/app/Data/Portraits"
  cp -r "$editor_dist/." "$out/app/Editor/dist/"
  rm -f "$out/app/Editor/dist/bundle.json"
  listed Data/Portraits | while IFS= read -r -d '' f; do
    if [[ "$f" == *.png ]]; then printf '%s\0' "$f"; fi
  done | xargs -0 -r cp -t "$out/app/Data/Portraits"
  mkdir -p "$out/app/Editor/dist/portraits"
  cp "$out/app/Data/Portraits/"*.png "$out/app/Editor/dist/portraits/" 2>/dev/null || true
  sign_bundle "$out/app/Editor/dist" "$release_key"
fi
if [[ -n "$world" ]]; then
  cp -r "$world/." "$out/world/"
fi
echo "$build_id" > "$out/BUILD_ID"

size_kb="$(du -sk "$out" | cut -f1)"
echo "stage.sh: staged $out: $((size_kb / 1024)) MB ($(find "$out" -type f | wc -l) files), build $build_id" >&2
if ((size_kb > 400 * 1024)); then
  echo "stage.sh: the context is over 400 MB: something that isn't the image's was staged; not building" >&2
  exit 1
fi
if ((build)); then
  # (Docker's tags take no '+'.)
  docker build --build-arg "RATW_BUILD_ID=$build_id" -t "$tag:${build_id//+/-}" -t "$tag:latest" "$out"
fi
