-- Known wolves and scene recaps (Docs/Design/50-player-card-friends-safety.md, Phase 4), as tables of their own
-- instead of inside the checkpoint row. Written by the game server's checkpoints like the save's other lists (only
-- changed rows, game.save_checkpoint_delta) and read back by game.load_checkpoint.
--
-- game.known_wolves: each character's entry on a wolf it has met (when first and last, where, scenes shared, its tag and
-- private note, the profile revision it last read). game.scene_recaps: recaps of scenes, written from what one
-- character perceived (narrative storage, like residents' conversation memories: doc 08), at most 3 for each wolf and
-- 150 a character.
--
-- Both, and game.private_inbox (migration 0037), are for the game server alone: the Dungeon Master, the editor and
-- the publisher don't read players' notes, recaps or private messages. (A report carries what the DM needs.)
DO $$
DECLARE t text;
BEGIN
    FOREACH t IN ARRAY ARRAY['known_wolves', 'scene_recaps']
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

ALTER TABLE game.known_wolves ADD COLUMN owner text GENERATED ALWAYS AS (data->>'owner') STORED;
ALTER TABLE game.scene_recaps ADD COLUMN owner text GENERATED ALWAYS AS (data->>'owner') STORED;

COMMENT ON TABLE game.known_wolves IS 'Known wolves (doc 50): each character''s entry on a wolf it met; tag, note, scenes. Game only.';
COMMENT ON TABLE game.scene_recaps IS 'Scene recaps (doc 50): what one character perceived of a scene, in a few sentences. Game only.';

INSERT INTO game.sections VALUES
    ('knownWolves', '{people,known}',  'known_wolves', $$concat(e->>'owner', '|', e->>'other')$$),
    ('sceneRecaps', '{people,recaps}', 'scene_recaps', $$e->>'id'$$);

REVOKE SELECT ON game.known_wolves, game.scene_recaps, game.private_inbox FROM ratw_dm, ratw_editor, ratw_publisher;
