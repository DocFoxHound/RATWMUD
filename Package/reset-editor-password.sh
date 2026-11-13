#!/usr/bin/env bash
# A lost Editor password, or an account locked by too many tries (Docs/Design/78, section 3.6). In the world folder,
# on the world's computer, while the world runs:  ./reset-editor-password.sh [NAME]
# It lists your world's Editor accounts, asks which one, and gives it a new one-time password and Editor code (shown
# here, and written into manage/first-editor.txt too). The account's sessions end and its devices are forgotten; it
# chooses a new password at its next sign-in. The same, typed:  docker compose exec world ratw-editor reset NAME
# (reset-editor-password.bat is the same for Windows.)
set -u
cd "$(dirname "$0")" || exit 1

pause() {
    if [ -t 0 ]; then read -r -p "Press Enter to close." _; fi
}

if ! docker compose version </dev/null >/dev/null 2>&1; then
    echo "Docker isn't running here, or isn't installed: start it, then run reset-editor-password.sh again."
    pause
    exit 1
fi
if ! docker compose exec -T world true </dev/null >/dev/null 2>&1; then
    echo "Your world isn't running. Start it first (in this folder: docker compose up -d), wait a minute, then run"
    echo "reset-editor-password.sh again."
    pause
    exit 1
fi

name=${1:-}
if [ -z "$name" ]; then
    echo "Your world's Editor accounts:"
    echo
    docker compose exec -T world ratw-editor list </dev/null
    echo
    read -r -p "Which account? Type its name (or just press Enter to stop): " name
    if [ -z "$name" ]; then
        echo "Nothing was changed."
        pause
        exit 0
    fi
fi
if ! [[ "$name" =~ ^[a-z][a-z0-9_-]{2,31}$ ]]; then
    echo "That isn't an Editor account's name (3 to 32 small letters, digits, - and _). Nothing was changed."
    pause
    exit 1
fi

echo
if docker compose exec -T world ratw-editor reset "$name" --write-file </dev/null; then
    echo
    echo "Paste the Editor code into the game's EDITOR button (or use the name and one-time password); you choose a new"
    echo "password next. The code is also in manage/first-editor.txt. Keep it to yourself: it works once, for 72 hours."
    code=0
else
    code=1
fi
pause
exit "$code"
