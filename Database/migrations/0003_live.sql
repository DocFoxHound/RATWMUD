-- Live simulation data. The game server changes these rows while it runs; the
-- editor places and edits seeds in DEV, and Push to live adds new seeds to PROD
-- without overwriting rows the live server has changed.
--
-- Anything placed in the world names an area plus an area-local tile. Those
-- references are checked at commit, so Push to live cannot remove a cell or
-- interior that live data still stands in.

-- The shared profession catalog and character roster (formerly Data/Characters/roster.json).
CREATE TABLE live.professions (
    id           ratw_id PRIMARY KEY,
    name         text NOT NULL CHECK (char_length(name) BETWEEN 1 AND 120),
    behavior     text NOT NULL CHECK (behavior IN ('merchant', 'guard', 'civilian')),
    work_label   text NOT NULL DEFAULT '' CHECK (char_length(work_label) <= 40),
    description  text NOT NULL DEFAULT '',
    hours_start  real NOT NULL CHECK (hours_start BETWEEN 0 AND 23.75),
    hours_end    real NOT NULL CHECK (hours_end BETWEEN 0 AND 23.75),
    paid         boolean NOT NULL DEFAULT true,
    CHECK (hours_start <> hours_end)
);

-- Profile fields (appearance, voice, personality, traits, backstory, greeting,
-- job preferences) stay in one document while NPC design is still changing.
CREATE TABLE live.characters (
    id          ratw_id PRIMARY KEY,
    name        text NOT NULL CHECK (char_length(name) BETWEEN 1 AND 120),
    age         integer NOT NULL CHECK (age BETWEEN 0 AND 200),
    status      text NOT NULL DEFAULT 'active',
    profession  ratw_id REFERENCES live.professions ON DELETE SET NULL,
    origin      text NOT NULL DEFAULT 'authored',
    profile     jsonb NOT NULL DEFAULT '{}' CHECK (jsonb_typeof(profile) = 'object'),
    updated_at  timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE live.patrol_routes (
    world_id  ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    id        ratw_id NOT NULL,
    name      text NOT NULL CHECK (char_length(name) BETWEEN 1 AND 120),
    PRIMARY KEY (world_id, id)
);

-- A route is an ordered loop of posts.
CREATE TABLE live.patrol_posts (
    world_id  ratw_id NOT NULL,
    route_id  ratw_id NOT NULL,
    seq       integer NOT NULL CHECK (seq >= 0),
    area      ratw_id NOT NULL,
    x         integer NOT NULL CHECK (x >= 0),
    y         integer NOT NULL CHECK (y >= 0),
    PRIMARY KEY (world_id, route_id, seq),
    FOREIGN KEY (world_id, route_id) REFERENCES live.patrol_routes ON DELETE CASCADE,
    FOREIGN KEY (world_id, area) REFERENCES world.areas (world_id, id) DEFERRABLE INITIALLY DEFERRED
);

-- Named NPCs designed for one world.
CREATE TABLE live.npcs (
    world_id      ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    id            ratw_id NOT NULL,
    name          text NOT NULL CHECK (char_length(name) BETWEEN 1 AND 120),
    role          text NOT NULL CHECK (role IN ('merchant', 'guard', 'civilian')),
    description   text NOT NULL DEFAULT '' CHECK (char_length(description) <= 4000),
    greeting      text NOT NULL DEFAULT '' CHECK (char_length(greeting) <= 1024),
    personality   text NOT NULL DEFAULT '' CHECK (char_length(personality) <= 2000),
    backstory     text NOT NULL DEFAULT '' CHECK (char_length(backstory) <= 6000),
    work_label    text NOT NULL DEFAULT '' CHECK (char_length(work_label) <= 40),
    age           integer NOT NULL CHECK (age BETWEEN 0 AND 200),
    voice         integer NOT NULL CHECK (voice BETWEEN 0 AND 31),
    appearance    jsonb NOT NULL CHECK (jsonb_typeof(appearance) = 'object'),
    route_id      ratw_id,
    paid          boolean NOT NULL DEFAULT true,
    purse         integer NOT NULL DEFAULT 0 CHECK (purse BETWEEN 0 AND 100000),
    herbs         integer NOT NULL DEFAULT 0 CHECK (herbs BETWEEN 0 AND 10000),
    meals         integer NOT NULL DEFAULT 0 CHECK (meals BETWEEN 0 AND 10000),
    hours_start   real NOT NULL CHECK (hours_start BETWEEN 0 AND 23.75),
    hours_end     real NOT NULL CHECK (hours_end BETWEEN 0 AND 23.75),
    home_area     ratw_id NOT NULL, home_x integer NOT NULL, home_y integer NOT NULL,
    work_area     ratw_id NOT NULL, work_x integer NOT NULL, work_y integer NOT NULL,
    evening_area  ratw_id NOT NULL, evening_x integer NOT NULL, evening_y integer NOT NULL,
    -- 'authored' rows came from the editor; the server marks rows it has changed 'runtime'.
    origin        text NOT NULL DEFAULT 'authored' CHECK (origin IN ('authored', 'runtime')),
    updated_at    timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, id),
    FOREIGN KEY (world_id, route_id) REFERENCES live.patrol_routes ON DELETE SET NULL (route_id),
    FOREIGN KEY (world_id, home_area) REFERENCES world.areas (world_id, id) DEFERRABLE INITIALLY DEFERRED,
    FOREIGN KEY (world_id, work_area) REFERENCES world.areas (world_id, id) DEFERRABLE INITIALLY DEFERRED,
    FOREIGN KEY (world_id, evening_area) REFERENCES world.areas (world_id, id) DEFERRABLE INITIALLY DEFERRED,
    CHECK (hours_start <> hours_end)
);

