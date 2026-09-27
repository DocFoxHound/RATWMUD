-- The server's save, split into tables (design step 8), and the Storykeeper's
-- state (formerly its own SQLite file).
--
-- The game server still hands over one versioned save document and gets the
-- same document back, so its validation and restore code is unchanged. These
-- functions store every per-thing list as one row per thing, in its own table,
-- writing only rows that changed; the checkpoint row keeps what is left
-- (clock, calendar, weather, doors, society, receipts...).

-- One table per list in the save. key identifies the thing (see game.sections);
-- position keeps the order the server wrote them in; data is the thing itself.
DO $$
DECLARE t text;
BEGIN
    FOREACH t IN ARRAY ARRAY['accounts', 'characters', 'npcs', 'map_memories', 'conversations', 'npc_memories',
                             'relationships', 'social_recent', 'social_sessions']
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

COMMENT ON TABLE game.accounts IS 'Player logins (username, password verifier, their characters). Game server only.';
COMMENT ON TABLE game.characters IS 'Player characters: body, position, sheet, inventory.';
COMMENT ON TABLE game.npcs IS 'NPC bodies as the simulation holds them.';
COMMENT ON TABLE game.map_memories IS 'What each character remembers of each cell (the World Map).';
COMMENT ON TABLE game.conversations IS 'Open conversations between NPCs and characters.';
COMMENT ON TABLE game.npc_memories IS 'Consolidated NPC memories of past conversations.';
COMMENT ON TABLE game.relationships IS 'The social ledger: who did what with whom.';
COMMENT ON TABLE game.social_recent IS 'Recent social activity per character.';
COMMENT ON TABLE game.social_sessions IS 'Social scenes and their members.';

-- Readable names for the tools to come (e.g. the live manager).
ALTER TABLE game.characters ADD COLUMN name text GENERATED ALWAYS AS (data->>'name') STORED;
ALTER TABLE game.npcs ADD COLUMN name text GENERATED ALWAYS AS (data->>'name') STORED;
ALTER TABLE game.npc_memories ADD COLUMN npc text GENERATED ALWAYS AS (data->>'npc') STORED;
ALTER TABLE game.relationships ADD COLUMN actor text GENERATED ALWAYS AS (data->>'actor') STORED,
                               ADD COLUMN partner text GENERATED ALWAYS AS (data->>'partner') STORED;

-- Which list of the save goes where, and what identifies one entry. Entries that share a key
-- (possible in logs) are told apart by occurrence: "key#2", "key#3"...
CREATE TABLE game.sections (
    name      text PRIMARY KEY,       -- the list's name in the save document
    path      text[] NOT NULL,        -- where it sits in the document
    tbl       text NOT NULL,          -- game.<tbl>
    key_expr  text NOT NULL           -- SQL over the entry "e" giving its key
);
INSERT INTO game.sections VALUES
    ('accounts',       '{accounts,entries}', 'accounts',        $$e->>'username'$$),
    ('players',        '{players}',          'characters',      $$e->>'id'$$),
    ('npcs',           '{npcs}',             'npcs',            $$e->>'id'$$),
    ('mapMemories',    '{mapMemories}',      'map_memories',    $$concat(e->>'observer', '|', e->>'id')$$),
    ('activeMemory',   '{activeMemory}',     'conversations',   $$e->>'key'$$),
    ('summaries',      '{summaries}',        'npc_memories',    $$e->>'id'$$),
    ('ledger',         '{ledger}',           'relationships',   $$concat(e->>'event', '|', e->>'actor', '|', e->>'partner', '|', e->>'reason')$$),
    ('socialRecent',   '{socialRecent}',     'social_recent',   $$concat(e->>'event', '|', e->>'actor', '|', e->>'cell')$$),
    ('socialSessions', '{socialSessions}',   'social_sessions', $$e->>'id'$$);

CREATE FUNCTION game.save_checkpoint(p_world ratw_id, p_revision bigint, p_payload text) RETURNS void
LANGUAGE plpgsql AS $$
DECLARE
    doc jsonb := p_payload::jsonb;
    rest jsonb := doc;
    present text[] := '{}';
    s game.sections;
