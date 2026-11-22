-- 0073_story_drafts.sql: world-story drafts kept on the server (Docs/Design/85-story-triggers-and-campaign-control.md,
-- Phase 4, 4.1 decision 5): many per world, each with a name, an owner and its saved versions, so a Dungeon Master's
-- draft is no longer one text in one browser. The browser keeps only the edits not yet saved; a viewer's drafts stay in
-- their browser, as before. Additive: nothing that exists is changed.
--
-- The program is `json`, not `jsonb`, so a draft comes back with the author's own key order (jsonb sorts keys), and the
-- Source tab reads as it was written. A save names the version it was made on: a save over a newer one is refused, so
-- two Dungeon Masters editing one draft never overwrite each other unawares.
CREATE TABLE dm.story_drafts (
    id         bigserial PRIMARY KEY,
    world_id   ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    name       text NOT NULL CHECK (length(btrim(name)) BETWEEN 1 AND 120),
    owner      text NOT NULL,
    program    json NOT NULL CHECK (json_typeof(program) = 'object'),
    version    integer NOT NULL DEFAULT 1 CHECK (version >= 1),
    saved_by   text NOT NULL,
    saved_at   timestamptz NOT NULL DEFAULT now(),
    created_at timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX story_drafts_world ON dm.story_drafts (world_id, saved_at DESC);

-- Each saved version of a draft (the newest 20 are kept), to look back at or open again.
CREATE TABLE dm.story_draft_versions (
    draft    bigint NOT NULL REFERENCES dm.story_drafts ON DELETE CASCADE,
    version  integer NOT NULL,
    program  json NOT NULL,
    saved_by text NOT NULL,
    saved_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (draft, version)
);

REVOKE ALL ON dm.story_drafts, dm.story_draft_versions FROM PUBLIC, ratw_game, ratw_editor, ratw_publisher;
GRANT SELECT, INSERT, UPDATE, DELETE ON dm.story_drafts, dm.story_draft_versions TO ratw_dm;
GRANT USAGE ON SEQUENCE dm.story_drafts_id_seq TO ratw_dm;
