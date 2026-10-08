-- Fame (Docs/Design/56-fame-and-memory.md): good deeds and the nicknames residents coin from them, as tables of their
-- own instead of inside the checkpoint row. Written by the game server's checkpoints like the save's other lists (only
-- changed rows, game.save_checkpoint_delta) and read back by game.load_checkpoint.
--
-- A deed row: its kind and weight, its doers, who it was done for, where and when, its witnesses with the name each knew
-- the doers by, the doers' looks then, the names it travels under, the towns whose word holds it, and whether the
-- Dungeon Master revoked it. Each deed is also a game.events row (kind 'deed'). A nickname row (Phase 3): the wolf, the
-- name, the deed it came from, who coined it, and whether the wolf asked folk not to use it.
CREATE TABLE game.deeds (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    key         text NOT NULL,
    position    integer NOT NULL,
    data        jsonb NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, key)
);
ALTER TABLE game.deeds ADD COLUMN kind text GENERATED ALWAYS AS (data->>'kind') STORED,
                       ADD COLUMN town text GENERATED ALWAYS AS (data->>'town') STORED;
CREATE INDEX deeds_doers ON game.deeds USING gin ((data->'doers'));
COMMENT ON TABLE game.deeds IS 'Deeds (doc 56): good deeds, their witnesses and how far word of them has travelled.';

CREATE TABLE game.nicknames (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    key         text NOT NULL,
    position    integer NOT NULL,
    data        jsonb NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, key)
);
ALTER TABLE game.nicknames ADD COLUMN wolf text GENERATED ALWAYS AS (data->>'wolf') STORED;
COMMENT ON TABLE game.nicknames IS 'Nicknames (doc 56): what residents call a wolf for its deeds, and who coined it.';

INSERT INTO game.sections VALUES ('deeds', '{people,deeds}', 'deeds', $$e->>'id'$$);
INSERT INTO game.sections VALUES ('nicknames', '{people,nicknames}', 'nicknames', $$e->>'id'$$);

REVOKE SELECT ON game.deeds FROM ratw_editor, ratw_publisher;

-- The chronicle (doc 56, 8): the game server reads a character's own rows of the event log, on request.
GRANT SELECT ON game.events TO ratw_game;
REVOKE SELECT ON game.nicknames FROM ratw_editor, ratw_publisher;
