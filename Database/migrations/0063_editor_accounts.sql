-- 0063_editor_accounts.sql: Editor accounts, the helper's sign-in state, and the dm.* tables closed to the game server
-- (Docs/Design/78-hosts-run-their-own-worlds.md, sections 6.3 and 6.5, rule I11; Phase M2a). Schema only: no row is
-- rewritten (existing accounts keep their role, and every new column has a default that changes nothing for them).
--
-- Editor accounts are the DM tool's own (dm.admins, dm.sessions), kept in each world's own database in hosted mode.
--   viewer:  watches, changes nothing           builder: also Atlas and Publish (slice 2)
--   dm:      the DM tool                         owner:   also My world's actions and Editor accounts
--   admin:   the user's own studio only (the Dev Console); an admin row can never sign in to a host's Editor.
ALTER TABLE dm.admins DROP CONSTRAINT admins_role_check;
ALTER TABLE dm.admins ADD CONSTRAINT admins_role_check CHECK (role IN ('viewer', 'dm', 'builder', 'owner', 'admin'));
-- A one-time password (an account the owner just added or reset, or the world's first owner) works only to choose a
-- password, and only until must_change_until.
ALTER TABLE dm.admins ADD COLUMN must_change boolean NOT NULL DEFAULT false;
ALTER TABLE dm.admins ADD COLUMN must_change_until timestamptz;
-- "May act on players' wolves" (kill, injure, move, resurrect: doc 78, Q24). The owner always may.
ALTER TABLE dm.admins ADD COLUMN may_act_on_wolves boolean NOT NULL DEFAULT false;
-- Where the account was first signed in from (the accounts list's "claimed from"), and who made it.
ALTER TABLE dm.admins ADD COLUMN claimed_from text;
ALTER TABLE dm.admins ADD COLUMN created_by text NOT NULL DEFAULT '';

-- Sessions end after 30 minutes without the host's input (last_seen, renewed only by the shell's keep-alive and the
-- client's own screens) as well as at expires_at. device: the dm.devices row it came from (its token's hash).
-- step_up_until: the 5-minute grant for the account routes, after the password typed again. address: the sign-in's
-- address (Editor accounts only, for the owner's list). since: the account's previous sign-in (the owner's banners).
ALTER TABLE dm.sessions ADD COLUMN last_seen timestamptz NOT NULL DEFAULT now();
ALTER TABLE dm.sessions ADD COLUMN device text;
ALTER TABLE dm.sessions ADD COLUMN step_up_until timestamptz;
ALTER TABLE dm.sessions ADD COLUMN address text NOT NULL DEFAULT '';
ALTER TABLE dm.sessions ADD COLUMN since timestamptz;

-- Known devices: a token the shell keeps in the OS keychain, stored here only as its SHA-256. It counts only for the
-- account it was issued to; at most 5 an account (the oldest dropped). Five wrong passwords from one device make that
-- device wait (wait_until); nothing else is blocked by them.
CREATE TABLE dm.devices (
    token_hash  text PRIMARY KEY,
    username    text NOT NULL REFERENCES dm.admins ON DELETE CASCADE,
    created_at  timestamptz NOT NULL DEFAULT now(),
    last_used   timestamptz NOT NULL DEFAULT now(),
    failures    integer NOT NULL DEFAULT 0 CHECK (failures >= 0),
    wait_until  timestamptz
);
CREATE INDEX devices_username ON dm.devices (username, last_used);

-- The sign-in pass inside an Editor code: works once, until expires_at; five wrong passwords with it void it.
CREATE TABLE dm.invites (
    invite_hash text PRIMARY KEY,
    username    text NOT NULL REFERENCES dm.admins ON DELETE CASCADE,
    created_at  timestamptz NOT NULL DEFAULT now(),
    expires_at  timestamptz NOT NULL,
    used_at     timestamptz,
    failures    integer NOT NULL DEFAULT 0 CHECK (failures >= 0),
    created_by  text NOT NULL DEFAULT ''
);
CREATE INDEX invites_username ON dm.invites (username);

-- Per-account lock state against typed passwords from unknown devices, so a restart of the helper keeps it: after 5
-- failures, unknown devices without an Editor code wait 15 minutes, doubling up to 4 hours (level).
CREATE TABLE dm.locks (
    username     text PRIMARY KEY REFERENCES dm.admins ON DELETE CASCADE,
    failures     integer NOT NULL DEFAULT 0 CHECK (failures >= 0),
    level        integer NOT NULL DEFAULT 0 CHECK (level >= 0),
    first_at     timestamptz NOT NULL DEFAULT now(),
    locked_until timestamptz,
    updated_at   timestamptz NOT NULL DEFAULT now()
);

-- The host's tools' own AI calls (life stories, roster generation: doc 78, section 7), counted where the game server
-- can't write, for the daily caps and the monthly budget (M3).
CREATE TABLE dm.ai_use (
    id          bigserial PRIMARY KEY,
    at          timestamptz NOT NULL DEFAULT now(),
    tool        text NOT NULL CHECK (tool ~ '^[a-z][a-z0-9_.-]{0,39}$'),
    account     text NOT NULL DEFAULT '',
    model       text NOT NULL DEFAULT '',
    calls       integer NOT NULL DEFAULT 1 CHECK (calls >= 0),
    tokens_in   bigint NOT NULL DEFAULT 0 CHECK (tokens_in >= 0),
    tokens_out  bigint NOT NULL DEFAULT 0 CHECK (tokens_out >= 0)
);
CREATE INDEX ai_use_at ON dm.ai_use (at);

-- The legacy Storykeeper (tools/dm_service.py, which connects as ratw_game and isn't started anywhere: doc 78, Q18)
-- kept its log in dm.audit, the DM tool's history. It gets a table of its own, so dm.audit can be closed to the game.
CREATE TABLE dm.storykeeper_audit (
    sequence  bigserial PRIMARY KEY,
    at        double precision NOT NULL,
    action    text NOT NULL,
    target    text NOT NULL,
    detail    text NOT NULL
);
GRANT SELECT, INSERT, UPDATE, DELETE ON dm.storykeeper_audit TO ratw_game, ratw_dm;
GRANT USAGE, SELECT ON SEQUENCE dm.storykeeper_audit_sequence_seq TO ratw_game, ratw_dm;

-- I11: no dm.* table the helper trusts is readable or writable by ratw_game. The game server keeps only what it uses
-- (dm.actions, dm.health, dm.watch, dm.watchers: Core/, by grep) and the legacy Storykeeper's own tables (dm.meta,
-- documents, commands, activity, routes, arrivals, storykeeper_audit). dm.admins and dm.sessions were closed in 0013.
REVOKE ALL ON dm.devices, dm.invites, dm.locks, dm.ai_use, dm.audit, dm.stories FROM ratw_game;
REVOKE ALL ON SEQUENCE dm.audit_sequence_seq, dm.ai_use_id_seq FROM ratw_game;
-- And every later dm.* table starts closed to it (0012 opened the schema's new tables to ratw_game by default): a table
-- the server needs is granted by name in its migration, as 0031 and 0032 did.
ALTER DEFAULT PRIVILEGES FOR ROLE ratw_owner IN SCHEMA dm REVOKE SELECT, INSERT, UPDATE, DELETE ON TABLES FROM ratw_game;
