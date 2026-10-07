-- Earned Gift tiers by account (Docs/Design/49-characters-and-earned-gifts.md, Phase 5): one row per account with the
-- characters it owns, when Gifted and Quickened opened for it and how ("earned", "dm:<name>"), the Dungeon Master's
-- hold, and its last measures (social level, scenes on Normal wolves, stars and their givers, closed Stories). Written
-- by the game server's checkpoints like the save's other lists (game.save_checkpoint_delta) and read back by
-- game.load_checkpoint. Unlike game.accounts it holds no password verifiers, so the tools and the Dungeon Master may
-- read it; their changes go through dm.actions (account.unlock).
CREATE TABLE game.account_standing (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    key         text NOT NULL,
    position    integer NOT NULL,
    data        jsonb NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, key)
);
COMMENT ON TABLE game.account_standing IS 'Earned Gift tiers by account (doc 49): characters, unlocks, hold, measures.';

INSERT INTO game.sections VALUES
    ('accountStanding', '{standing,accounts}', 'account_standing', $$e->>'account'$$);
