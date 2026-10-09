-- Storylines and storytellers (Docs/Design/58-player-storytellers.md): tables of their own instead of inside the
-- checkpoint row, written by the game server's checkpoints like the save's other lists (only changed rows,
-- game.save_checkpoint_delta) and read back by game.load_checkpoint. Each step done, story begun or ended, prize and
-- star is also a game.events row ('storyline step', 'tale ended', 'story prize'...): the record the credits and the
-- chronicle read.
--
-- game.storylines: personal storylines, tales and world stories: kind, source, owner or storyteller, title and premise,
--   steps with their objectives (who did each, when), participants, cast, state.
-- game.storytellers: each account's standing: applied (with its note), approved, refused or revoked, by whom and why.
-- game.storyteller_log: what storytellers did (narration, story characters' lines, rolls, visitors, prizes, calls,
--   invitations, ticks): kept as evidence for the DM. The text of narration and story characters' lines is cleared
--   after 30 days (the user, 2026-10-08); the rest of the record after 180.
-- game.credits: credits screens still open to stars (a tale's end, a world story's milestone), for 3 days.
CREATE TABLE game.storylines (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    key         text NOT NULL,
    position    integer NOT NULL,
    data        jsonb NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, key)
);
ALTER TABLE game.storylines ADD COLUMN kind text GENERATED ALWAYS AS (data->>'kind') STORED,
                            ADD COLUMN state text GENERATED ALWAYS AS (data->>'state') STORED,
                            ADD COLUMN author text GENERATED ALWAYS AS (data->>'authorAccount') STORED;
COMMENT ON TABLE game.storylines IS 'Storylines (doc 58): personal storylines, tales and world stories, with their steps.';

CREATE TABLE game.storytellers (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    key         text NOT NULL,
    position    integer NOT NULL,
    data        jsonb NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, key)
);
ALTER TABLE game.storytellers ADD COLUMN state text GENERATED ALWAYS AS (data->>'state') STORED;
COMMENT ON TABLE game.storytellers IS 'Storytellers (doc 58): each account''s application and standing.';

CREATE TABLE game.storyteller_log (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    key         text NOT NULL,
    position    integer NOT NULL,
    data        jsonb NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, key)
);
ALTER TABLE game.storyteller_log ADD COLUMN account text GENERATED ALWAYS AS (data->>'account') STORED,
                                 ADD COLUMN storyline text GENERATED ALWAYS AS (data->>'storyline') STORED;
CREATE INDEX storyteller_log_account ON game.storyteller_log (world_id, account);
COMMENT ON TABLE game.storyteller_log IS 'What storytellers did (doc 58), as evidence for the DM; text cleared after 30 days.';

CREATE TABLE game.credits (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    key         text NOT NULL,
    position    integer NOT NULL,
    data        jsonb NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, key)
);
COMMENT ON TABLE game.credits IS 'Credits screens (doc 58) still open to stars: who did what at a tale''s end or a milestone.';

INSERT INTO game.sections VALUES ('storylines', '{people,storylines}', 'storylines', $$e->>'id'$$);
INSERT INTO game.sections VALUES ('storytellers', '{people,storytellers}', 'storytellers', $$e->>'account'$$);
INSERT INTO game.sections VALUES ('storyteller_log', '{people,storytellerLog}', 'storyteller_log', $$e->>'id'$$);
INSERT INTO game.sections VALUES ('credits', '{people,credits}', 'credits', $$e->>'id'$$);

-- Storytellers' words are for the DM only, never the world editors.
REVOKE SELECT ON game.storyteller_log FROM ratw_editor, ratw_publisher;

-- The DM's approved list of visitors a storyteller may bring on stage (doc 58, 6): written by the DM like
-- live.npc_areas, read by the game at start and on visitors.sync.
CREATE TABLE live.story_visitors (
    world_id     ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    id           ratw_id NOT NULL,
    name         text NOT NULL CHECK (char_length(name) BETWEEN 1 AND 60),
    description  text NOT NULL DEFAULT '' CHECK (char_length(description) <= 400),
    appearance   jsonb NOT NULL DEFAULT '{}' CHECK (jsonb_typeof(appearance) = 'object'),
    enabled      boolean NOT NULL DEFAULT true,
    approved_by  text,
    approved_at  timestamptz,
    updated_at   timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, id)
);
COMMENT ON TABLE live.story_visitors IS 'Visitors a storyteller may bring on stage (doc 58), from the DM''s approved list.';
