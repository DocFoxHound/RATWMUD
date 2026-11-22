-- 0071_story_commands.sql: steering a running campaign (Docs/Design/85-story-triggers-and-campaign-control.md, Phase 2,
-- "Steer it"). Additive: no row is rewritten, and every new column has a default that changes nothing for what exists.
--
-- Every intervention is a command in an inbox that the story runner carries out (doc 85, 5.1 rule 3), first in each tick,
-- in order, inside the same transaction as the trigger history: the state keeps one writer, and a command lands between
-- two world events. A command carries what it was made against (the run's visit and digest: Phase 5b moves runs onto
-- new versions), a reason, who asked, and once taken its result with the values before and after.
CREATE TABLE dm.story_commands (
    id           bigserial PRIMARY KEY,
    run          text NOT NULL REFERENCES dm.story_releases ON DELETE CASCADE,
    -- The console's own id for the command: sending it twice does it once (5.8, rule 3).
    key          text NOT NULL CHECK (key ~ '^[A-Za-z0-9_-]{8,64}$'),
    visit        integer,
    digest       text NOT NULL DEFAULT '',
    kind         text NOT NULL CHECK (kind ~ '^[a-z][a-z-]{1,31}$'),
    payload      jsonb NOT NULL DEFAULT '{}',
    reason       text NOT NULL,
    requested_by text NOT NULL,
    requested_at timestamptz NOT NULL DEFAULT now(),
    status       text NOT NULL DEFAULT 'queued' CHECK (status IN ('queued', 'done', 'refused')),
    result       jsonb,
    done_at      timestamptz,
    UNIQUE (run, key)
);
CREATE INDEX story_commands_queued ON dm.story_commands (run, id) WHERE status = 'queued';
CREATE INDEX story_commands_run ON dm.story_commands (run, id DESC);

-- The runner's heartbeat, one row per world, written every tick (5.6: "the runner is not running").
CREATE TABLE dm.story_runner (
    world_id   ratw_id PRIMARY KEY REFERENCES world.worlds ON DELETE CASCADE,
    beat_at    timestamptz NOT NULL DEFAULT now(),
    started_at timestamptz NOT NULL DEFAULT now(),
    pid        integer NOT NULL DEFAULT 0,
    version    text NOT NULL DEFAULT '',
    behind     bigint NOT NULL DEFAULT 0
);

REVOKE ALL ON dm.story_commands, dm.story_runner FROM PUBLIC, ratw_game, ratw_editor, ratw_publisher;
GRANT SELECT, INSERT, UPDATE ON dm.story_commands TO ratw_dm;
GRANT USAGE ON SEQUENCE dm.story_commands_id_seq TO ratw_dm;
GRANT SELECT, INSERT, UPDATE, DELETE ON dm.story_runner TO ratw_dm;

-- Held: an admin's pause (reads no events, starts no effects, fires no timers). Stopping: stop was sent, and the game has
-- not yet acknowledged story.stop (known problem 10). `paused` keeps meaning "the game refused something".
ALTER TABLE dm.story_releases DROP CONSTRAINT story_releases_status_check;
ALTER TABLE dm.story_releases ADD CONSTRAINT story_releases_status_check
    CHECK (status IN ('approved', 'running', 'complete', 'paused', 'held', 'stopping', 'stopped'));

-- "May steer campaigns" (doc 85, 5.7; the user's answer to question 3, 2026-11-21): on a hosted world the owner always may;
-- any other account only when the owner has given it this, as "may act on players' wolves" (0063).
ALTER TABLE dm.admins ADD COLUMN may_steer_campaigns boolean NOT NULL DEFAULT false;
