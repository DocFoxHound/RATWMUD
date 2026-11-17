-- 0066_mantles_down.sql: migration 0066 (mantles, Docs/Design/84-living-conversations.md §17.1) taken back. Run by hand
-- as the owner, in one transaction, and ONLY after `python3 tools/mantle_load.py backup <world>` has dumped every mantle,
-- item, history row and binding to Saved/Backups/ (`mantle_load.py restore FILE` puts them back once 0066 is applied
-- again):
--
--     python3 tools/mantle_load.py backup <world>
--     psql "$OWNER_URL" --single-transaction -v ON_ERROR_STOP=1 -f Database/rollback/0066_mantles_down.sql
--
-- Not a migration (world_db.migrate never reads this folder). Everything 0066 added goes, in the reverse order, and its
-- row in public.schema_migrations is deleted, so world_db.migrate applies 0066 again later. live.npc_voice keeps every
-- row and column it had before 0066; only the binding columns (mantle_id, bound_day, bound_how, released_day) go. A
-- server built before Phase 9 never read them; one built since reads past their absence (no mantles: the library speaks).

DROP FUNCTION IF EXISTS dm.queue_talk_sync(text, text, text);
DROP TRIGGER IF EXISTS game_changes ON live.npc_voice;
DROP INDEX IF EXISTS live.npc_voice_mantle_worn;
ALTER TABLE live.npc_voice DROP CONSTRAINT IF EXISTS npc_voice_mantle,
                           DROP COLUMN IF EXISTS mantle_id,
                           DROP COLUMN IF EXISTS bound_day,
                           DROP COLUMN IF EXISTS bound_how,
                           DROP COLUMN IF EXISTS released_day;
DROP TRIGGER IF EXISTS mantle_changed ON live.mantle_lines;
DROP TRIGGER IF EXISTS mantle_changed ON live.mantles;
DROP TABLE IF EXISTS live.mantle_history;
DROP TABLE IF EXISTS live.mantle_lines;
DROP TABLE IF EXISTS live.mantles;
DROP FUNCTION IF EXISTS live.mantle_changed();
DELETE FROM public.schema_migrations WHERE version = '0066';
