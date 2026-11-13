-- 0065_publish.sql: Publish (Docs/Design/78-hosts-run-their-own-worlds.md, 10.3, 12.3-12.5; Phase M8). Schema only: no
-- authored row is rewritten.
--
-- world.builds gains what Publish needs to know of a build:
--   edits_through  the newest world.edits.seq the build was made from ("N map changes not published" counts the map
--                  edits after it; NULL on a build made before this, which counts none);
--   format         the build-format number (tools/world_build.py's BUILD_FORMAT, Core's build::WorldBuildFormat): the
--                  server loads the current format and the one before it, so a format bump doesn't force a rebuild;
--   copy_of        the build this one copies ("Back to the previous map", and the entrypoint's fallback: the server
--                  takes only newer builds, so the previous map is published again under a new id);
--   failed         a build the server didn't start on (the fallback marks it; it stays for the bug report).
ALTER TABLE world.builds ADD COLUMN edits_through bigint,
                         ADD COLUMN format integer NOT NULL DEFAULT 1 CHECK (format >= 1),
                         ADD COLUMN copy_of bigint,
                         ADD COLUMN failed boolean NOT NULL DEFAULT false;

-- admin.server_state gains host_only: the server runs and admits only the host's tickets (a first look at a new map).
ALTER TABLE admin.server_state DROP CONSTRAINT IF EXISTS server_state_state_check;
ALTER TABLE admin.server_state ADD CONSTRAINT server_state_state_check CHECK (state IN ('running', 'maintenance', 'host_only'));
