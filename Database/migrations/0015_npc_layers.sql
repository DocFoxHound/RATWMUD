-- NPC layers (Dungeon Master phase 3): painted NPC areas and spawn rules.
--
-- An area is a set of tiles in one cell or interior. Kinds:
--   wander  NPCs assigned to it roam its open tiles in their free time (civilians: work hours and evenings)
--   spawn   where a spawn rule brings NPCs into the world
--   plan    a marked zone for planning; no effect in play yet
-- A spawn rule keeps `count` NPCs made from a template NPC alive in its area. When one dies, after
-- `respawn_minutes` the fallen body is cleared and a new NPC arrives. The game server runs the rules.

CREATE TABLE live.npc_areas (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    id          ratw_id NOT NULL,
    name        text NOT NULL CHECK (char_length(name) BETWEEN 1 AND 120),
    kind        text NOT NULL CHECK (kind IN ('wander', 'spawn', 'plan')),
    area        ratw_id NOT NULL,
    -- Cell- or interior-local tiles: [[x, y], ...]
    tiles       jsonb NOT NULL DEFAULT '[]' CHECK (jsonb_typeof(tiles) = 'array' AND jsonb_array_length(tiles) <= 4096),
    position    integer NOT NULL DEFAULT 0,
    origin      text NOT NULL DEFAULT 'authored' CHECK (origin IN ('authored', 'runtime')),
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, id),
    FOREIGN KEY (world_id, area) REFERENCES world.areas (world_id, id) DEFERRABLE INITIALLY DEFERRED
);

CREATE TABLE live.spawns (
    world_id         ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    id               ratw_id NOT NULL,
    name             text NOT NULL CHECK (char_length(name) BETWEEN 1 AND 80),
    area_id          ratw_id NOT NULL,
    template_id      ratw_id NOT NULL,
    count            integer NOT NULL DEFAULT 1 CHECK (count BETWEEN 1 AND 50),
    respawn_minutes  integer NOT NULL DEFAULT 30 CHECK (respawn_minutes BETWEEN 0 AND 10080),
    enabled          boolean NOT NULL DEFAULT true,
    position         integer NOT NULL DEFAULT 0,
    origin           text NOT NULL DEFAULT 'authored' CHECK (origin IN ('authored', 'runtime')),
    updated_at       timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, id),
    FOREIGN KEY (world_id, area_id) REFERENCES live.npc_areas ON DELETE CASCADE
);

-- Which area an NPC wanders, and which spawn rule made them.
ALTER TABLE live.npcs
    ADD COLUMN wander_area ratw_id,
    ADD COLUMN spawn_id ratw_id,
    ADD FOREIGN KEY (world_id, wander_area) REFERENCES live.npc_areas ON DELETE SET NULL (wander_area),
    ADD FOREIGN KEY (world_id, spawn_id) REFERENCES live.spawns ON DELETE SET NULL (spawn_id);

-- NPC records now include wander tiles: wander "npc" N "cell" x y ... (tile centers).
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
    )
    SELECT coalesce(string_agg(text, E'\n' ORDER BY part, kind, position, line), '') FROM lines
$$;
