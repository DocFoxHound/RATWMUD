-- The game server builds its NPCs from the live tables (Dungeon Master phase 2).
--
-- live.people_manifest writes the NPC records of the game's world manifest
-- (economy, route, resident, story lines) from live.* rows, in exactly the
-- format tools/map_editor.py exports, so the server's own validated loader
-- reads them. The world build keeps only the world itself. Pass an NPC ID to
-- get just that NPC's lines (empty if they no longer exist).

-- std::quoted-style quoting, as map_editor.quote().
CREATE FUNCTION live.q(value text) RETURNS text LANGUAGE sql IMMUTABLE AS $$
    SELECT '"' || replace(replace(coalesce(value, ''), '\', '\\'), '"', '\"') || '"'
$$;

-- A place as "cell" x+.5 y+.5 (tile centers), as map_editor's at().
CREATE FUNCTION live.at(area text, x integer, y integer) RETURNS text LANGUAGE sql IMMUTABLE AS $$
    SELECT live.q(area) || ' ' || ((x + 0.5)::float8)::text || ' ' || ((y + 0.5)::float8)::text
$$;

CREATE FUNCTION live.people_manifest(p_world ratw_id, p_npc text DEFAULT NULL) RETURNS text
LANGUAGE sql STABLE AS $$
    WITH people AS (
        -- Named NPCs.
        SELECT 0 AS kind, n.position, n.id, n.name, n.role, n.work_label, n.description, n.greeting, n.age, n.appearance,
               n.voice, n.paid, n.hours_start, n.hours_end, n.route_id, n.purse, n.herbs, n.meals,
               n.home_area, n.home_x, n.home_y, n.work_area, n.work_x, n.work_y, n.evening_area, n.evening_x, n.evening_y,
               n.personality, n.backstory
        FROM live.npcs n WHERE n.world_id = p_world
        UNION ALL
        -- Profession slots filled by a roster character (roster.residents_for_slots).
        SELECT 1, s.position, c.id, c.name, p.behavior, coalesce(nullif(s.work_label, ''), p.work_label),
               coalesce(c.profile->>'description', ''), coalesce(c.profile->>'greeting', ''), c.age, c.profile->'appearance',
               (c.profile->>'voice')::integer, s.paid, s.hours_start, s.hours_end, s.route_id, s.purse, s.herbs, s.meals,
               s.home_area, s.home_x, s.home_y, s.work_area, s.work_x, s.work_y, s.evening_area, s.evening_x, s.evening_y,
               trim(coalesce(c.profile->>'personality', '') ||
                    CASE WHEN jsonb_array_length(coalesce(c.profile->'traits', '[]')) > 0
                         THEN ' Traits: ' || (SELECT string_agg(t, ', ') FROM jsonb_array_elements_text(c.profile->'traits') t) || '.'
                         ELSE '' END),
               coalesce(c.profile->>'backstory', '')
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
    )
    SELECT coalesce(string_agg(text, E'\n' ORDER BY part, kind, position, line), '') FROM lines
$$;

GRANT EXECUTE ON FUNCTION live.q(text), live.at(text, integer, integer), live.people_manifest(ratw_id, text) TO ratw_game, ratw_dm, ratw_editor, ratw_publisher;

-- The Dungeon Master changes NPCs through the live-edit log (tools/live_edit.py), so on DEV, Atlas users see
-- them arrive like anyone else's edits. It may add to the log and advance the world's revision, nothing else.
GRANT INSERT ON world.edits TO ratw_dm;
GRANT UPDATE (revision, updated_at) ON world.worlds TO ratw_dm;
