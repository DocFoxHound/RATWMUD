-- The game server loads the world from the database (design step 6).
--
-- A build is the world compiled for the game: the manifest and cell files the
-- runtime's validated loader reads, produced by the one exporter
-- (tools/map_editor.py) with seams, door arrivals and residents resolved.
-- PROD gets one with every release; DEV gets one on demand for local servers.
-- The server loads the newest build of the one world.
CREATE TABLE world.builds (
    id          bigserial PRIMARY KEY,
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    release     integer,
    created_at  timestamptz NOT NULL DEFAULT now(),
    created_by  text NOT NULL DEFAULT '' CHECK (char_length(created_by) <= 60),
    -- {"world.ratw": "...", "cells/town.cell": "...", ...}
    files       jsonb NOT NULL CHECK (jsonb_typeof(files) = 'object' AND files ? 'world.ratw')
);
CREATE INDEX builds_latest ON world.builds (world_id, id DESC);

-- The server's save is kept byte for byte as it wrote it (jsonb would reorder
-- keys and cannot hold every string the game may carry). Step 8 splits it into
-- real tables.
ALTER TABLE game.checkpoints ALTER COLUMN payload TYPE text USING payload::text;
