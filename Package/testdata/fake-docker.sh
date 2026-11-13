#!/usr/bin/env bash
# A stand-in for `docker` (Package/test_package.py's ScriptsTests): logs each call to $FAKE_LOG and answers the few the
# host's scripts make, as the real backup service and ratw-editor would, and reads all of stdin as docker compose
# does (a script must give it none, or its answers are gone). FAKE_REFUSE=1: the restore is refused.
echo "$*" >> "$FAKE_LOG"
cat > /dev/null      # (As docker compose run and exec do: they read stdin. The scripts give them none.)
[ "$1" = compose ] || exit 1
shift
case "$*" in
    version) echo "Docker Compose version v5.1.1" ;;
    "stop backup"|"stop world"|"up -d") ;;
    "run --rm -T backup list") printf 'Your world'"'"'s backups, newest first:\n\n    1  2026-11-12 14:03 UTC  (3 hours ago)  by hand\n' ;;
    "run --rm -T backup name 1") echo "ratw-20261112T140300Z-by-hand" ;;
    "run --rm -T backup name "*) echo "There is no backup numbered ${*##* }." >&2; exit 2 ;;
    "run --rm -T backup show ratw-"*) echo "This puts your world back to 2026-11-12 14:03 UTC. Everything players did since then is lost." ;;
    "run --rm -T backup restore ratw-"*) if [ -n "${FAKE_REFUSE:-}" ]; then echo "Your world is running" >&2; exit 2; fi; echo "Done" ;;
    "exec -T world true") ;;
    "exec -T world ratw-editor list") echo "host-k7q2   owner   on   devices: 1" ;;
    "exec -T world ratw-editor reset "*" --write-file") printf 'Editor account: %s\nratw-editor:abc\n' "$6" ;;
    *) echo "fake docker: unexpected: $*" >&2; exit 1 ;;
esac
