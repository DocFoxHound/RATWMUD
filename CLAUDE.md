# Working on RATW

## Running a game server to look at something

Several sessions work on this project at once. **To start a game server for yourself, use a scratch server**:

    bash tools/scratch.sh dev      # DEV's world, read only
    bash tools/scratch.sh town     # Greyfen Crossing

Run it in the background. It prints its port ("RATW server listening on port N"; open http://127.0.0.1:N/) and its
pid. It writes nothing to the world and runs beside the real DEV server and other sessions' scratch servers.
**Stop it when you're done** (`kill PID`); a forgotten one stops after an hour (`--for SECONDS` for longer). See
Docs/Design/20-world-database.md, "Scratch servers".

Don't start the real server (`tools/live.sh server dev`, `tools/server.sh --database dev`) unless the user asks: only
one may run per world, and a second is refused.
