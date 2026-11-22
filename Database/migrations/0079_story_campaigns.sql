-- 0079_story_campaigns.sql: campaigns made of chapters, and the world's own declared variables
-- (Docs/Design/85-story-triggers-and-campaign-control.md, 3.5 "story.start", 3.2 the `world` scope, and Phase 10).
-- Additive: nothing that exists is rewritten.
--
-- A campaign is several world stories approved together, in order: its chapters. The first is published with a ticket as
-- any story is; the others wait here, approved with it, until a chapter before them starts them (the story effect
-- `story.start`, carried out by the story runner: no ticket, with the audience and the values the effect names). A chapter
-- that has started has a run (dm.story_releases.campaign and .chapter name it). Approving the campaign again under the
-- same id replaces the chapters that have not started; those that have keep their runs.
CREATE TABLE dm.story_campaigns (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    id          text NOT NULL CHECK (id ~ '^[A-Za-z0-9_][A-Za-z0-9_.:-]{0,79}$'),
    title       text NOT NULL,
    -- [{story, title, digest, program, approvedBy}], in order.
    chapters    jsonb NOT NULL,
    approved_by text NOT NULL,
    approved_at timestamptz NOT NULL DEFAULT now(),
    status      text NOT NULL DEFAULT 'approved' CHECK (status IN ('approved', 'running', 'ended', 'withdrawn')),
    PRIMARY KEY (world_id, id)
);

ALTER TABLE dm.story_releases ADD COLUMN campaign text;
ALTER TABLE dm.story_releases ADD COLUMN chapter integer;
CREATE INDEX story_releases_campaign ON dm.story_releases (world_id, campaign) WHERE campaign IS NOT NULL;

-- The world's variables, declared for the world (not only by the stories that use them): a type, a first value and a
-- note, kept by the Campaigns console, where an admin may also set a value (audited, `story.world-var`). A story that
-- declares a world variable of that name reads and writes the world's value; one declaring it with another type is
-- refused at approval. A row with `declared` false is a value a story wrote with no declaration for the world (Phase 5).
ALTER TABLE dm.story_world_vars ADD COLUMN declared boolean NOT NULL DEFAULT false;
ALTER TABLE dm.story_world_vars ADD COLUMN initial jsonb;
ALTER TABLE dm.story_world_vars ADD COLUMN note text NOT NULL DEFAULT '';
ALTER TABLE dm.story_world_vars ADD COLUMN declared_by text;
ALTER TABLE dm.story_world_vars ADD COLUMN updated_by text;

REVOKE ALL ON dm.story_campaigns FROM PUBLIC, ratw_game, ratw_editor, ratw_publisher;
GRANT SELECT, INSERT, UPDATE, DELETE ON dm.story_campaigns TO ratw_dm;
