-- 0081_story_realm_vars.sql: the `realm` scope of a version 2 story's variables (Docs/Design/86-quests-and-the-ninth-howl.md,
-- engine spec J2 and "Phase 8 as built"): each wolf's own value, kept after a run ends and shared by every story of the
-- world that declares a variable of that name and type (Act I sets it, Act II reads it). Written by the story runner only,
-- in the transaction of the tick or console command that changed it (only the wolf-name pairs that changed); read at a
-- run's launch for its wolves, then only the rows changed since its cursor (updated_at), and for a wolf when they join.
-- A value kept under another type is not read, and approval refuses a story declaring a kept name as another type.
-- (0080, dm.story_posts, is the same phase's.) Additive: nothing that exists is changed.
CREATE TABLE dm.story_realm_vars (
    world_id   ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    name       text NOT NULL CHECK (name ~ '^[A-Za-z0-9_][A-Za-z0-9_.:-]{0,79}$'),
    wolf       text NOT NULL,
    type       text NOT NULL,
    value      jsonb NOT NULL,
    run        text,
    updated_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    PRIMARY KEY (world_id, name, wolf)
);
CREATE INDEX story_realm_vars_updated ON dm.story_realm_vars (world_id, updated_at);

REVOKE ALL ON dm.story_realm_vars FROM PUBLIC, ratw_game, ratw_editor, ratw_publisher;
GRANT SELECT, INSERT, UPDATE, DELETE ON dm.story_realm_vars TO ratw_dm;
