-- Central (Docs/Design/77-central-accounts-and-hosted-worlds.md, section 4): accounts, sessions, wolves, reserved
-- names, issued tickets, and a stub of the world registry (its heartbeat and list are Phase 4). Its own database
-- (ratw_central), never a world's. Times are written by ratw_central from its own clock, so tests can move it.

CREATE SCHEMA central;

-- Accounts. The username is normalised (lower case, 3-32: letters, digits, _ or -, a letter first), as the world's
-- accounts are (Core/RatwAccountsCore). The password is a PBKDF2-HMAC-SHA256 record: {"algorithm": "pbkdf2-sha256",
-- "iterations", "salt" (hex), "hash" (hex)}. Only the age check's passing is kept, never the birth year. No email
-- during the closed beta (doc 77, Q7).
CREATE TABLE central.accounts (
    id             text PRIMARY KEY CHECK (id ~ '^acct-[0-9a-f]{32}$'),
    username       text NOT NULL UNIQUE CHECK (username ~ '^[a-z][a-z0-9_-]{2,31}$'),
    password       jsonb NOT NULL,
    created_at     timestamptz NOT NULL,
    age_checked_at timestamptz NOT NULL,
    banned         boolean NOT NULL DEFAULT false,
    host           boolean NOT NULL DEFAULT false,
    admin          boolean NOT NULL DEFAULT false
);

-- A session: one sign-in. Its access token (15 minutes) and refresh token (30 days with "remember me", else 12 hours)
-- are kept only as SHA-256 hashes. A refresh replaces both; a refresh token used twice ends the whole session (it was
-- copied), which is why the spent ones are kept until they would have expired.
CREATE TABLE central.sessions (
    id              text PRIMARY KEY,
    account_id      text NOT NULL REFERENCES central.accounts (id) ON DELETE CASCADE,
    access_hash     text NOT NULL UNIQUE,
    access_expires  timestamptz NOT NULL,
    refresh_hash    text NOT NULL UNIQUE,
    refresh_expires timestamptz NOT NULL,
    remember        boolean NOT NULL,
    created_at      timestamptz NOT NULL,
    refreshed_at    timestamptz NOT NULL
);
CREATE INDEX sessions_account ON central.sessions (account_id);

CREATE TABLE central.spent_refresh (
    refresh_hash text PRIMARY KEY,
    session_id   text NOT NULL REFERENCES central.sessions (id) ON DELETE CASCADE,
    expires_at   timestamptz NOT NULL
);

-- Wolves: the identity a ticket carries. The name never changes and is unique across central, compared by `name_key`
-- (NFKC, case-folded, spaces collapsed). Everything here was checked by ratw_identity, the game's own rules; the
-- appearance is the game's form of it. The family is the Gift family alone (Q12 (a): the tier and the strengths are
-- chosen at each world's first arrival); '' for none. The profile is the roleplay profile as the game saves it.
CREATE TABLE central.wolves (
    id         text PRIMARY KEY CHECK (id ~ '^wolf-[0-9a-f]{32}$'),
    account_id text NOT NULL REFERENCES central.accounts (id) ON DELETE CASCADE,
    name       text NOT NULL,
    name_key   text NOT NULL UNIQUE,
    sex        text NOT NULL CHECK (sex IN ('female', 'male')),
    age        integer NOT NULL CHECK (age BETWEEN 6 AND 99),
    appearance jsonb NOT NULL,
    family     text NOT NULL DEFAULT '',
    profile    jsonb NOT NULL DEFAULT '{}',
    revision   integer NOT NULL DEFAULT 1,
    created_at timestamptz NOT NULL,
    updated_at timestamptz NOT NULL
);
CREATE INDEX wolves_account ON central.wolves (account_id);

-- Names held back after an account is deleted (30 days), so nobody takes a deleted wolf's name, or username, at once.
CREATE TABLE central.reserved_names (
    kind     text NOT NULL CHECK (kind IN ('wolf', 'username')),
    name_key text NOT NULL,
    until    timestamptz NOT NULL,
    PRIMARY KEY (kind, name_key)
);

-- Wolves deleted on central, for the worlds to forget (the heartbeat reply, Phase 4). Only the id: nothing personal.
CREATE TABLE central.deleted_wolves (
    wolf_id    text PRIMARY KEY,
    deleted_at timestamptz NOT NULL
);

-- The world registry: a stub for Phase 1 (worlds added by hand with `ratw_central.py worlds add`), so tickets can name
-- a world and its kind. Phase 4 adds the host tokens' use, the heartbeat and the list. `kind` is set by central only.
CREATE TABLE central.worlds (
    id             text PRIMARY KEY CHECK (id ~ '^world-[0-9a-f]{32}$'),
    name           text NOT NULL,
    kind           text NOT NULL CHECK (kind IN ('official', 'community')),
    host_account   text REFERENCES central.accounts (id) ON DELETE SET NULL,
    token_hash     text UNIQUE,
    delisted       boolean NOT NULL DEFAULT false,
    created_at     timestamptz NOT NULL,
    last_heartbeat timestamptz,
    address        text,
    port           integer,
    listing        jsonb NOT NULL DEFAULT '{}',
    online         jsonb NOT NULL DEFAULT '[]'
);

-- Every ticket issued, for counting per world (honest player counts, Phase 4b) and limits. Kept a week.
CREATE TABLE central.tickets (
    jti        text PRIMARY KEY,
    world_id   text NOT NULL,
    wolf_id    text NOT NULL,
    account_id text NOT NULL,
    kid        text NOT NULL,
    issued_at  timestamptz NOT NULL,
    expires_at timestamptz NOT NULL
);
CREATE INDEX tickets_world ON central.tickets (world_id, issued_at);
