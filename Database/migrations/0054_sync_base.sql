-- Pull and import, with conflicts (Docs/Design/66-worlds-atlas-and-dm.md, section 4), and maintenance.
--
-- admin.sync_base (in every world; used by a dev world with an upstream): the snapshot of its upstream live world as of
-- the last pull or import, the base B of the three-way comparison. One header row (which upstream, the release it
-- saw, when, who) and admin.sync_base_rows, every synced row by (table, key) as JSON with a digest of it (the content
-- is tens of megabytes on a big map, mostly terrain: a row each keeps it out of one huge value). tools/world_sync.py
-- writes them; nothing else does.
--
-- admin.server_state (in every world): whether the world's game server may run (running) or is closed for
-- maintenance, who changed it, when and why. A change is announced on ratw_maintenance; the game server listens,
-- tells its players, saves and exits (status 76), and refuses to start while it says maintenance.

CREATE TABLE admin.sync_base (
    singleton  boolean PRIMARY KEY DEFAULT true CHECK (singleton),
    upstream   text NOT NULL,                              -- The upstream world's registry key.
    world_id   ratw_id,                                    -- The world both hold (null: the upstream had none yet).
    release    integer NOT NULL DEFAULT 0,                 -- The upstream's latest release when it was taken.
    taken_at   timestamptz NOT NULL DEFAULT now(),
    taken_by   text NOT NULL DEFAULT '' CHECK (char_length(taken_by) <= 60),
    how        text NOT NULL CHECK (how IN ('init', 'copy', 'pull', 'import')),
    rows       integer NOT NULL DEFAULT 0
);

CREATE TABLE admin.sync_base_rows (
    table_name  text NOT NULL,                             -- e.g. live.npcs
    key         jsonb NOT NULL CHECK (jsonb_typeof(key) = 'array'),   -- The row's key, without the world.
    digest      text NOT NULL,                             -- sha1 of the row's canonical JSON.
    row         jsonb NOT NULL,
    PRIMARY KEY (table_name, key)
);

CREATE TABLE admin.server_state (
    singleton   boolean PRIMARY KEY DEFAULT true CHECK (singleton),
    state       text NOT NULL DEFAULT 'running' CHECK (state IN ('running', 'maintenance')),
    changed_by  text NOT NULL DEFAULT '' CHECK (char_length(changed_by) <= 60),
    changed_at  timestamptz NOT NULL DEFAULT now(),
    note        text NOT NULL DEFAULT '' CHECK (char_length(note) <= 400)
);
INSERT INTO admin.server_state DEFAULT VALUES;

CREATE FUNCTION admin.announce_server_state() RETURNS trigger LANGUAGE plpgsql AS $$
BEGIN
    IF TG_OP = 'INSERT' OR NEW.state IS DISTINCT FROM OLD.state OR NEW.note IS DISTINCT FROM OLD.note THEN
        PERFORM pg_notify('ratw_maintenance', json_build_object('state', NEW.state, 'by', NEW.changed_by,
                                                                'at', NEW.changed_at, 'note', NEW.note)::text);
    END IF;
    RETURN NEW;
END $$;
CREATE TRIGGER announce AFTER INSERT OR UPDATE ON admin.server_state FOR EACH ROW EXECUTE FUNCTION admin.announce_server_state();

-- An import is a release of its own kind, its password attempts counted with the rest.
ALTER TABLE admin.releases DROP CONSTRAINT IF EXISTS releases_kind_check;
ALTER TABLE admin.releases ADD CONSTRAINT releases_kind_check CHECK (kind IN ('push', 'rollback', 'import'));
ALTER TABLE admin.publish_attempts DROP CONSTRAINT IF EXISTS publish_attempts_action_check;
ALTER TABLE admin.publish_attempts ADD CONSTRAINT publish_attempts_action_check CHECK (action IN ('push', 'rollback', 'import'));

-- Who may do what. Pull and import run as ratw_publisher (the one role that reaches both a dev and a live world) and
-- replace the base; Atlas (ratw_editor) may read and write a dev world's base; the game server and the DM read the
-- server's state; ratw_publisher sets it (tools/world_sync.py maintenance, the DM through it).
GRANT USAGE ON SCHEMA admin TO ratw_editor, ratw_game, ratw_dm;
GRANT SELECT, INSERT, UPDATE, DELETE ON admin.sync_base, admin.sync_base_rows TO ratw_publisher, ratw_editor;
GRANT SELECT, UPDATE ON admin.server_state TO ratw_publisher;
GRANT SELECT ON admin.server_state TO ratw_editor, ratw_game, ratw_dm;
