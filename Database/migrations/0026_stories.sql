-- NPC chronicles (Docs/Design/26-living-npcs.md, Phase 8). A chronicle is not stored: it is read from the event log
-- (game.events) whenever it is asked for. What is kept is the life story the Dungeon Master had written from one, so
-- each is paid for once, with the last event it knew of (a newer event means the story is behind).
CREATE TABLE dm.stories (
    world_id      ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    subject       text NOT NULL CHECK (char_length(subject) BETWEEN 1 AND 80),
    story         text NOT NULL CHECK (char_length(story) BETWEEN 1 AND 4000),
    through_event bigint NOT NULL DEFAULT 0,
    entries       integer NOT NULL DEFAULT 0,
    model         text NOT NULL DEFAULT '' CHECK (char_length(model) <= 80),
    written_by    text NOT NULL,
    written_at    timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, subject)
);
COMMENT ON TABLE dm.stories IS 'Life stories written from NPC and character chronicles for the Dungeon Master.';
