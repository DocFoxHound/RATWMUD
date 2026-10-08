-- Ties (Docs/Design/52-newcomers.md, Phase 3): a story starter linking a new wolf to a mentor or a resident, as a table
-- of its own instead of inside the checkpoint row. Written by the game server's checkpoints like the save's other lists
-- (only changed rows, game.save_checkpoint_delta) and read back by game.load_checkpoint.
--
-- Each row is one tie: the newcomer's character and account, the other (a mentor's character or a resident), the
-- starter, its state (seeking, offered, active, lapsed, ended), the start town, whom it was offered to and until when,
-- the mentors asked, when it was made and lapses, the scenes shared at its start, and where the other was when it was
-- made (the marker). Ended and lapsed ties are let go after 30 days. The Dungeon Master reads them (the Players tab's
-- Ties list); the editor and publisher have no need to.
CREATE TABLE game.ties (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    key         text NOT NULL,
    position    integer NOT NULL,
    data        jsonb NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, key)
);
ALTER TABLE game.ties ADD COLUMN newcomer text GENERATED ALWAYS AS (data->>'newcomer') STORED,
                      ADD COLUMN state text GENERATED ALWAYS AS (data->>'state') STORED;
COMMENT ON TABLE game.ties IS 'Ties (doc 52): story starters linking new wolves to mentors or residents, and how each stands.';

INSERT INTO game.sections VALUES ('ties', '{people,ties}', 'ties', $$e->>'id'$$);

REVOKE SELECT ON game.ties FROM ratw_editor, ratw_publisher;
