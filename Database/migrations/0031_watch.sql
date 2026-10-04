-- The LIVE map (Docs/Design/34-dungeon-master-refresh.md, 1.1): where everyone is, while a Dungeon Master watches.
--
-- The DM host keeps a row in dm.watchers for each account with the map open, renewed as the map asks for frames.
-- While any row is unexpired, the game server writes its newest frame of positions to dm.watch every couple of
-- seconds (Core/RatwWatch.h), on its own connection; with nobody watching it writes nothing. Both tables are
-- unlogged: they are only ever the latest moment, and are worth nothing after a database restart.
CREATE UNLOGGED TABLE dm.watchers (
    username    text PRIMARY KEY,
    until       timestamptz NOT NULL
);

CREATE UNLOGGED TABLE dm.watch (
    world_id    ratw_id PRIMARY KEY REFERENCES world.worlds ON DELETE CASCADE,
    frame       text NOT NULL,
    written_at  timestamptz NOT NULL DEFAULT now()
);
COMMENT ON TABLE dm.watch IS 'The latest positions of every player and NPC, written by the game server while a DM watches (LIVE map).';

GRANT SELECT, INSERT, UPDATE, DELETE ON dm.watchers, dm.watch TO ratw_dm, ratw_game;
