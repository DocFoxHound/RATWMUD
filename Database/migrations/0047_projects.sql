-- Town projects (Docs/Design/57-changing-the-world.md, 4): public works a town needs, as a table of their own instead of
-- inside the checkpoint row. Written by the game server's checkpoints like the save's other lists (only changed rows,
-- game.save_checkpoint_delta) and read back by game.load_checkpoint.
--
-- A project row: its kind, town and place, its title and state (open, built, worn, ruin, cancelled, removed), what it
-- needs and the work done, the structure it stands as on the Chapters' layer (game.camp_structures), who it is named
-- for, and its gifts (who gave coin, goods or labour, how much, and its worth), with the name each giver chose to be
-- shown by on its plaque. Ids and amounts only, no prose. Its coin and goods are the society's account "project:<id>".
-- Each gift and each finish is also a game.events row ('project gift', 'project built').
CREATE TABLE game.projects (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    key         text NOT NULL,
    position    integer NOT NULL,
    data        jsonb NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, key)
);
ALTER TABLE game.projects ADD COLUMN kind text GENERATED ALWAYS AS (data->>'kind') STORED,
                          ADD COLUMN town text GENERATED ALWAYS AS (data->>'town') STORED,
                          ADD COLUMN state text GENERATED ALWAYS AS (data->>'state') STORED;
CREATE INDEX projects_town ON game.projects (world_id, town);
COMMENT ON TABLE game.projects IS 'Town projects (doc 57): public works, their needs, progress, gifts and plaques.';

INSERT INTO game.sections VALUES ('projects', '{people,projects}', 'projects', $$e->>'id'$$);

-- The Dungeon Master's LIVE map reads them (the Projects layer).
GRANT SELECT ON game.projects TO ratw_editor;
