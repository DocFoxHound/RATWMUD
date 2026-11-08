-- The server list (Docs/Design/77-central-accounts-and-hosted-worlds.md, section 7; Phase 4): host tokens, the
-- heartbeat, central's call-back and the honest player count, on the world registry of 0001.
--
-- A world is listed only while its heartbeat arrives (within 3 minutes) with a live host token, from an address
-- central could call back. Nothing here is filled in by hand: `address` and `port` are where the heartbeat came from
-- (never a field the host gives), `listing` is the heartbeat's own fields, `online` the wolf ids it named that central
-- issued tickets for, and `players` their count.

ALTER TABLE central.worlds
    -- The host token (its hash is 0001's token_hash): made, and last used by a heartbeat. One unused for 90 days is
    -- refused, and the world drops off the list. A new token replaces the old one.
    ADD COLUMN token_created timestamptz,
    ADD COLUMN token_used    timestamptz,
    -- Central's call-back to the world's /status (before first listing, every 10 minutes, and when the address
    -- changes): whether it answered as this world, when, why not (shown on the Host page), and the "ip:port" it
    -- called.
    ADD COLUMN reachable     boolean NOT NULL DEFAULT false,
    ADD COLUMN checked_at    timestamptz,
    ADD COLUMN check_note    text NOT NULL DEFAULT '',
    ADD COLUMN check_address text,
    -- The heartbeat's optional host name (listed instead of the address when it resolves to it), and the honest count.
    ADD COLUMN host_name     text,
    ADD COLUMN players       integer NOT NULL DEFAULT 0,
    -- Removed by its host from the Host page: delisted for good, its token dropped. The row stays, so its id is never
    -- reused and old tickets stay counted against it.
    ADD COLUMN removed       boolean NOT NULL DEFAULT false;

CREATE INDEX worlds_host ON central.worlds (host_account) WHERE NOT removed;

-- The honest count looks up the wolves a heartbeat names among the tickets issued for that world (kept 30 days now).
CREATE INDEX tickets_world_wolf ON central.tickets (world_id, wolf_id, issued_at);
