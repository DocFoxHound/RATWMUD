-- Circles (Docs/Design/50-player-card-friends-safety.md, Phase 5): out-of-character groups of accounts, as a table of
-- their own instead of inside the checkpoint row. Written by the game server's checkpoints like the save's other lists
-- (only changed rows, game.save_checkpoint_delta) and read back by game.load_checkpoint.
--
-- Each row is one circle: its name, members with their rank and whether they show it which wolf they're playing,
-- invitations waiting, and planned nights. Circle chat is never stored. The Dungeon Master may read the circles and
-- their members (the Players tab); the editor and publisher have no need to.
CREATE TABLE game.circles (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    key         text NOT NULL,
    position    integer NOT NULL,
    data        jsonb NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, key)
);
ALTER TABLE game.circles ADD COLUMN name text GENERATED ALWAYS AS (data->>'name') STORED;
COMMENT ON TABLE game.circles IS 'Circles (doc 50): out-of-character groups of accounts; members, ranks, nights. Never their chat.';

INSERT INTO game.sections VALUES ('circles', '{people,circles}', 'circles', $$e->>'id'$$);

REVOKE SELECT ON game.circles FROM ratw_editor, ratw_publisher;
