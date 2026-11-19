-- Older events have a known cell at most. Preserve that distinction from a recorded tile (0, 0).
ALTER TABLE game.events ADD COLUMN x integer, ADD COLUMN y integer,
    ADD CONSTRAINT events_tile_pair CHECK ((x IS NULL) = (y IS NULL) AND (x IS NULL OR (cell <> '' AND x >= 0 AND y >= 0)));

CREATE OR REPLACE FUNCTION game.record_events(p_world ratw_id, p_events jsonb) RETURNS integer
LANGUAGE sql AS $$
    WITH written AS (
        INSERT INTO game.events (world_id, game_time, game_day, kind, actor, target, cell, item, quantity, coins, detail, x, y)
        SELECT p_world, coalesce((e->>'time')::double precision, 0), coalesce((e->>'day')::double precision, 0),
               e->>'kind', coalesce(e->>'actor', ''), coalesce(e->>'target', ''), coalesce(e->>'cell', ''),
               coalesce(e->>'item', ''), coalesce((e->>'quantity')::integer, 0), coalesce((e->>'coins')::bigint, 0),
               left(coalesce(e->>'detail', ''), 400), (e->>'x')::integer, (e->>'y')::integer
        FROM jsonb_array_elements(p_events) e
        RETURNING 1)
    SELECT count(*)::integer FROM written
$$;
