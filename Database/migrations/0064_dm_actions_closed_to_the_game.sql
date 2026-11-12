-- 0064_dm_actions_closed_to_the_game.sql: the game server answers the DM tool's acts, never makes or rewrites one
-- (Docs/Design/78-hosts-run-their-own-worlds.md, rule I11; the R1 review's low "ratw_game keeps INSERT/UPDATE on
-- dm.actions": a compromised server could forge requested_by in the acts' history). Schema only: no row is rewritten.
--
-- 0012's default privileges gave ratw_game every DML right on dm.actions (0013). What Core needs, by grep
-- (Core/RatwGame.cpp, applyDmActions): it reads the queued rows, and answers each one (status, result, done_at). It
-- never inserts, deletes, or changes what was asked, by whom or when (kind, target_id, payload, requested_by,
-- requested_at). The DM tool (ratw_dm) and the helper keep theirs.
REVOKE INSERT, UPDATE, DELETE ON dm.actions FROM ratw_game;
GRANT UPDATE (status, result, done_at) ON dm.actions TO ratw_game;
