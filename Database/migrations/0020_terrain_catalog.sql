-- Terrain is one catalog (Data/Terrain/terrain.json): each tile is stored as a one-character printable ASCII code
-- and drawn as a Unicode glyph. The database no longer lists the codes, so adding a tile needs no migration; the
-- tools and the game server check every code against the catalog.
ALTER TABLE world.terrain_chunks
    DROP CONSTRAINT terrain_chunks_glyphs_check,
    ADD CONSTRAINT terrain_chunks_glyphs_check CHECK (char_length(glyphs) = size * size AND glyphs ~ '^[!-~]*$');

ALTER TABLE world.interiors
    DROP CONSTRAINT interiors_glyphs_check,
    ADD CONSTRAINT interiors_glyphs_check CHECK (glyphs ~ '^[!-~]*$');
