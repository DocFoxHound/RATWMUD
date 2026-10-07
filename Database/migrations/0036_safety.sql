-- Mute, block and report (Docs/Design/50-player-card-friends-safety.md, Phase 2).
--
-- game.safety_marks: each account's mutes (aimed at a character) and blocks (aimed at an account), written by the game
-- server's checkpoints like the save's other lists (game.save_checkpoint_delta) and read back by game.load_checkpoint.
-- The Dungeon Master sees counts, never who blocked whom.
--
-- game.reports: a player's report of another wolf with the lines the reporter received as evidence (the one exception
-- to doc 08's no-prose rule, agreed 2026-10-06). Not in the checkpoint: reports hold prose and outlive it. The game
-- server writes them; the Dungeon Master reads them and decides through dm.actions (report.decide). Open and dismissed
-- reports go after 30 days; upheld ones keep their record for good and their evidence 180 days (a daily purge).
CREATE TABLE game.safety_marks (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    key         text NOT NULL,
    position    integer NOT NULL,
    data        jsonb NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, key)
);
ALTER TABLE game.safety_marks ADD COLUMN kind text GENERATED ALWAYS AS (data->>'kind') STORED,
                              ADD COLUMN target text GENERATED ALWAYS AS (data->>'target') STORED;
COMMENT ON TABLE game.safety_marks IS 'Mutes and blocks by account (doc 50): holder, kind, target, the character pointed at.';

INSERT INTO game.sections VALUES
    ('safetyMarks', '{people,safety}', 'safety_marks', $$concat(e->>'holder', '|', e->>'kind', '|', e->>'target')$$);

CREATE TABLE game.reports (
    world_id            ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    id                  text NOT NULL CHECK (id ~ '^rep-[A-Za-z0-9-]{1,60}$'),
    created_at          timestamptz NOT NULL DEFAULT now(),
    reporter_account    text NOT NULL CHECK (char_length(reporter_account) BETWEEN 1 AND 80),
    reporter_character  text NOT NULL CHECK (char_length(reporter_character) BETWEEN 1 AND 80),
    reported_account    text NOT NULL CHECK (char_length(reported_account) BETWEEN 1 AND 80),
    reported_character  text NOT NULL CHECK (char_length(reported_character) BETWEEN 1 AND 80),
    kind                text NOT NULL CHECK (kind IN ('speech', 'profile', 'tell', 'circle')),
    category            text NOT NULL CHECK (category IN ('harassment', 'hateful', 'spam', 'cheating', 'other')),
    note                text NOT NULL DEFAULT '' CHECK (char_length(note) <= 600),
    evidence            jsonb NOT NULL DEFAULT '[]'::jsonb,
    status              text NOT NULL DEFAULT 'open' CHECK (status IN ('open', 'upheld', 'dismissed')),
    decided_by          text,
    decided_at          timestamptz,
    outcome             text CHECK (outcome IS NULL OR outcome IN ('', 'note', 'warning', 'silence')),
    silence_hours       integer NOT NULL DEFAULT 0 CHECK (silence_hours IN (0, 1, 6, 24, 72)),
    PRIMARY KEY (world_id, id)
);
CREATE INDEX reports_open ON game.reports (world_id, created_at) WHERE status = 'open';
CREATE INDEX reports_against ON game.reports (world_id, reported_account);
COMMENT ON TABLE game.reports IS 'Players'' reports of other wolves, with the lines received as evidence (doc 50); decided by a DM.';
