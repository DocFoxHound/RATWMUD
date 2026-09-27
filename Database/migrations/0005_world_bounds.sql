-- Terrain block size is a per-world setting instead of a fixed 32, and each
-- world records its bounds (the area the editor shows, in world tiles).

ALTER TABLE world.worlds
    ADD COLUMN chunk_size integer NOT NULL DEFAULT 128 CHECK (chunk_size BETWEEN 8 AND 1024),
    ADD COLUMN min_x integer NOT NULL DEFAULT 0,
    ADD COLUMN min_y integer NOT NULL DEFAULT 0,
    ADD COLUMN width integer NOT NULL DEFAULT 0 CHECK (width >= 0),
    ADD COLUMN height integer NOT NULL DEFAULT 0 CHECK (height >= 0),
    ADD UNIQUE (id, chunk_size);

-- Each chunk records its size, tied to its world's, so a world's chunks always
-- match its setting. Changing chunk_size means re-cutting the terrain.
ALTER TABLE world.terrain_chunks
    DROP CONSTRAINT terrain_chunks_glyphs_check,
    ADD COLUMN size integer NOT NULL,
    ADD FOREIGN KEY (world_id, size) REFERENCES world.worlds (id, chunk_size) ON UPDATE CASCADE ON DELETE CASCADE,
    ADD CHECK (char_length(glyphs) = size * size AND glyphs ~ '^[.#,"T=~:^+]*$');

-- A roster character's job: {"world": ..., "slot": ...}, or null when free.
ALTER TABLE live.characters ADD COLUMN assignment jsonb CHECK (assignment IS NULL OR jsonb_typeof(assignment) = 'object');
