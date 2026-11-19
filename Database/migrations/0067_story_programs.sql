-- DM owns approval, deterministic runs and their cursors. The game only reads its normal dm.actions queue.
CREATE TABLE dm.story_releases (
    id text PRIMARY KEY, world_id ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    digest text NOT NULL, program jsonb NOT NULL, approved_by text NOT NULL,
    ticket_hash text NOT NULL UNIQUE, approved_at timestamptz NOT NULL DEFAULT now(),
    expires_at timestamptz NOT NULL DEFAULT now() + interval '1 day',
    status text NOT NULL CHECK (status IN ('approved','running','complete','paused','stopped')),
    state jsonb, cursor bigint NOT NULL DEFAULT 0, receipts bigint[] NOT NULL DEFAULT '{}',
    problem text NOT NULL DEFAULT ''
);
CREATE INDEX story_running ON dm.story_releases(world_id,status);
CREATE TABLE dm.story_transitions (
    run text REFERENCES dm.story_releases ON DELETE CASCADE, visit integer NOT NULL,
    phase text NOT NULL, game_day double precision NOT NULL, cause text NOT NULL, receipts bigint[] NOT NULL,
    PRIMARY KEY(run,visit)
);
CREATE TABLE dm.story_signals (
    ticket_hash text PRIMARY KEY, run text NOT NULL REFERENCES dm.story_releases ON DELETE CASCADE,
    visit integer NOT NULL, signal text NOT NULL, kind text NOT NULL DEFAULT 'signal' CHECK (kind IN ('signal','voice')),
    granted_by text NOT NULL, receipt bigint REFERENCES dm.actions, used boolean NOT NULL DEFAULT false,
    expires_at timestamptz NOT NULL DEFAULT now() + interval '1 hour'
);
REVOKE ALL ON dm.story_releases, dm.story_transitions, dm.story_signals FROM PUBLIC, ratw_game, ratw_editor, ratw_publisher;
GRANT SELECT, INSERT, UPDATE, DELETE ON dm.story_releases, dm.story_transitions, dm.story_signals TO ratw_dm;

-- Named residents and working roster characters have different authoring tables.
CREATE VIEW dm.story_residents AS
SELECT world_id,id,name,description,greeting,personality,backstory,home_area,work_area FROM live.npcs
UNION ALL
SELECT s.world_id,c.id,c.name,c.profile->>'description',c.profile->>'greeting',c.profile->>'personality',
       c.profile->>'backstory',s.home_area,s.work_area
FROM live.profession_slots s JOIN live.characters c ON c.status='active'
 AND c.assignment=jsonb_build_object('world',s.world_id,'slot',s.id);
REVOKE ALL ON dm.story_residents FROM PUBLIC, ratw_game, ratw_editor, ratw_publisher;
GRANT SELECT ON dm.story_residents TO ratw_dm;
