-- The world's event log (Docs/Design/26-living-npcs.md, Phase 2): what happened, who did it, to whom, where and
-- when, one row per event, never changed afterwards. Trades and every other economy ledger entry, deaths and
-- revivals, relocations, arrivals and departures, conversations with NPCs (that they happened, never what was said),
-- spawns and operator actions. NPC memory, rumour, chronicles and the Dungeon Master's audit read from here.
--
-- The game server writes events in the same transaction as its next checkpoint, through game.record_events.
CREATE TABLE game.events (
    id          bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    recorded_at timestamptz NOT NULL DEFAULT now(),
    game_time   double precision NOT NULL,            -- World seconds.
    game_day    double precision NOT NULL,            -- Calendar days.
    kind        text NOT NULL CHECK (char_length(kind) BETWEEN 1 AND 40),
    actor       text NOT NULL DEFAULT '' CHECK (char_length(actor) <= 80),
    target      text NOT NULL DEFAULT '' CHECK (char_length(target) <= 80),
    cell        text NOT NULL DEFAULT '' CHECK (char_length(cell) <= 80),
    item        text NOT NULL DEFAULT '' CHECK (char_length(item) <= 40),
    quantity    integer NOT NULL DEFAULT 0,
    coins       bigint NOT NULL DEFAULT 0,
    detail      text NOT NULL DEFAULT '' CHECK (char_length(detail) <= 400)
);
COMMENT ON TABLE game.events IS 'Append-only log of what happened in the world (game server writes; tools read).';

-- Everything about someone (as actor or target), everything of a kind, and everything in a place, newest first.
CREATE INDEX events_actor ON game.events (world_id, actor, id) WHERE actor <> '';
CREATE INDEX events_target ON game.events (world_id, target, id) WHERE target <> '';
CREATE INDEX events_kind ON game.events (world_id, kind, id);
CREATE INDEX events_cell ON game.events (world_id, cell, id) WHERE cell <> '';

-- Appends a JSON array of events, each {"kind", "actor", "target", "cell", "time", "day", "item", "quantity",
-- "coins", "detail"} (missing text fields are empty, missing numbers zero). Returns how many were written.
CREATE FUNCTION game.record_events(p_world ratw_id, p_events jsonb) RETURNS integer
LANGUAGE sql AS $$
    WITH written AS (
        INSERT INTO game.events (world_id, game_time, game_day, kind, actor, target, cell, item, quantity, coins, detail)
        SELECT p_world, coalesce((e->>'time')::double precision, 0), coalesce((e->>'day')::double precision, 0),
               e->>'kind', coalesce(e->>'actor', ''), coalesce(e->>'target', ''), coalesce(e->>'cell', ''),
               coalesce(e->>'item', ''), coalesce((e->>'quantity')::integer, 0), coalesce((e->>'coins')::bigint, 0),
               left(coalesce(e->>'detail', ''), 400)
        FROM jsonb_array_elements(p_events) e
        RETURNING 1)
    SELECT count(*)::integer FROM written
$$;
REVOKE ALL ON FUNCTION game.record_events(ratw_id, jsonb) FROM PUBLIC;
GRANT EXECUTE ON FUNCTION game.record_events(ratw_id, jsonb) TO ratw_game;

-- The log is written once: the game server may add to it, never change or remove what is there.
REVOKE UPDATE, DELETE ON game.events FROM ratw_game;
