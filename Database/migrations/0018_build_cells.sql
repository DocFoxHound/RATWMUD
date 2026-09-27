-- Streamed builds: the game server loads a cell's tiles only while someone is in it or next to it, so a world of
-- any size fits (Docs/Design/20-world-database.md, "Massive worlds"). The build's manifest (world.builds.files,
-- "RATW_WORLD 3") names every cell with an "area" record; each cell is a row here, read by the server as needed.
CREATE TABLE world.build_cells (
    build_id  bigint NOT NULL REFERENCES world.builds ON DELETE CASCADE,
    cell_id   ratw_id NOT NULL,
    -- The cell file up to its "grid:" line (name, size, place, weather, light): read for every cell at start.
    header    text NOT NULL,
    -- The whole cell file, read when the cell loads.
    body      text NOT NULL,
    -- This cell's side of each open boundary to a neighbouring cell, as manifest "door" records.
    seams     text NOT NULL DEFAULT '',
    PRIMARY KEY (build_id, cell_id)
);
