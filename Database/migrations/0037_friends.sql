-- Friends and private messages (Docs/Design/50-player-card-friends-safety.md, Phase 3), as tables of their own instead
-- of inside the checkpoint row. Written by the game server's checkpoints like the save's other lists (only changed
-- rows, game.save_checkpoint_delta) and read back by game.load_checkpoint.
--
-- game.friendships: each account's side of each friendship (when it began, whether it shows the friend which character
-- it is playing); a friendship is two rows. game.friend_requests: requests waiting, gone after 14 days.
-- game.private_inbox: out-of-character messages kept for an offline friend until they next come (50 a recipient, each
-- gone after 14 days: the user, 2026-10-07). Their text is the player's own words, kept only until delivered; the
-- Dungeon Master reads it only inside a report, as received.
DO $$
DECLARE t text;
BEGIN
    FOREACH t IN ARRAY ARRAY['friendships', 'friend_requests', 'private_inbox']
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

ALTER TABLE game.friendships ADD COLUMN account text GENERATED ALWAYS AS (data->>'account') STORED,
                             ADD COLUMN friend text GENERATED ALWAYS AS (data->>'friend') STORED;
ALTER TABLE game.private_inbox ADD COLUMN recipient text GENERATED ALWAYS AS (data->>'to') STORED;

COMMENT ON TABLE game.friendships IS 'Friends (doc 50): each account''s side of each friendship, with its sharing switch.';
COMMENT ON TABLE game.friend_requests IS 'Friend requests waiting for an answer (doc 50); gone after 14 days.';
COMMENT ON TABLE game.private_inbox IS 'Private messages kept for an offline friend (doc 50): 50 a recipient, 14 days, until delivered.';

INSERT INTO game.sections VALUES
    ('friendships',    '{people,friends}',  'friendships',     $$concat(e->>'account', '|', e->>'friend')$$),
    ('friendRequests', '{people,requests}', 'friend_requests', $$concat(e->>'from', '|', e->>'to')$$),
    ('privateInbox',   '{people,inbox}',    'private_inbox',   $$e->>'id'$$);
