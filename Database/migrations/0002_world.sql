-- Authored world content. See Docs/Design/20-world-database.md.
--
-- Terrain is one continuous landscape stored in 32x32 chunks, so the world can
-- grow in any direction. World cells are rectangles over it (metadata only).
-- Interiors are detached maps that own their own tiles. Cells and interiors
-- share one ID space, "areas", which everything placed in the world refers to.

CREATE TABLE world.worlds (
    id          ratw_id PRIMARY KEY,
    name        text NOT NULL CHECK (char_length(name) BETWEEN 1 AND 120),
    spawn_area  ratw_id,
    spawn_x     integer,
    spawn_y     integer,
    revision    bigint NOT NULL DEFAULT 0,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    CHECK ((spawn_area IS NULL) = (spawn_x IS NULL) AND (spawn_x IS NULL) = (spawn_y IS NULL))
);

-- 32x32 tiles, row-major, one glyph per tile. Missing chunks are unpainted ground.
-- heights: sparse elevation overrides keyed "x,y" in chunk-local tiles.
CREATE TABLE world.terrain_chunks (
    world_id  ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    cx        integer NOT NULL,
    cy        integer NOT NULL,
    glyphs    text NOT NULL CHECK (char_length(glyphs) = 1024 AND glyphs ~ '^[.#,"T=~:^+]*$'),
    heights   jsonb NOT NULL DEFAULT '{}' CHECK (jsonb_typeof(heights) = 'object'),
    PRIMARY KEY (world_id, cx, cy)
);

CREATE TABLE world.areas (
    world_id  ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    id        ratw_id NOT NULL,
    kind      text NOT NULL CHECK (kind IN ('cell', 'interior')),
    PRIMARY KEY (world_id, id),
    UNIQUE (world_id, id, kind)
);

ALTER TABLE world.worlds ADD FOREIGN KEY (id, spawn_area) REFERENCES world.areas (world_id, id) DEFERRABLE INITIALLY DEFERRED;

-- Columns every cell and interior share: scene text, level, weather, ambient light, territory.
CREATE TABLE world.cells (
    world_id          ratw_id NOT NULL,
    id                ratw_id NOT NULL,
    kind              text NOT NULL DEFAULT 'cell' CHECK (kind = 'cell'),
    name              text NOT NULL CHECK (char_length(name) BETWEEN 1 AND 120),
    description       text NOT NULL DEFAULT '' CHECK (char_length(description) <= 4000),
    x                 integer NOT NULL,
    y                 integer NOT NULL,
    width             integer NOT NULL CHECK (width BETWEEN 4 AND 256),
    height            integer NOT NULL CHECK (height BETWEEN 4 AND 256),
    z                 integer NOT NULL DEFAULT 0 CHECK (z BETWEEN -16 AND 16),
    outdoors          boolean NOT NULL DEFAULT true,
    weather           text NOT NULL DEFAULT 'clear' CHECK (weather IN ('clear', 'rain', 'fog', 'snow')),
    light_artificial  real NOT NULL DEFAULT 0 CHECK (light_artificial BETWEEN 0 AND 1),
    light_daylight    real NOT NULL DEFAULT 1 CHECK (light_daylight BETWEEN 0 AND 1),
    light_tone        text NOT NULL DEFAULT 'neutral' CHECK (light_tone IN ('warm', 'neutral', 'cool')),
    region            text NOT NULL DEFAULT 'unassigned',
    chapter           text NOT NULL DEFAULT '',
    claims            text[] NOT NULL DEFAULT '{}',
    PRIMARY KEY (world_id, id),
    FOREIGN KEY (world_id, id, kind) REFERENCES world.areas (world_id, id, kind) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED,
    -- World cells never overlap (touching edges is fine).
    EXCLUDE USING gist (world_id WITH =,
        box(point(x, y), point(x + width - 1, y + height - 1)) WITH &&)
);

