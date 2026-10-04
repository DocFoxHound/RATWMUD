-- Server health (Core/RatwHealth.h): how the game server ran, kept for a fortnight, for the DM's Health tab and
-- tools/perf_report.py.
--
-- The game server writes a row each minute (kind 'window': tick times and where they went, the world's own parts,
-- traffic, players' pings as their pages report them, slow connections), and one for each tick that ran long (kind
-- 'spike': its parts). Each body is JSON. Written on the server's own connection, never by the game thread; rows
-- older than fourteen days are removed by the server as it goes.
CREATE TABLE dm.health (
    id          bigserial PRIMARY KEY,
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    kind        text NOT NULL CHECK (kind IN ('window', 'spike')),
    at          timestamptz NOT NULL DEFAULT now(),
    body        jsonb NOT NULL
);
CREATE INDEX health_by_time ON dm.health (world_id, at DESC);
COMMENT ON TABLE dm.health IS 'The game server''s health each minute and its slow ticks (Core/RatwHealth.h), kept 14 days.';

GRANT SELECT, INSERT, DELETE ON dm.health TO ratw_dm, ratw_game;
GRANT USAGE ON SEQUENCE dm.health_id_seq TO ratw_game;
