-- Elevation and weather revamp (Docs/Design/22-elevation-weather.md): '%' is a paintable cliff face, and weather
-- gains overcast, storm and sandstorm. Heights stay sparse jsonb; the tools validate their half-tile steps.
-- The old checks were unnamed, so they are found by what they test rather than by name.
DO $$
DECLARE
    target record;
BEGIN
    FOR target IN
        SELECT c.conname, t.relname
        FROM pg_constraint c
        JOIN pg_class t ON t.oid = c.conrelid
        JOIN pg_namespace n ON n.oid = t.relnamespace
        WHERE n.nspname = 'world' AND c.contype = 'c' AND t.relname IN ('terrain_chunks', 'cells', 'interiors')
          AND (pg_get_constraintdef(c.oid) LIKE '%glyphs ~%' OR pg_get_constraintdef(c.oid) LIKE '%weather%')
    LOOP
        EXECUTE format('ALTER TABLE world.%I DROP CONSTRAINT %I', target.relname, target.conname);
    END LOOP;
END $$;

ALTER TABLE world.terrain_chunks
    ADD CONSTRAINT terrain_chunks_glyphs_check
        CHECK (char_length(glyphs) = size * size AND glyphs ~ '^[.#,"T=~:^+%]*$');

ALTER TABLE world.cells
    ADD CONSTRAINT cells_weather_check
        CHECK (weather IN ('clear', 'overcast', 'rain', 'storm', 'fog', 'snow', 'sandstorm'));

ALTER TABLE world.interiors
    ADD CONSTRAINT interiors_glyphs_check CHECK (glyphs ~ '^[.#,"T=~:^+%]*$'),
    ADD CONSTRAINT interiors_weather_check
        CHECK (weather IN ('clear', 'overcast', 'rain', 'storm', 'fog', 'snow', 'sandstorm'));
