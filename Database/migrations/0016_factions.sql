-- Factions become live data (Dungeon Master phase 4).
--
-- Atlas plans factions and territory on DEV; a push adds only new ones, and once
-- live the Dungeon Master runs them in PROD. Territory claims move off the cells
-- and interiors into their own rows, and can be painted tile by tile:
--   live.factions          every faction: NPC factions, cities, guilds and clans
--   live.faction_claims    a faction's claim on one cell or interior: its tiles, or the whole place when empty
--   live.faction_relations how one faction regards another (-100..100 and a stance), with the reason
--   live.faction_relation_log   every change to a relation, never pushed or copied
--   live.faction_members   named NPCs who belong to a faction, with a rank
-- A claim is not control (design doc 16). Several factions may claim the same tiles.

CREATE TABLE live.factions (
    world_id     ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    id           ratw_id NOT NULL,
    position     integer NOT NULL DEFAULT 0,
    name         text NOT NULL CHECK (char_length(name) BETWEEN 1 AND 120),
    color        text NOT NULL DEFAULT '#a8c7ad' CHECK (color ~ '^#[0-9a-fA-F]{6}$'),
    kind         text NOT NULL DEFAULT 'npc' CHECK (kind IN ('npc', 'city', 'guild', 'clan', 'other')),
    description  text NOT NULL DEFAULT '' CHECK (char_length(description) <= 4000),
    origin       text NOT NULL DEFAULT 'authored' CHECK (origin IN ('authored', 'runtime')),
    updated_at   timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, id)
);
INSERT INTO live.factions (world_id, id, position, name, color)
    SELECT world_id, id, position, name, color FROM world.factions;

CREATE TABLE live.faction_claims (
    world_id    ratw_id NOT NULL,
    faction_id  ratw_id NOT NULL,
    area        ratw_id NOT NULL,
    -- Place-local tiles [[x, y], ...]; empty claims the whole cell or interior.
    tiles       jsonb NOT NULL DEFAULT '[]' CHECK (jsonb_typeof(tiles) = 'array' AND jsonb_array_length(tiles) <= 4096),
    origin      text NOT NULL DEFAULT 'authored' CHECK (origin IN ('authored', 'runtime')),
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, faction_id, area),
    FOREIGN KEY (world_id, faction_id) REFERENCES live.factions ON DELETE CASCADE,
    FOREIGN KEY (world_id, area) REFERENCES world.areas (world_id, id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
INSERT INTO live.faction_claims (world_id, faction_id, area)
    SELECT world_id, unnest(claims), id FROM world.cells
    UNION SELECT world_id, unnest(claims), id FROM world.interiors;

ALTER TABLE world.cells DROP COLUMN claims;
ALTER TABLE world.interiors DROP COLUMN claims;
DROP TABLE world.factions;

CREATE TABLE live.faction_relations (
    world_id     ratw_id NOT NULL,
    faction_id   ratw_id NOT NULL,
    other_id     ratw_id NOT NULL,
    disposition  integer NOT NULL DEFAULT 0 CHECK (disposition BETWEEN -100 AND 100),
    stance       text NOT NULL DEFAULT 'neutral' CHECK (stance IN ('allied', 'friendly', 'neutral', 'tense', 'hostile', 'war')),
    reason       text NOT NULL DEFAULT '' CHECK (char_length(reason) <= 1000),
    updated_by   text NOT NULL DEFAULT '',
    origin       text NOT NULL DEFAULT 'authored' CHECK (origin IN ('authored', 'runtime')),
    updated_at   timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, faction_id, other_id),
    CHECK (faction_id <> other_id),
    FOREIGN KEY (world_id, faction_id) REFERENCES live.factions ON DELETE CASCADE,
    FOREIGN KEY (world_id, other_id) REFERENCES live.factions ON DELETE CASCADE
);

CREATE TABLE live.faction_relation_log (
    id           bigserial PRIMARY KEY,
    world_id     ratw_id NOT NULL,
    faction_id   ratw_id NOT NULL,
    other_id     ratw_id NOT NULL,
    disposition  integer NOT NULL,
    stance       text NOT NULL,
    reason       text NOT NULL,
    by           text NOT NULL,
    at           timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX ON live.faction_relation_log (world_id, faction_id, other_id, id DESC);

CREATE TABLE live.faction_members (
    world_id    ratw_id NOT NULL,
    faction_id  ratw_id NOT NULL,
    npc_id      ratw_id NOT NULL,
    rank        text NOT NULL DEFAULT '' CHECK (char_length(rank) <= 60),
    origin      text NOT NULL DEFAULT 'authored' CHECK (origin IN ('authored', 'runtime')),
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, faction_id, npc_id),
    FOREIGN KEY (world_id, faction_id) REFERENCES live.factions ON DELETE CASCADE,
    FOREIGN KEY (world_id, npc_id) REFERENCES live.npcs ON DELETE CASCADE
);

GRANT USAGE, SELECT ON SEQUENCE live.faction_relation_log_id_seq TO ratw_dm, ratw_editor, ratw_publisher, ratw_game;

