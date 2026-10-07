-- The star book (Docs/Design/51-scenes-and-stars.md, Phase 1), as tables of their own instead of inside the checkpoint
-- row. Written by the game server's checkpoints like the save's other lists (only changed rows,
-- game.save_checkpoint_delta) and read back by game.load_checkpoint.
--
-- game.star_tallies: each account's tally (stars counted toward its total, by kind and tag; the accounts that gave
-- them; the rate's chances and Gold Stars received). game.stars: the last 30 days' stars, each with giver and recipient
-- (account and character), its kind, tag, the social XP it paid, and whether it counted: what the counting rules read,
-- and what the Dungeon Master looks at. Older stars live on only in the tallies.
DO $$
DECLARE t text;
BEGIN
    FOREACH t IN ARRAY ARRAY['star_tallies', 'stars']
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

ALTER TABLE game.star_tallies ADD COLUMN total integer GENERATED ALWAYS AS ((data->>'total')::integer) STORED;
ALTER TABLE game.stars ADD COLUMN recipient_account text GENERATED ALWAYS AS (data->>'recipientAccount') STORED,
                       ADD COLUMN giver_account text GENERATED ALWAYS AS (data->>'giverAccount') STORED;

COMMENT ON TABLE game.star_tallies IS 'Each account''s stars (doc 51): counted total by kind and tag, distinct givers, the rate''s counts.';
COMMENT ON TABLE game.stars IS 'The last 30 days of stars (doc 51): giver and recipient, kind, tag, XP, whether it counted.';

INSERT INTO game.sections VALUES
    ('starTallies', '{people,starTallies}', 'star_tallies', $$e->>'account'$$),
    ('stars',       '{people,stars}',       'stars',        $$e->>'id'$$);

REVOKE SELECT ON game.star_tallies, game.stars FROM ratw_editor, ratw_publisher;
