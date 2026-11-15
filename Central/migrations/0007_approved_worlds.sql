-- Approved worlds (Docs/Design/77-central-accounts-and-hosted-worlds.md, section 7.1; Docs/Design/49-characters-and-
-- earned-gifts.md, 9.2; the user, 2026-11-15: "there should be 'approved' worlds that are built with an unmodified
-- Atlas, but that should be an automatic thing"). Progression (first: the account-wide Quickened unlock) counts from an
-- official world, or from a community world central approves by itself: it runs a released world image's own software
-- and progression rules (their fingerprints match a signed release manifest), without cheats or open tiers, and its
-- unlocks come with evidence central checks on its own (tickets, play time, stars their givers confirmed).

-- The releases central knows: one signed manifest per world image (Package/stage.sh --build), imported with
-- `ratw_central.py releases add FILE` (or from Central/releases/ when central starts). Withdrawn: no longer approves.
CREATE TABLE central.releases (
    build         text PRIMARY KEY,
    server        text NOT NULL,          -- SHA-256 of ratw_server, as the image holds it
    agent         text NOT NULL,          -- of tools/world_agent.py
    fingerprints  text NOT NULL,          -- of tools/world_fingerprints.py (what computes the others)
    rules         text NOT NULL,          -- the progression rules' fingerprint (tools/world_fingerprints.py)
    manifest      jsonb NOT NULL,         -- the whole manifest (its files, the Quickened thresholds)
    key_id        text NOT NULL,          -- the release key that signed it ('' when imported unsigned, by hand)
    made_at       timestamptz,
    added_at      timestamptz NOT NULL,
    withdrawn_at  timestamptz,
    withdrawn_note text NOT NULL DEFAULT ''
);

ALTER TABLE central.worlds
    -- What the world agent last said the world runs ({server, agent, fingerprints, rules}: SHA-256 each), and central's
    -- verdict from it at that heartbeat: approved or not, by which release, why not, and since when.
    ADD COLUMN fingerprints        jsonb,
    ADD COLUMN approved            boolean NOT NULL DEFAULT false,
    ADD COLUMN approved_release    text,
    ADD COLUMN approval_note       text NOT NULL DEFAULT '',
    ADD COLUMN approval_changed_at timestamptz,
    -- Held off approval until an admin lets it go (`worlds unblock`): an implausible unlock rate, or by hand.
    ADD COLUMN approval_blocked    text,
    ADD COLUMN approval_blocked_at timestamptz;

-- Play time, per world and account, from the heartbeats' honest online lists (each counted wolf adds the time since
-- the world's last heartbeat, at most two minutes). Central's own evidence of who has actually played where.
CREATE TABLE central.play (
    world_id   text NOT NULL REFERENCES central.worlds (id),
    account_id text NOT NULL REFERENCES central.accounts (id) ON DELETE CASCADE,
    seconds    double precision NOT NULL DEFAULT 0,
    first_at   timestamptz NOT NULL,
    last_at    timestamptz NOT NULL,
    PRIMARY KEY (world_id, account_id)
);

-- The stars a world says were given there (its stars.json, sent with the heartbeat): giver and recipient wolves, the
-- kind, whether the world counted it, when central first heard of it (central's clock: what a giver's confirmation
-- is matched against), and whether central approved the world then (a star heard while it was not counts for
-- nothing on central: progress on an unapproved world stays there).
CREATE TABLE central.star_claims (
    world_id       text NOT NULL REFERENCES central.worlds (id),
    star_id        text NOT NULL,
    giver_wolf     text NOT NULL,
    recipient_wolf text NOT NULL,
    kind           text NOT NULL,
    counted        boolean NOT NULL,
    world_at       timestamptz,
    heard_at       timestamptz NOT NULL,
    approved       boolean NOT NULL,
    PRIMARY KEY (world_id, star_id)
);
CREATE INDEX star_claims_recipient ON central.star_claims (world_id, recipient_wolf);

-- What givers' own clients told central as they gave a star (POST /stars/confirm, signed in as the giver).
CREATE TABLE central.star_confirmations (
    id             bigserial PRIMARY KEY,
    world_id       text NOT NULL REFERENCES central.worlds (id),
    giver_account  text NOT NULL REFERENCES central.accounts (id) ON DELETE CASCADE,
    giver_wolf     text NOT NULL,
    recipient_wolf text NOT NULL,
    kind           text NOT NULL,
    at             timestamptz NOT NULL
);
CREATE INDEX star_confirmations_recipient ON central.star_confirmations (world_id, recipient_wolf, at);

-- Every unlock a world claimed while central took its word (official, or approved), one row per world and account:
-- first claimed, accepted (and how: 'official' or 'approved', by which release), what evidence is missing while it
-- waits, flagged for review (when the world lost approval, or by the rate rule), and revoked by an admin. Recorded,
-- never deleted.
CREATE TABLE central.unlocks (
    world_id         text NOT NULL REFERENCES central.worlds (id),
    account_id       text NOT NULL REFERENCES central.accounts (id) ON DELETE CASCADE,
    wolf_id          text NOT NULL,
    tier             text NOT NULL DEFAULT 'quickened',
    first_claimed_at timestamptz NOT NULL,
    accepted_at      timestamptz,
    how              text,
    release          text,
    note             text NOT NULL DEFAULT '',
    flagged_at       timestamptz,
    flag_reason      text NOT NULL DEFAULT '',
    revoked_at       timestamptz,
    revoked_note     text NOT NULL DEFAULT '',
    checked_at       timestamptz,             -- when its evidence was last looked at (while it waits)
    PRIMARY KEY (world_id, account_id, tier)
);
CREATE INDEX unlocks_claimed ON central.unlocks (world_id, first_claimed_at);
