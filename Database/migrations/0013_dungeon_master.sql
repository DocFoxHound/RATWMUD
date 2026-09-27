-- Dungeon Master (Docs/Design/21-dungeon-master.md), phase 1: its accounts, its
-- sessions, the queue of live actions the game server applies, and what the
-- ratw_dm login may touch. The role itself is created by
-- `python3 tools/world_db.py migrate` (roles need the superuser).

-- DM accounts. Kept in PROD; passwords only as salted scrypt hashes.
--   viewer: sees everything, changes nothing
--   dm:     runs the live world (players, NPCs, factions, stories)
--   admin:  everything a dm can, plus managing DM accounts
CREATE TABLE dm.admins (
    username    text PRIMARY KEY CHECK (username ~ '^[a-z][a-z0-9_-]{2,31}$'),
    role        text NOT NULL CHECK (role IN ('viewer', 'dm', 'admin')),
    password    jsonb NOT NULL,
    disabled    boolean NOT NULL DEFAULT false,
    created_at  timestamptz NOT NULL DEFAULT now(),
    last_login  timestamptz
);

-- Signed-in DM sessions: only a hash of the token is stored.
CREATE TABLE dm.sessions (
    token_hash  text PRIMARY KEY,
    username    text NOT NULL REFERENCES dm.admins ON DELETE CASCADE,
    created_at  timestamptz NOT NULL DEFAULT now(),
    expires_at  timestamptz NOT NULL
);

-- Live actions for the running game server (applied in id order, once each; see RatwGameMode ApplyDmActions).
CREATE TABLE dm.actions (
    id            bigserial PRIMARY KEY,
    kind          text NOT NULL CHECK (kind ~ '^[a-z]+\.[a-z_]+$'),
    target_id     text NOT NULL DEFAULT '',
    payload       jsonb NOT NULL DEFAULT '{}' CHECK (jsonb_typeof(payload) = 'object'),
    requested_by  text NOT NULL,
    requested_at  timestamptz NOT NULL DEFAULT now(),
    status        text NOT NULL DEFAULT 'queued' CHECK (status IN ('queued', 'applied', 'refused', 'expired')),
    result        text NOT NULL DEFAULT '',
    done_at       timestamptz
);
CREATE INDEX actions_queued ON dm.actions (id) WHERE status = 'queued';

ALTER TABLE dm.audit ADD COLUMN who text NOT NULL DEFAULT '';

-- What the Dungeon Master may touch: read the world (never write terrain), run the live layer, read game state
-- except logins, and its own schema.
GRANT USAGE ON SCHEMA world, live, game, dm TO ratw_dm;
GRANT SELECT ON ALL TABLES IN SCHEMA world TO ratw_dm;
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA live TO ratw_dm;
GRANT SELECT ON ALL TABLES IN SCHEMA game TO ratw_dm;
REVOKE SELECT ON game.accounts FROM ratw_dm;
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA dm TO ratw_dm;
GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA world, live, dm TO ratw_dm;
ALTER DEFAULT PRIVILEGES FOR ROLE ratw_owner IN SCHEMA world GRANT SELECT ON TABLES TO ratw_dm;
ALTER DEFAULT PRIVILEGES FOR ROLE ratw_owner IN SCHEMA live GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO ratw_dm;
ALTER DEFAULT PRIVILEGES FOR ROLE ratw_owner IN SCHEMA game GRANT SELECT ON TABLES TO ratw_dm;
ALTER DEFAULT PRIVILEGES FOR ROLE ratw_owner IN SCHEMA dm GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO ratw_dm;
ALTER DEFAULT PRIVILEGES FOR ROLE ratw_owner GRANT USAGE, SELECT ON SEQUENCES TO ratw_dm;

-- The game server reads and answers actions, but never sees DM passwords or sessions.
REVOKE ALL ON dm.admins, dm.sessions FROM ratw_game;
