#!/usr/bin/env bash
# Puts your world back to one of its backups (Docs/Design/78, section 12.2). In the world folder:
#
#     ./restore.sh            lists the backups in backups/ and asks which one
#     ./restore.sh NAME       that one (My world's "Copy restore steps" gives the line)
#
# It says what you would lose and asks before anything changes. Then it stops the world (it saves first), makes a safety
# copy of the world as it is now, puts the backup back (keeping today's Editor accounts, unless you choose the backup's),
# ends every Editor session and starts the world again. The backup service's own `restore` refuses a running world, a
# backup from another world, a damaged one and one made by a newer package. (restore.bat is the same for Windows.)
set -u
cd "$(dirname "$0")" || exit 1
# (Without Compose's "Container ... Creating" lines between the questions: M6's run.)
export COMPOSE_PROGRESS=quiet

pause() {
    if [ -t 0 ]; then read -r -p "Press Enter to close." _; fi
}
again() {
    echo "Starting your world again..."
    docker compose up -d </dev/null >/dev/null 2>&1 || echo "It didn't start: type docker compose up -d in this folder."
}

if ! docker compose version </dev/null >/dev/null 2>&1; then
    echo "Docker isn't running here, or isn't installed: start it, then run restore.sh again."
    pause
    exit 1
fi
if [ ! -f compose.yaml ]; then
    echo "Run restore.sh from your world's folder (the one with compose.yaml)."
    pause
    exit 1
fi

echo "Stopping the backup service while you choose (it starts again at the end)..."
docker compose stop backup </dev/null >/dev/null 2>&1

name=${1:-}
if [ -z "$name" ]; then
    if ! docker compose run --rm -T backup list </dev/null; then
        again
        pause
        exit 1
    fi
    read -r -p "Which backup? Type its number (or just press Enter to stop): " number
    if [ -z "$number" ]; then
        echo "Nothing was changed."
        again
        pause
        exit 0
    fi
    case "$number" in
        *[!0-9]*) echo "That isn't a number. Nothing was changed."; again; pause; exit 1 ;;
    esac
    if ! name=$(docker compose run --rm -T backup name "$number" </dev/null); then
        echo "Nothing was changed."
        again
        pause
        exit 1
    fi
fi
case "$name" in
    *[!0-9A-Za-z-]*|"") echo "That isn't a backup's name. Nothing was changed."; again; pause; exit 1 ;;
esac

if ! docker compose run --rm -T backup show "$name" </dev/null; then
    echo "Nothing was changed."
    again
    pause
    exit 1
fi
read -r -p "Type RESTORE to go on (anything else stops): " sure
if [ "$sure" != "RESTORE" ]; then
    echo "Nothing was changed."
    again
    pause
    exit 0
fi

echo
echo "Keep your Editor accounts as they are now (recommended), or take the ones in the backup?"
echo "Keeping them means a co-builder you removed, an old password or a forgotten device doesn't come back."
read -r -p "Type K to keep them (or just press Enter), or B to take the backup's: " which
case "$which" in
    [bB]*) accounts=--take-accounts ;;
    *) accounts=--keep-accounts ;;
esac

echo
echo "Stopping your world (it saves first: this takes up to a minute and a half)..."
if ! docker compose stop world </dev/null; then
    echo "Your world didn't stop. Nothing was changed."
    again
    pause
    exit 1
fi
docker compose run --rm -T backup restore "$name" "$accounts" </dev/null
code=$?
again
if [ "$code" -eq 0 ]; then
    echo "Your world is back. Sign in to its Editor again (every Editor session ended)."
else
    echo "The restore didn't finish (see above)."
fi
pause
exit "$code"
