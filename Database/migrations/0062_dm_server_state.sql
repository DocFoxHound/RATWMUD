-- 0062_dm_server_state.sql: the DM tool may close and open its world (Docs/Design/78-hosts-run-their-own-worlds.md, 7;
-- Phase M1). Schema only: no row is rewritten.
--
-- Close and Open set admin.server_state, which only ratw_publisher could update (0054). A host's Editor reaches them
-- through the studio as ratw_dm, never with the publisher's password (which also writes builds and runs the push), so
-- ratw_dm may update the state's four columns, and nothing else: no INSERT or DELETE (the table is one row), and not
-- `singleton`. It already reads the table (0054). The trigger that tells the server (ratw_maintenance) fires as before.
GRANT UPDATE (state, changed_by, changed_at, note) ON admin.server_state TO ratw_dm;
