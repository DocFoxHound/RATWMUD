-- Rumours (Docs/Design/26-living-npcs.md, Phase 5; Source/RATWMUD/Core/RatwRoads.h): what each character has heard,
-- one row per holder, subject and claim ("holder|subject|claim"), with who told them and how sure they are. Written by
-- the game server's checkpoints like the save's other lists (only changed rows); readable by the tools.
CREATE TABLE game.beliefs (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    key         text NOT NULL,
    position    integer NOT NULL,
    data        jsonb NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    holder      text GENERATED ALWAYS AS (data->>'holder') STORED,
    subject     text GENERATED ALWAYS AS (data->>'subject') STORED,
    PRIMARY KEY (world_id, key)
);
COMMENT ON TABLE game.beliefs IS 'What each character has heard about another (rumours), and from whom.';
CREATE INDEX beliefs_subject ON game.beliefs (world_id, subject);

INSERT INTO game.sections VALUES ('beliefs', '{beliefs}', 'beliefs', $$concat(e->>'holder', '|', e->>'subject', '|', e->>'claim')$$);
