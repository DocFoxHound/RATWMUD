-- Checkpoints that send only what changed (Docs/Design/26-living-npcs.md, Phase 2). game.save_checkpoint takes the
-- whole save document and compares every row of every list with what is stored; with thousands of residents that is
-- most of the work of a save, every fifteen seconds, for rows that mostly haven't changed. The game server now
-- remembers what it last wrote and sends:
--
--   p_rest     the document without its lists (clock, calendar, society, receipts...), as save_checkpoint stores it;
--   p_changes  for every list in the document, {"rows": [{"key", "position", "data"}...], "deleted": [key...]}: the
--              entries that are new or changed (keyed exactly as game.sections keys them, "#2" etc. for repeats), and
--              the keys no longer there. A list with nothing changed is still named, with empty arrays.
--
-- The stored save is then exactly what save_checkpoint would have stored, and game.load_checkpoint reads it back
-- unchanged. The server still sends a whole document now and then (on start, after a failed write, and every so
-- often), so anything changed behind its back is put right.
CREATE FUNCTION game.save_checkpoint_delta(p_world ratw_id, p_revision bigint, p_rest text, p_changes jsonb) RETURNS void
LANGUAGE plpgsql AS $$
DECLARE
    present text[] := '{}';
    s game.sections;
    change jsonb;
BEGIN
    FOR s IN SELECT * FROM game.sections LOOP
        change := p_changes -> s.name;
        IF change IS NULL THEN
            CONTINUE;                              -- Not in this document (an older save format).
        END IF;
        present := present || s.name;
        EXECUTE format('DELETE FROM game.%I WHERE world_id = $1 AND key IN (SELECT jsonb_array_elements_text($2))', s.tbl)
            USING p_world, coalesce(change -> 'deleted', '[]'::jsonb);
        EXECUTE format($f$
            INSERT INTO game.%I AS t (world_id, key, position, data)
            SELECT $1, r->>'key', (r->>'position')::integer, r->'data' FROM jsonb_array_elements($2) r
            ON CONFLICT (world_id, key) DO UPDATE SET position = excluded.position, data = excluded.data, updated_at = now()
            $f$, s.tbl) USING p_world, coalesce(change -> 'rows', '[]'::jsonb);
    END LOOP;
    INSERT INTO game.checkpoints (world_id, schema_version, revision, payload, saved_at)
    VALUES (p_world, 2, p_revision, (p_rest::jsonb || jsonb_build_object('$sections', to_jsonb(present)))::text, now())
    ON CONFLICT (world_id) DO UPDATE SET schema_version = 2, revision = excluded.revision, payload = excluded.payload,
                                         saved_at = excluded.saved_at;
END $$;
REVOKE ALL ON FUNCTION game.save_checkpoint_delta(ratw_id, bigint, text, jsonb) FROM PUBLIC;
GRANT EXECUTE ON FUNCTION game.save_checkpoint_delta(ratw_id, bigint, text, jsonb) TO ratw_game;
