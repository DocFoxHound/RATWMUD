-- NPCs without a map (Docs/Design/66-worlds-atlas-and-dm.md, 2): an NPC's home, work and evening areas become
-- optional, on named NPCs and on profession slots alike. Unplaced is a state, not a place:
--
--   home and work      placed, as before
--   home, no work      placed without work: lives at home, idle (no job, no wage)
--   no home            unplaced: in the roster with its talk, relations and memories, but no body in the world
--
-- An unset area keeps its foreign key (a set one must still name an area of the world); its coordinates stay NOT NULL
-- and mean nothing while it is unset (the tools write 0, 0). The game's NPC records write an unset place as "-" 0 0
-- (no area ID can be "-": IDs start with a letter); the editor's exporter writes them the same way.
ALTER TABLE live.npcs ALTER COLUMN home_area DROP NOT NULL,
                      ALTER COLUMN work_area DROP NOT NULL,
                      ALTER COLUMN evening_area DROP NOT NULL;
ALTER TABLE live.profession_slots ALTER COLUMN home_area DROP NOT NULL,
                                  ALTER COLUMN work_area DROP NOT NULL,
                                  ALTER COLUMN evening_area DROP NOT NULL;

-- A place in the NPC records: live.at's "area" x y, or "-" 0 0 when it is unset.
CREATE OR REPLACE FUNCTION live.place(area text, x integer, y integer) RETURNS text LANGUAGE sql IMMUTABLE AS $$
    SELECT CASE WHEN area IS NULL THEN '"-" 0 0' ELSE live.at(area, x, y) END
$$;

-- As in 0051, with unset places.
CREATE OR REPLACE FUNCTION live.people_manifest(p_world ratw_id, p_npc text DEFAULT NULL) RETURNS text
LANGUAGE sql STABLE AS $$
    WITH people AS (
        SELECT 0 AS kind, n.position, n.id, n.name, n.role, n.work_label, n.description, n.greeting, n.age, n.appearance,
               n.voice, n.paid, n.hours_start, n.hours_end, n.route_id, n.purse, n.herbs, n.meals,
               n.home_area, n.home_x, n.home_y, n.work_area, n.work_x, n.work_y, n.evening_area, n.evening_x, n.evening_y,
               n.personality, n.backstory, n.wander_area, n.joinable, n.protected
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
               coalesce(c.profile->>'backstory', ''), NULL, false, false
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
               live.place(home_area, home_x, home_y), live.place(work_area, work_x, work_y), live.place(evening_area, evening_x, evening_y))
        FROM people WHERE p_npc IS NULL OR id = p_npc
        UNION ALL
        SELECT 2, kind, position, 1, 'story ' || live.q(id) || ' ' || live.q(replace(personality, E'\n', ' ')) || ' ' || live.q(replace(backstory, E'\n', ' '))
        FROM people WHERE (p_npc IS NULL OR id = p_npc) AND (personality <> '' OR backstory <> '')
        UNION ALL
        SELECT 2, kind, position, 2, 'joinable ' || live.q(id)
        FROM people WHERE (p_npc IS NULL OR id = p_npc) AND joinable
        UNION ALL
        SELECT 2, kind, position, 3, 'protected ' || live.q(id)
        FROM people WHERE (p_npc IS NULL OR id = p_npc) AND protected
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
        UNION ALL
        SELECT 6, 0, 0, 0, 'kin ' || live.q(r.npc_id) || ' ' || live.q(r.other_id) || ' ' || r.kind
        FROM live.npc_relations r WHERE r.world_id = p_world AND p_npc IS NULL
    )
    SELECT coalesce(string_agg(text, E'\n' ORDER BY part, kind, position, line, text), '') FROM lines
$$;

GRANT EXECUTE ON FUNCTION live.place(text, integer, integer) TO ratw_game, ratw_dm, ratw_editor, ratw_publisher;
