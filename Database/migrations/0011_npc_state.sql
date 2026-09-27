-- What each NPC is doing and has, as the game server last saved it (design step 7).
--
-- Written by the server with every save, in the same transaction as its
-- checkpoint. Read by the server at start-up: rows newer than its own
-- checkpoint were put there from outside (e.g. DEV copying the live NPC state
-- from PROD) and are applied over what it restored.
-- state: {"cell", "x", "y", "age", "hunger", "fatigue", "cash", "stock": {"herbs", "meal"}, "task"}
CREATE TABLE live.npc_state (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    npc_id      text NOT NULL CHECK (char_length(npc_id) BETWEEN 1 AND 80),
    alive       boolean NOT NULL DEFAULT true,
    state       jsonb NOT NULL CHECK (jsonb_typeof(state) = 'object'),
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, npc_id)
);
