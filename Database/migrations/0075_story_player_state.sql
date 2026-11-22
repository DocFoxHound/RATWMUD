-- 0075_story_player_state.sql: each wolf's own state of a world story (Docs/Design/85-story-triggers-and-campaign-
-- control.md, 3.6 and Phase 6, "Quests and a living audience").
--
-- A story with quests or an open audience ("open": nobody at first, wolves come in by taking a quest; "everyone": the
-- active wolves and each who signs in) keeps what belongs to one wolf out of its release's one JSON (dm.story_releases.
-- state): their variables of the `player` scope, their copies of the story's quests, their own ending, and a mark of what
-- their journal was last told. One row per wolf of the run, so a tick rewrites only the wolves it touched, and a run may
-- hold 5,000 wolves (it was 500). The story runner merges the rows in when it reads a run and takes them out again when it
-- writes it, in the same transaction; a run moved onto a new version (Phase 5b) carries them, and one the game refused
-- to move is put back with them. Other stories keep their whole state in the release as before.
--
-- Written by the story runner only. Closed to the game, the Editor's role and the publisher. Additive: nothing that
-- exists is changed. (0076 is Phase 8's; 0077 Phase 5's.)
CREATE TABLE dm.story_player_state (
    run        text NOT NULL REFERENCES dm.story_releases ON DELETE CASCADE,
    player     text NOT NULL CHECK (player ~ '^[A-Za-z0-9_][A-Za-z0-9_.:-]{0,79}$'),
    state      jsonb NOT NULL,
    updated_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (run, player)
);

REVOKE ALL ON dm.story_player_state FROM PUBLIC, ratw_game, ratw_editor, ratw_publisher;
GRANT SELECT, INSERT, UPDATE, DELETE ON dm.story_player_state TO ratw_dm;
