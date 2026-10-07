-- People (Docs/Design/50-player-card-friends-safety.md, Phase 1): each account as a person (its public handle,
-- experience, played time, first character and settings) and each character's roleplay profile, as tables of their own
-- instead of inside the checkpoint row. Written by the game server's checkpoints like the save's other lists (only
-- changed rows, game.save_checkpoint_delta) and read back by game.load_checkpoint. Neither holds a password verifier, so
-- the tools and the Dungeon Master may read them (game.accounts stays the game server's alone).
DO $$
DECLARE t text;
BEGIN
    FOREACH t IN ARRAY ARRAY['account_profiles', 'profiles']
    LOOP
        EXECUTE format($f$
            CREATE TABLE game.%I (
                world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
                key         text NOT NULL,
                position    integer NOT NULL,
                data        jsonb NOT NULL,
                updated_at  timestamptz NOT NULL DEFAULT now(),
                PRIMARY KEY (world_id, key)
            )$f$, t);
    END LOOP;
END $$;

ALTER TABLE game.account_profiles ADD COLUMN handle text GENERATED ALWAYS AS (data->>'handle') STORED;
ALTER TABLE game.profiles ADD COLUMN status text GENERATED ALWAYS AS (coalesce(data->>'status', 'ic')) STORED;
CREATE INDEX account_profiles_handle ON game.account_profiles (world_id, lower(handle));

COMMENT ON TABLE game.account_profiles IS 'Accounts as people (doc 50): handle, experience, played time, first character, settings.';
COMMENT ON TABLE game.profiles IS 'Characters'' roleplay profiles on the player card (doc 50): description, glances, status, OOC parts.';

INSERT INTO game.sections VALUES
    ('accountProfiles', '{people,accounts}', 'account_profiles', $$e->>'account'$$),
    ('profiles',        '{people,profiles}', 'profiles',         $$e->>'character'$$);
