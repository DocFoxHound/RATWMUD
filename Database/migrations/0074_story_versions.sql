-- 0074_story_versions.sql: moving a running campaign onto a newly approved version of its story
-- (Docs/Design/85-story-triggers-and-campaign-control.md, Phase 5b; the user's answer to question 5, 2026-11-21: yes).
-- Additive: no row is rewritten, nothing that exists is changed.
--
-- One row per version a run has been on, and per version approved for it and not yet moved onto. The run's release row
-- (dm.story_releases) keeps the current one: its program, digest and approver change when a move is taken. A move is a
-- command in the Phase 2 inbox (`migrate`), taken between two world events; the game is told the new version's words and
-- regions for the visit the run is in (`story.phase` with `revise`), and if it refuses, the run goes back to the version
-- it was on (`state_before`). Nothing the world was given is taken back by either.
--
--   approved  approved as a new version of the run, not moved onto yet (a newer approval of it replaces it: withdrawn)
--   moving    the move was taken; the game has not answered its order (`receipt`) yet
--   current   the version the run is on, once moved (before any move the run has no rows: its release says it all)
--   past      a version the run was on and has left (the one it was published on gets its row at the first move)
--   refused   the game refused the move: the run went back (`problem` says why)
--   withdrawn an approval replaced by a newer one, or by the run moving onto another version first
CREATE TABLE dm.story_versions (
    id           bigserial PRIMARY KEY,
    run          text NOT NULL REFERENCES dm.story_releases ON DELETE CASCADE,
    digest       text NOT NULL,
    program      jsonb NOT NULL,
    approved_by  text NOT NULL,
    approved_at  timestamptz NOT NULL DEFAULT now(),
    status       text NOT NULL DEFAULT 'approved'
                 CHECK (status IN ('approved', 'moving', 'current', 'past', 'refused', 'withdrawn')),
    -- The move: from which version, by whom, by which command, where in the run (visit, the event it landed after, the
    -- game day), and the migration report (the diff, and what was kept, reset, defaulted and dropped).
    from_digest  text,
    moved_at     timestamptz,
    moved_by     text,
    command      bigint REFERENCES dm.story_commands ON DELETE SET NULL,
    visit        integer,
    at_event     bigint,
    game_day     double precision,
    report       jsonb,
    -- What the run held before the move, kept until the game has answered (a refusal puts it back).
    state_before jsonb,
    receipt      bigint,
    settled_at   timestamptz,
    problem      text NOT NULL DEFAULT ''
);
CREATE INDEX story_versions_run ON dm.story_versions (run, id DESC);
CREATE UNIQUE INDEX story_versions_one_current ON dm.story_versions (run) WHERE status IN ('current', 'moving');

REVOKE ALL ON dm.story_versions FROM PUBLIC, ratw_game, ratw_editor, ratw_publisher;
GRANT SELECT, INSERT, UPDATE ON dm.story_versions TO ratw_dm;
GRANT USAGE ON SEQUENCE dm.story_versions_id_seq TO ratw_dm;
