# Working on RATW

## The world we're working on

**Ridgemere 3** (since 2026-11-14): world key `ridgemere3`, database `ratw_ridgemere3`, Docs/Design/82. Look at, test
and change it by default. `dev` still names Ridgemere 2.0 (`ratw_ridgemere2`), which is no longer the active world.
The laptop's world on central (Saved/WorldAgent/run.sh, port 7788) runs Ridgemere 3; publishing a build restarts it.

## Running a game server to look at something

Several sessions work on this project at once. **To start a game server for yourself, use a scratch server**:

    bash tools/scratch.sh ridgemere3  # Ridgemere 3, the active world, read only
    bash tools/scratch.sh dev         # Ridgemere 2.0 (the previous world)
    bash tools/scratch.sh dev-old     # the old map's DEV, kept as an archive (PROD is still on that map)
    bash tools/scratch.sh town        # Greyfen Crossing

Run it in the background. It prints its port ("RATW server listening on port N"; open http://127.0.0.1:N/) and its
pid. It writes nothing to the world and runs beside the real DEV server and other sessions' scratch servers.
**Stop it when you're done** (`kill PID`); a forgotten one stops after an hour (`--for SECONDS` for longer). See
Docs/Design/20-world-database.md, "Scratch servers".

Don't start the real server (`tools/live.sh server ...`, `tools/server.sh --database ...`) unless the user asks: only
one may run per world, and a second is refused.
