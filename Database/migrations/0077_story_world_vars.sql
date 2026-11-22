-- 0077_story_world_vars.sql: the `world` scope of a version 2 story's variables (Docs/Design/85-story-triggers-and-
-- campaign-control.md, 3.2 and Phase 5): a value kept for the world after a run ends and shared by every story of the
-- world that declares a variable of that name and type (the game cache of a map editor). Written by the story runner
-- only, in the transaction of the tick or console command that changed it; read when a run is ticked, explained or
-- steered. A value kept under another type is not read by a story declaring the name as something else.
--
-- (Numbers: Phase 5b took 0074 and Phases 6 and 8 take 0075 and 0076; this one is Phase 5's, numbered after them.)
-- Additive: nothing that exists is changed.
CREATE TABLE dm.story_world_vars (
    world_id   ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    name       text NOT NULL CHECK (name ~ '^[A-Za-z0-9_][A-Za-z0-9_.:-]{0,79}$'),
    type       text NOT NULL,
    value      jsonb NOT NULL,
    run        text,
    updated_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, name)
);

REVOKE ALL ON dm.story_world_vars FROM PUBLIC, ratw_game, ratw_editor, ratw_publisher;
GRANT SELECT, INSERT, UPDATE, DELETE ON dm.story_world_vars TO ratw_dm;
