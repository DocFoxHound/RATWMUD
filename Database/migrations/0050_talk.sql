-- Talk (Docs/Design/62-talk.md, phase 6): each resident's voice, the dialects, families, and figures not yet in the
-- game. Live data (doc 20): Atlas edits DEV's, a push to live adds only rows PROD lacks, the Dungeon Master edits
-- PROD's, copying from live brings them back. The game reads them at start and on the DM's voice.sync.
--
-- live.npc_voice: one row a resident: where they grew up and their accent, their Ridgemere quarter, their traits and
--   quirk, the personality written for them (decision 10; worldgen's short one stays in live.npcs), and their own
--   grammar layer and rules, in the talk library's .grammar and .talk syntax (checked by talk_check before saving).
-- live.dialects: the regional grammars (decision 7), seeded from Data/Talk/grammar; plain stays a file.
-- live.npc_relations: families (decision 8): `npc` is `other`'s spouse, or `npc` is `other`'s parent.
-- live.figures: people residents know of who aren't in the game yet (decision 8), and the NPC each becomes.
CREATE TABLE live.npc_voice (
    world_id     ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    npc_id       ratw_id NOT NULL,
    home_region  text NOT NULL DEFAULT '' CHECK (home_region ~ '^[a-z_]{0,40}$'),
    accent       text NOT NULL DEFAULT '' CHECK (accent IN ('', 'none', 'light', 'broad')),
    quarter      text NOT NULL DEFAULT '' CHECK (quarter ~ '^[a-z_]{0,40}$'),
    traits       jsonb NOT NULL DEFAULT '{}' CHECK (jsonb_typeof(traits) = 'object'),
    quirk        text NOT NULL DEFAULT '' CHECK (quirk ~ '^[a-z_]{0,60}$'),
    persona      text NOT NULL DEFAULT '' CHECK (char_length(persona) <= 4000),
    grammar      text NOT NULL DEFAULT '' CHECK (char_length(grammar) <= 8000),
    rules        text NOT NULL DEFAULT '' CHECK (char_length(rules) <= 8000),
    written_by   text NOT NULL DEFAULT '' CHECK (char_length(written_by) <= 80),
    updated_at   timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, npc_id),
    FOREIGN KEY (world_id, npc_id) REFERENCES live.npcs (world_id, id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
COMMENT ON TABLE live.npc_voice IS 'Each resident''s voice (doc 62, phase 6): origin, accent, quarter, traits, quirk, personality, own grammar and rules.';

CREATE TABLE live.dialects (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    name        text NOT NULL CHECK (name ~ '^[a-z_]{1,40}$' AND name <> 'plain'),
    position    integer NOT NULL DEFAULT 0,
    grammar     text NOT NULL CHECK (char_length(grammar) <= 20000),
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, name)
);
COMMENT ON TABLE live.dialects IS 'The regional grammars (doc 62, decision 7), in the .grammar syntax; plain stays a file.';

CREATE TABLE live.npc_relations (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    npc_id      ratw_id NOT NULL,
    other_id    ratw_id NOT NULL CHECK (other_id <> npc_id),
    kind        text NOT NULL CHECK (kind IN ('spouse', 'parent')),
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, npc_id, other_id, kind),
    FOREIGN KEY (world_id, npc_id) REFERENCES live.npcs (world_id, id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED,
    FOREIGN KEY (world_id, other_id) REFERENCES live.npcs (world_id, id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
COMMENT ON TABLE live.npc_relations IS 'Families (doc 62, decision 8): npc is other''s spouse, or npc is other''s parent.';

CREATE TABLE live.figures (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    id          ratw_id NOT NULL,
    position    integer NOT NULL DEFAULT 0,
    name        text NOT NULL CHECK (char_length(name) BETWEEN 1 AND 120),
    title       text NOT NULL DEFAULT '' CHECK (char_length(title) <= 120),
    role        text NOT NULL DEFAULT '' CHECK (role ~ '^[a-z_]{0,40}$'),
    known_in    text NOT NULL DEFAULT 'realm' CHECK (known_in ~ '^(realm|region:[a-z_]+|house:[a-z_]+)$'),
    about       text NOT NULL DEFAULT '' CHECK (char_length(about) <= 600),
    npc_id      ratw_id,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, id)
);
COMMENT ON TABLE live.figures IS 'People residents know of who aren''t in the game yet (doc 62, decision 8); npc_id once they arrive.';