BEGIN
    FOR s IN SELECT * FROM game.sections LOOP
        IF doc #> s.path IS NULL THEN
            CONTINUE;                          -- An older save without this list: nothing to write, nothing to restore.
        END IF;
        present := present || s.name;
        EXECUTE format($f$
            WITH items AS (
                SELECT e AS data, (n - 1)::integer AS position, coalesce(%1$s, '') AS base
                FROM jsonb_array_elements(coalesce($1 #> $2, '[]'::jsonb)) WITH ORDINALITY AS x(e, n)),
            keyed AS (
                SELECT data, position,
                       base || CASE WHEN row_number() OVER w > 1 THEN '#' || row_number() OVER w ELSE '' END AS key
                FROM items WINDOW w AS (PARTITION BY base ORDER BY position)),
            gone AS (
                DELETE FROM game.%2$I t WHERE t.world_id = $3 AND NOT EXISTS (SELECT 1 FROM keyed k WHERE k.key = t.key))
            INSERT INTO game.%2$I AS t (world_id, key, position, data)
            SELECT $3, key, position, data FROM keyed
            ON CONFLICT (world_id, key) DO UPDATE SET position = excluded.position, data = excluded.data, updated_at = now()
            WHERE t.position IS DISTINCT FROM excluded.position OR t.data IS DISTINCT FROM excluded.data
            $f$, s.key_expr, s.tbl) USING doc, s.path, p_world;
        rest := rest #- s.path;
    END LOOP;
    rest := rest || jsonb_build_object('$sections', to_jsonb(present));
    INSERT INTO game.checkpoints (world_id, schema_version, revision, payload, saved_at)
    VALUES (p_world, 2, p_revision, rest::text, now())
    ON CONFLICT (world_id) DO UPDATE SET schema_version = 2, revision = excluded.revision, payload = excluded.payload,
                                         saved_at = excluded.saved_at;
END $$;

-- The save document as the server wrote it (a legacy one-row save is returned as it is), or NULL for a new world.
CREATE FUNCTION game.load_checkpoint(p_world ratw_id) RETURNS text
LANGUAGE plpgsql STABLE AS $$
DECLARE
    version integer;
    stored text;
    doc jsonb;
    items jsonb;
    s game.sections;
BEGIN
    SELECT schema_version, payload INTO version, stored FROM game.checkpoints WHERE world_id = p_world;
    IF NOT FOUND THEN
        RETURN NULL;
    END IF;
    IF version = 1 THEN
        RETURN stored;
    END IF;
    doc := stored::jsonb;
    FOR s IN SELECT * FROM game.sections WHERE name IN (SELECT jsonb_array_elements_text(doc->'$sections')) LOOP
        EXECUTE format('SELECT coalesce(jsonb_agg(data ORDER BY position), ''[]''::jsonb) FROM game.%I WHERE world_id = $1', s.tbl)
            INTO items USING p_world;
        doc := jsonb_set(doc, s.path, items, true);
    END LOOP;
    -- The document goes back with schema 1: the version of the document format, which has not changed.
    RETURN (doc - '$sections')::text;
END $$;

REVOKE ALL ON FUNCTION game.save_checkpoint(ratw_id, bigint, text), game.load_checkpoint(ratw_id) FROM PUBLIC;
GRANT EXECUTE ON FUNCTION game.save_checkpoint(ratw_id, bigint, text), game.load_checkpoint(ratw_id) TO ratw_game;
-- Logins hold password verifiers: only the game server reads them.
REVOKE SELECT ON game.accounts FROM ratw_editor, ratw_publisher;

-- The Storykeeper (tools/dm_service.py), in its own schema. One Storykeeper per database, pinned to its world.
CREATE SCHEMA dm;
GRANT USAGE ON SCHEMA dm TO ratw_game;
ALTER DEFAULT PRIVILEGES FOR ROLE ratw_owner IN SCHEMA dm GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO ratw_game;
CREATE TABLE dm.meta (key text PRIMARY KEY, value text NOT NULL);
CREATE TABLE dm.documents (kind text NOT NULL, id text NOT NULL, data text NOT NULL, PRIMARY KEY (kind, id));
CREATE TABLE dm.commands (id text PRIMARY KEY, fingerprint text NOT NULL, response text NOT NULL);
CREATE TABLE dm.audit (sequence bigserial PRIMARY KEY, at double precision NOT NULL, action text NOT NULL,
                       target text NOT NULL, detail text NOT NULL);
CREATE TABLE dm.activity (chapter text NOT NULL, weekday integer NOT NULL, hour integer NOT NULL,
                          activeminutes double precision NOT NULL, memberminutes double precision NOT NULL,
                          samples integer NOT NULL, PRIMARY KEY (chapter, weekday, hour));
CREATE TABLE dm.routes (chapter text NOT NULL, source text NOT NULL, destination text NOT NULL,
                        transitions integer NOT NULL, PRIMARY KEY (chapter, source, destination));
CREATE TABLE dm.arrivals (npc text PRIMARY KEY, at double precision NOT NULL, event text NOT NULL);
