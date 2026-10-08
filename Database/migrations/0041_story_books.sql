-- Story books (Docs/Design/51-scenes-and-stars.md, Phase 7): player-made Stories of linked scenes, as a table of their
-- own instead of inside the checkpoint row. Written by the game server's checkpoints like the save's other lists (only
-- changed rows, game.save_checkpoint_delta) and read back by game.load_checkpoint.
--
-- Each row is one book: its title, keeper, wolves, who it is shared with, a world storyline it may be tied to (the
-- DM's), its chapters (each keeping its scene's place, time, wolves, title and summary: scenes are let go after eight
-- days), whether it is finished, its flavour text, its volume links, and an official book's Story. The Dungeon Master
-- may read them; the editor and publisher have no need to.
CREATE TABLE game.story_books (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    key         text NOT NULL,
    position    integer NOT NULL,
    data        jsonb NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, key)
);
ALTER TABLE game.story_books ADD COLUMN title text GENERATED ALWAYS AS (data->>'title') STORED,
                             ADD COLUMN state text GENERATED ALWAYS AS (data->>'state') STORED;
COMMENT ON TABLE game.story_books IS 'Story books (doc 51): player-made Stories of linked scenes, their chapters, sharing and volumes.';

INSERT INTO game.sections VALUES ('storyBooks', '{people,books}', 'story_books', $$e->>'id'$$);

REVOKE SELECT ON game.story_books FROM ratw_editor, ratw_publisher;
