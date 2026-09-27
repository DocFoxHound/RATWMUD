-- How everyone regards everyone else (Docs/Design/26-living-npcs.md, Phase 3; Source/RATWMUD/Core/RatwBonds.h):
-- one row per one-sided bond, "holder|other", with affinity, trust, familiarity, fear, respect, what is owed and the
-- day of last contact. Written by the game server's checkpoints like the save's other lists (only changed rows, with
-- game.save_checkpoint_delta); readable by the tools and the Dungeon Master.
CREATE TABLE game.bonds (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    key         text NOT NULL,
    position    integer NOT NULL,
    data        jsonb NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    holder      text GENERATED ALWAYS AS (data->>'holder') STORED,
    other       text GENERATED ALWAYS AS (data->>'other') STORED,
    PRIMARY KEY (world_id, key)
);
COMMENT ON TABLE game.bonds IS 'How each character regards another (one row per holder and other).';
CREATE INDEX bonds_other ON game.bonds (world_id, other);

INSERT INTO game.sections VALUES ('bonds', '{bonds}', 'bonds', $$concat(e->>'holder', '|', e->>'other')$$);