-- Jobs placed in a world and filled from the character roster.
CREATE TABLE live.profession_slots (
    world_id      ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    id            ratw_id NOT NULL,
    name          text NOT NULL CHECK (char_length(name) BETWEEN 1 AND 120),
    profession    ratw_id NOT NULL REFERENCES live.professions,
    work_label    text NOT NULL DEFAULT '' CHECK (char_length(work_label) <= 40),
    route_id      ratw_id,
    paid          boolean NOT NULL DEFAULT true,
    purse         integer NOT NULL DEFAULT 0 CHECK (purse BETWEEN 0 AND 100000),
    herbs         integer NOT NULL DEFAULT 0 CHECK (herbs BETWEEN 0 AND 10000),
    meals         integer NOT NULL DEFAULT 0 CHECK (meals BETWEEN 0 AND 10000),
    hours_start   real NOT NULL CHECK (hours_start BETWEEN 0 AND 23.75),
    hours_end     real NOT NULL CHECK (hours_end BETWEEN 0 AND 23.75),
    home_area     ratw_id NOT NULL, home_x integer NOT NULL, home_y integer NOT NULL,
    work_area     ratw_id NOT NULL, work_x integer NOT NULL, work_y integer NOT NULL,
    evening_area  ratw_id NOT NULL, evening_x integer NOT NULL, evening_y integer NOT NULL,
    -- The roster character permanently assigned to this job, once filled.
    character_id  ratw_id REFERENCES live.characters ON DELETE SET NULL,
    origin        text NOT NULL DEFAULT 'authored' CHECK (origin IN ('authored', 'runtime')),
    updated_at    timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, id),
    FOREIGN KEY (world_id, route_id) REFERENCES live.patrol_routes ON DELETE SET NULL (route_id),
    FOREIGN KEY (world_id, home_area) REFERENCES world.areas (world_id, id) DEFERRABLE INITIALLY DEFERRED,
    FOREIGN KEY (world_id, work_area) REFERENCES world.areas (world_id, id) DEFERRABLE INITIALLY DEFERRED,
    FOREIGN KEY (world_id, evening_area) REFERENCES world.areas (world_id, id) DEFERRABLE INITIALLY DEFERRED,
    CHECK (hours_start <> hours_end)
);

CREATE TABLE live.economy (
    world_id     ratw_id PRIMARY KEY REFERENCES world.worlds ON DELETE CASCADE,
    treasury     integer NOT NULL DEFAULT 0 CHECK (treasury BETWEEN 0 AND 1000000),
    store_herbs  integer NOT NULL DEFAULT 0 CHECK (store_herbs BETWEEN 0 AND 10000),
    store_meals  integer NOT NULL DEFAULT 0 CHECK (store_meals BETWEEN 0 AND 10000),
    daily_herbs  integer NOT NULL DEFAULT 0 CHECK (daily_herbs BETWEEN 0 AND 1000),
    daily_meals  integer NOT NULL DEFAULT 0 CHECK (daily_meals BETWEEN 0 AND 1000),
    updated_at   timestamptz NOT NULL DEFAULT now()
);
