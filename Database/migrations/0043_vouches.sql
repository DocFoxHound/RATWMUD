-- Vouches (Docs/Design/52-newcomers.md, Phase 5): one wolf's word for another to a resident, as a table of its own
-- instead of inside the checkpoint row. Written by the game server's checkpoints like the save's other lists (only
-- changed rows, game.save_checkpoint_delta) and read back by game.load_checkpoint.
--
-- Each row is one vouch: the resident, the voucher and the one vouched for; when, and until which calendar day; what
-- it gave the resident and each of its household (taken back if it goes bad); active, ended or broken. IDs and amounts
-- only, never prose. The Dungeon Master may read them; the editor and publisher have no need to.
CREATE TABLE game.vouches (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    key         text NOT NULL,
    position    integer NOT NULL,
    data        jsonb NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, key)
);
ALTER TABLE game.vouches ADD COLUMN voucher text GENERATED ALWAYS AS (data->>'voucher') STORED,
                         ADD COLUMN state text GENERATED ALWAYS AS (data->>'state') STORED;
COMMENT ON TABLE game.vouches IS 'Vouches (doc 52): one wolf''s word for another to a resident, the trust it carried, and how it stands.';

INSERT INTO game.sections VALUES ('vouches', '{people,vouches}', 'vouches', $$e->>'id'$$);

REVOKE SELECT ON game.vouches FROM ratw_editor, ratw_publisher;
