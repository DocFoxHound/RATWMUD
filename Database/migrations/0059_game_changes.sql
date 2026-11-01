-- The live world's truth (Docs/Design/66-worlds-atlas-and-dm.md, Progress, "The live world's truth: NPCs and markets in
-- conflicts"): what the game server changes in the live tables Atlas and the sync also write, row by row, as the
-- database itself sees it.
--
--   live.game_changes  one row a change the game server (role ratw_game) made to live.npcs, live.profession_slots,
--                      live.npc_relations or live.faction_members: the table, the row's key (a JSON array, without the
--                      world), insert | update | delete, the row before and after (JSON, without updated_at), and the
--                      world's revision at the time (the game's writes never raise it). Rows a spawn rule made (spawn_id
--                      set) are left out: each server's own, as the sync leaves them out.
--
-- Why a log the database keeps, not a revision the game raises: every write of the game is caught, whatever code makes it
-- (an arrival's home today, a marriage or a death tomorrow), with the value it replaced, which is the base Atlas needs to
-- tell "the game moved it" from "you moved it". Raising the world's revision instead would refuse every whole-world save
-- (the generator's, minutes long) whenever the game placed anyone, and say nothing about which row or field.
--
-- Read by Atlas (live_edit's keyed edits and its heartbeat, world_store.save_world) on a world a game server runs:
-- a game-owned field the game changed since Atlas read it keeps the game's value, Atlas's own edits to authored fields
-- still save, and where both changed one field it is a conflict that defaults to the game's. Written only by the trigger.
CREATE TABLE live.game_changes (
    seq         bigserial PRIMARY KEY,
    world_id    ratw_id NOT NULL,
    table_name  text NOT NULL,
    row_key     jsonb NOT NULL CHECK (jsonb_typeof(row_key) = 'array'),
    op          text NOT NULL CHECK (op IN ('insert', 'update', 'delete')),
    before      jsonb,
    after       jsonb,
    revision    bigint NOT NULL DEFAULT 0,
    at          timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX game_changes_row ON live.game_changes (world_id, table_name, row_key, seq);
COMMENT ON TABLE live.game_changes IS 'What the game server changed in live.npcs, profession_slots, npc_relations and faction_members (doc 66, Progress, "The live world''s truth").';

CREATE FUNCTION live.log_game_change() RETURNS trigger LANGUAGE plpgsql SECURITY DEFINER
SET search_path = pg_catalog, pg_temp AS $$
DECLARE
    old_row jsonb := CASE WHEN TG_OP <> 'INSERT' THEN to_jsonb(OLD) - 'updated_at' END;
    new_row jsonb := CASE WHEN TG_OP <> 'DELETE' THEN to_jsonb(NEW) - 'updated_at' END;
    any_row jsonb := coalesce(new_row, old_row);
    key jsonb := '[]'::jsonb;
    col text;
BEGIN
    IF coalesce(old_row->>'spawn_id', new_row->>'spawn_id') IS NOT NULL OR old_row IS NOT DISTINCT FROM new_row THEN
        RETURN NULL;
    END IF;
    FOREACH col IN ARRAY TG_ARGV LOOP
        key := key || jsonb_build_array(any_row->col);
    END LOOP;
    INSERT INTO live.game_changes (world_id, table_name, row_key, op, before, after, revision)
    VALUES (any_row->>'world_id', TG_TABLE_SCHEMA || '.' || TG_TABLE_NAME, key, lower(TG_OP), old_row, new_row,
            coalesce((SELECT w.revision FROM world.worlds w WHERE w.id = any_row->>'world_id'), 0));
    RETURN NULL;
END $$;
REVOKE ALL ON FUNCTION live.log_game_change() FROM PUBLIC;

CREATE TRIGGER game_changes AFTER INSERT OR UPDATE OR DELETE ON live.npcs FOR EACH ROW
    WHEN (current_user = 'ratw_game') EXECUTE FUNCTION live.log_game_change('id');
CREATE TRIGGER game_changes AFTER INSERT OR UPDATE OR DELETE ON live.profession_slots FOR EACH ROW
    WHEN (current_user = 'ratw_game') EXECUTE FUNCTION live.log_game_change('id');
CREATE TRIGGER game_changes AFTER INSERT OR UPDATE OR DELETE ON live.npc_relations FOR EACH ROW
    WHEN (current_user = 'ratw_game') EXECUTE FUNCTION live.log_game_change('npc_id', 'other_id', 'kind');
CREATE TRIGGER game_changes AFTER INSERT OR UPDATE OR DELETE ON live.faction_members FOR EACH ROW
    WHEN (current_user = 'ratw_game') EXECUTE FUNCTION live.log_game_change('faction_id', 'npc_id');

-- Read by everyone who saves or compares the live tables; written by the trigger only. (The live schema's default
-- privileges gave the editor, the publisher and the game writing too.)
REVOKE INSERT, UPDATE, DELETE, TRUNCATE ON live.game_changes FROM ratw_editor, ratw_publisher, ratw_game, ratw_dm;
GRANT SELECT ON live.game_changes TO ratw_editor, ratw_publisher, ratw_game, ratw_dm;
