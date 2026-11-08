#!/usr/bin/env bash
# Stages the world image's build context (Docs/Design/77, section 8, and Package/README.md), and optionally builds it.
#
#   bash Package/stage.sh [--world EXPORT_DIR] [--out DIR] [--build-id ID] [--build] [--tag TAG]
#
# The repository is about 90 GB (artifacts/, .claude/worktrees/, Saved/), so the image is never built from its root:
# this copies exactly what the image needs into DIR (default build-package/context, under /build*/ and so ignored by
# git), checks the size, and with --build runs `docker build` there. Only files git knows about, or would add (tracked
# or untracked but not ignored), are copied, from the working tree as it is now.
#   DIR/Dockerfile                 Package/Dockerfile
#   DIR/src/                       what the server is built from: CMakeLists.txt, Core/, Server/, Tests/ (CMake
#                                  configures every target, even when only ratw_server is built), Client/wasm/walk.cpp
#                                  and Data/Terrain/terrain.json (the client-data hash)
#   DIR/app/                       what runs: Data/ (the server's directories only), Database/migrations/, tools/*.py
#                                  (not the tests), the Package's entrypoint, and Client/dist as app/web (a LAN world's
#                                  page; see README.md)
#   DIR/world/                     the world export (tools/world_package.py export) given with --world; empty without
#                                  one, and the container then starts only with RATW_PACKAGE_IMPORT=skip
# The build id is the commit (12 characters), with +dirty if any staged file differs from it; --build-id overrides it.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$root/build-package/context" world="" build_id="" build=0 tag="ratw-world"
while (($#)); do
  case "$1" in
    --world) world="$2"; shift 2 ;;
    --out) out="$2"; shift 2 ;;
    --build-id) build_id="$2"; shift 2 ;;
    --build) build=1; shift ;;
    --tag) tag="$2"; shift 2 ;;
    -h|--help) sed -n '2,22p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) echo "stage.sh: unknown argument $1 (see --help)" >&2; exit 2 ;;
  esac
done
case "$out" in
  /*) ;;
  *) out="$PWD/$out" ;;
esac
case "$out" in
  "$root"|"$root"/|/) echo "stage.sh: --out must be a directory of its own, not the repository" >&2; exit 2 ;;
esac

cd "$root"
# The server's Data directories (what Core reads: RATW_DATA_DIR); Portraits, Fonts, Cells, Characters and Worlds are
# the client's or the tools', not the server's.
data_dirs=(Economy Fame Gifts Items Lore Progression Social Storylines Talk Terrain Together Town Voice Wild)
src_paths=(CMakeLists.txt Core Server Tests Client/wasm/walk.cpp Data/Terrain/terrain.json)
app_paths=(Database/migrations Package/entrypoint.py)
for d in "${data_dirs[@]}"; do app_paths+=("Data/$d"); done

listed() {   # The files git tracks or would add under the paths given (deleted ones left out), NUL-separated.
  git ls-files -z --cached --others --exclude-standard -- "$@" | while IFS= read -r -d '' f; do
    if [[ -f "$f" ]]; then printf '%s\0' "$f"; fi
  done
}

rm -rf "$out"
mkdir -p "$out/src" "$out/app/tools" "$out/world"
listed "${src_paths[@]}" | xargs -0 cp --parents -t "$out/src"
listed "${app_paths[@]}" | xargs -0 cp --parents -t "$out/app"
# tools/*.py, not the tests (tools/worldgen and the other subdirectories are Atlas's, not a world's).
listed tools | while IFS= read -r -d '' f; do
  if [[ "$f" == tools/*.py && "$f" != tools/*/* && "$(basename "$f")" != test_* ]]; then printf '%s\0' "$f"; fi
done | xargs -0 cp -t "$out/app/tools"
if [[ -f Client/dist/index.html ]]; then
  cp -r Client/dist "$out/app/web"
else
  echo "stage.sh: no Client/dist (npm run build in Client/): a LAN world in this image has no page" >&2
fi
cp Package/Dockerfile "$out/Dockerfile"
if [[ -n "$world" ]]; then
  [[ -f "$world/package.json" ]] || { echo "stage.sh: $world has no package.json: not a world export" >&2; exit 2; }
  cp -r "$world/." "$out/world/"
fi

if [[ -z "$build_id" ]]; then
  build_id="$(git rev-parse --short=12 HEAD)"
  if [[ -n "$(git status --porcelain -- "${src_paths[@]}" "${app_paths[@]}" tools Client/dist 2>/dev/null)" ]]; then
    build_id+="+dirty"
  fi
fi
[[ "$build_id" =~ ^[A-Za-z0-9._+-]+$ ]] || { echo "stage.sh: a build id is letters, digits and . _ + - only" >&2; exit 2; }
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