CREATE TABLE world.interiors (
    world_id          ratw_id NOT NULL,
    id                ratw_id NOT NULL,
    kind              text NOT NULL DEFAULT 'interior' CHECK (kind = 'interior'),
    name              text NOT NULL CHECK (char_length(name) BETWEEN 1 AND 120),
    description       text NOT NULL DEFAULT '' CHECK (char_length(description) <= 4000),
    width             integer NOT NULL CHECK (width BETWEEN 4 AND 256),
    height            integer NOT NULL CHECK (height BETWEEN 4 AND 256),
    -- Row-major glyphs, width x height.
    glyphs            text NOT NULL CHECK (glyphs ~ '^[.#,"T=~:^+]*$'),
    heights           jsonb NOT NULL DEFAULT '{}' CHECK (jsonb_typeof(heights) = 'object'),
    z                 integer NOT NULL DEFAULT 0 CHECK (z BETWEEN -16 AND 16),
    outdoors          boolean NOT NULL DEFAULT false,
    weather           text NOT NULL DEFAULT 'clear' CHECK (weather IN ('clear', 'rain', 'fog', 'snow')),
    light_artificial  real NOT NULL DEFAULT 1 CHECK (light_artificial BETWEEN 0 AND 1),
    light_daylight    real NOT NULL DEFAULT 0 CHECK (light_daylight BETWEEN 0 AND 1),
    light_tone        text NOT NULL DEFAULT 'warm' CHECK (light_tone IN ('warm', 'neutral', 'cool')),
    region            text NOT NULL DEFAULT 'unassigned',
    chapter           text NOT NULL DEFAULT '',
    claims            text[] NOT NULL DEFAULT '{}',
    -- Where the interior appears on the in-game world map.
    overview_x        integer NOT NULL DEFAULT 0,
    overview_y        integer NOT NULL DEFAULT 0,
    PRIMARY KEY (world_id, id),
    FOREIGN KEY (world_id, id, kind) REFERENCES world.areas (world_id, id, kind) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED,
    CHECK (char_length(glyphs) = width * height)
);

-- Doors, passages and stairs between two different areas. Tiles are area-local.
CREATE TABLE world.links (
    world_id  ratw_id NOT NULL,
    id        ratw_id NOT NULL,
    name      text NOT NULL CHECK (char_length(name) BETWEEN 1 AND 120),
    kind      text NOT NULL CHECK (kind IN ('door', 'passage', 'stairs')),
    open      boolean NOT NULL DEFAULT true,
    a_area    ratw_id NOT NULL,
    a_x       integer NOT NULL CHECK (a_x >= 0),
    a_y       integer NOT NULL CHECK (a_y >= 0),
    b_area    ratw_id NOT NULL,
    b_x       integer NOT NULL CHECK (b_x >= 0),
    b_y       integer NOT NULL CHECK (b_y >= 0),
    PRIMARY KEY (world_id, id),
    FOREIGN KEY (world_id, a_area) REFERENCES world.areas (world_id, id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED,
    FOREIGN KEY (world_id, b_area) REFERENCES world.areas (world_id, id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED,
    CHECK (a_area <> b_area)
);

-- Resource nodes players interact with, such as the herb patch.
CREATE TABLE world.resources (
    world_id  ratw_id NOT NULL,
    id        ratw_id NOT NULL,
    kind      text NOT NULL CHECK (kind IN ('herb')),
    area      ratw_id NOT NULL,
    x         integer NOT NULL CHECK (x >= 0),
    y         integer NOT NULL CHECK (y >= 0),
    PRIMARY KEY (world_id, id),
    FOREIGN KEY (world_id, area) REFERENCES world.areas (world_id, id) DEFERRABLE INITIALLY DEFERRED
);

-- Named spots other data can point at: beds, counters, work posts, waypoints.
CREATE TABLE world.places (
    world_id  ratw_id NOT NULL,
    id        ratw_id NOT NULL,
    name      text NOT NULL CHECK (char_length(name) BETWEEN 1 AND 120),
    kind      text NOT NULL DEFAULT 'spot' CHECK (kind ~ '^[a-z][a-z0-9_-]{0,31}$'),
    area      ratw_id NOT NULL,
    x         integer NOT NULL CHECK (x >= 0),
    y         integer NOT NULL CHECK (y >= 0),
    PRIMARY KEY (world_id, id),
    FOREIGN KEY (world_id, area) REFERENCES world.areas (world_id, id) DEFERRABLE INITIALLY DEFERRED
);

CREATE TABLE world.factions (
    world_id  ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    id        ratw_id NOT NULL,
    name      text NOT NULL CHECK (char_length(name) BETWEEN 1 AND 120),
    color     text NOT NULL DEFAULT '#a8c7ad' CHECK (color ~ '^#[0-9a-fA-F]{6}$'),
    PRIMARY KEY (world_id, id)
);

CREATE TABLE world.chapters (
    world_id  ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    id        ratw_id NOT NULL,
    name      text NOT NULL CHECK (char_length(name) BETWEEN 1 AND 120),
    PRIMARY KEY (world_id, id)
);

-- Small per-cell picture for the player's minimap, regenerated when the cell's terrain is saved.
CREATE TABLE world.minimaps (
    world_id    ratw_id NOT NULL,
    area        ratw_id NOT NULL,
    width       integer NOT NULL CHECK (width > 0),
    height      integer NOT NULL CHECK (height > 0),
    image       bytea NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, area),
    FOREIGN KEY (world_id, area) REFERENCES world.areas (world_id, id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
