-- Game-server state and publishing.
--
-- game.checkpoints holds the server's complete save (accounts, characters, NPC
-- state, memories, relationships, map memory, doors, clock) as one versioned
-- document, exactly as the SQLite checkpoint does today. Moving the server here
-- keeps its atomic save; later migrations split the document into proper tables
-- (accounts, characters, npc_memories, relationships, ...) one part at a time.

CREATE TABLE game.checkpoints (
    world_id        ratw_id PRIMARY KEY REFERENCES world.worlds ON DELETE CASCADE,
    schema_version  integer NOT NULL,
    revision        bigint NOT NULL,
    payload         jsonb NOT NULL,
    saved_at        timestamptz NOT NULL DEFAULT now()
);

-- Settings changed only by administrators (tools/world_db.py), e.g. the publish password hash.
CREATE TABLE admin.settings (
    key         text PRIMARY KEY,
    value       jsonb NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now()
);

-- Every Push to live, with a full copy of the authored content it replaced, for rollback.
CREATE TABLE admin.releases (
    world_id   ratw_id NOT NULL,
    number     integer NOT NULL CHECK (number > 0),
    pushed_at  timestamptz NOT NULL DEFAULT now(),
    pushed_by  text NOT NULL DEFAULT '',
    note       text NOT NULL DEFAULT '',
    replaced   jsonb,
    PRIMARY KEY (world_id, number)
);