-- The game's NPC records now also carry the factions and every place's claims, so Dungeon Master changes reach a
-- running server: faction "id" "name" "#color", and claims "area" N "faction"... (the server drops the build's own).
CREATE OR REPLACE FUNCTION live.people_manifest(p_world ratw_id, p_npc text DEFAULT NULL) RETURNS text
LANGUAGE sql STABLE AS $$
    WITH people AS (
        SELECT 0 AS kind, n.position, n.id, n.name, n.role, n.work_label, n.description, n.greeting, n.age, n.appearance,
               n.voice, n.paid, n.hours_start, n.hours_end, n.route_id, n.purse, n.herbs, n.meals,
               n.home_area, n.home_x, n.home_y, n.work_area, n.work_x, n.work_y, n.evening_area, n.evening_x, n.evening_y,
               n.personality, n.backstory, n.wander_area
        FROM live.npcs n WHERE n.world_id = p_world
        UNION ALL
        SELECT 1, s.position, c.id, c.name, p.behavior, coalesce(nullif(s.work_label, ''), p.work_label),
               coalesce(c.profile->>'description', ''), coalesce(c.profile->>'greeting', ''), c.age, c.profile->'appearance',
               (c.profile->>'voice')::integer, s.paid, s.hours_start, s.hours_end, s.route_id, s.purse, s.herbs, s.meals,
               s.home_area, s.home_x, s.home_y, s.work_area, s.work_x, s.work_y, s.evening_area, s.evening_x, s.evening_y,
               trim(coalesce(c.profile->>'personality', '') ||
                    CASE WHEN jsonb_array_length(coalesce(c.profile->'traits', '[]')) > 0
                         THEN ' Traits: ' || (SELECT string_agg(t, ', ') FROM jsonb_array_elements_text(c.profile->'traits') t) || '.'
                         ELSE '' END),
               coalesce(c.profile->>'backstory', ''), NULL
        FROM live.profession_slots s
        JOIN live.professions p ON p.id = s.profession
        JOIN live.characters c ON c.status = 'active' AND c.assignment = jsonb_build_object('world', s.world_id, 'slot', s.id)
        WHERE s.world_id = p_world
    ),
    lines AS (
        SELECT 0 AS part, 0 AS kind, 0 AS position, 0 AS line, 'economy ' || concat_ws(' ', treasury, store_herbs, store_meals, daily_herbs, daily_meals) AS text
        FROM live.economy WHERE world_id = p_world AND p_npc IS NULL
        UNION ALL
        SELECT 1, 0, r.position, 0, 'route ' || live.q(r.id) || ' ' || count(pp.*) || ' ' || string_agg(live.at(pp.area, pp.x, pp.y), ' ' ORDER BY pp.seq)
        FROM live.patrol_routes r JOIN live.patrol_posts pp ON pp.world_id = r.world_id AND pp.route_id = r.id
        WHERE r.world_id = p_world AND p_npc IS NULL
        GROUP BY r.id, r.position
        UNION ALL
        SELECT 2, kind, position, 0, concat_ws(' ', 'resident', live.q(id), live.q(name), live.q(role), live.q(work_label),
               live.q(replace(description, E'\n', ' ')), live.q(replace(greeting, E'\n', ' ')), age,
               live.q(appearance->>'species'), live.q(appearance->>'sex'), live.q(appearance->>'stature'), live.q(appearance->>'pattern'),
               appearance->>'baseColor', appearance->>'gradientColor', appearance->>'markingColor', voice, paid::integer,
               hours_start::text, hours_end::text, live.q(coalesce(route_id, '-')), purse, herbs, meals,
               live.at(home_area, home_x, home_y), live.at(work_area, work_x, work_y), live.at(evening_area, evening_x, evening_y))
        FROM people WHERE p_npc IS NULL OR id = p_npc
        UNION ALL
        SELECT 2, kind, position, 1, 'story ' || live.q(id) || ' ' || live.q(replace(personality, E'\n', ' ')) || ' ' || live.q(replace(backstory, E'\n', ' '))
        FROM people WHERE (p_npc IS NULL OR id = p_npc) AND (personality <> '' OR backstory <> '')
        UNION ALL
        SELECT 3, pe.kind, pe.position, 0, 'wander ' || live.q(pe.id) || ' ' || jsonb_array_length(a.tiles) || ' ' ||
               (SELECT string_agg(live.at(a.area, (t->>0)::integer, (t->>1)::integer), ' ' ORDER BY n) FROM jsonb_array_elements(a.tiles) WITH ORDINALITY AS x(t, n))
        FROM people pe JOIN live.npc_areas a ON a.world_id = p_world AND a.id = pe.wander_area
        WHERE (p_npc IS NULL OR pe.id = p_npc) AND jsonb_array_length(a.tiles) > 0
        UNION ALL
        SELECT 4, 0, f.position, 0, 'faction ' || live.q(f.id) || ' ' || live.q(f.name) || ' ' || live.q(f.color)
        FROM live.factions f WHERE f.world_id = p_world AND p_npc IS NULL
        UNION ALL
        SELECT 5, 0, 0, 0, 'claims ' || live.q(c.area) || ' ' || count(*) || ' ' || string_agg(live.q(c.faction_id), ' ' ORDER BY c.faction_id)
        FROM live.faction_claims c WHERE c.world_id = p_world AND p_npc IS NULL
        GROUP BY c.area
    )
    SELECT coalesce(string_agg(text, E'\n' ORDER BY part, kind, position, line, text), '') FROM lines
$$;
