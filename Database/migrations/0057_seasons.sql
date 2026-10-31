-- The seasons in the Country's supply and the residents' comforts (Docs/Design/68-the-country-and-the-city-loop.md,
-- Progress, "Seasons and the DM's rules"): each world's own, in live.economy (never synced, as 0055's and 0056's):
--
--   comforts_above   pennies a head a household holds before it buys comforts (NULL: Data/Items/country.json's, 60)
--   comforts_share   percent of what is over that it spends on them a day (NULL: country.json's, 10)
--   seasons          the season groups this world has changed, {group: {supply: [4], price: [4], shelf: [4], normal: N}}:
--                    percent of normal by season (Spring, Summer, Autumn, Winter) for the Country's supply a day, its
--                    goods' price and the shops' shelves of them, and the group's normal pieces a day ({}: all as
--                    country.json says)
--
-- The game reads them in the NPC records: a "comforts" line when either is set ("-" for country.json's), a "season" line
-- a group: comforts ABOVE SHARE; season "<group>" supply SP SU AU WI price SP SU AU WI shelf SP SU AU WI normal N.
ALTER TABLE live.economy
    ADD COLUMN comforts_above integer CHECK (comforts_above BETWEEN 0 AND 1000000),
    ADD COLUMN comforts_share integer CHECK (comforts_share BETWEEN 0 AND 100),
    ADD COLUMN seasons        jsonb NOT NULL DEFAULT '{}'::jsonb CHECK (jsonb_typeof(seasons) = 'object');

-- As in 0056, with the comforts' and the seasons' lines.
CREATE OR REPLACE FUNCTION live.people_manifest(p_world ratw_id, p_npc text DEFAULT NULL) RETURNS text
LANGUAGE sql STABLE AS $$
    WITH people AS (
        SELECT 0 AS kind, n.position, n.id, n.name, n.role, n.work_label, n.description, n.greeting, n.age, n.appearance,
               n.voice, n.paid, n.hours_start, n.hours_end, n.route_id, n.purse, n.herbs, n.meals,
               n.home_area, n.home_x, n.home_y, n.work_area, n.work_x, n.work_y, n.evening_area, n.evening_x, n.evening_y,
               n.personality, n.backstory, n.wander_area, n.joinable, n.protected,
               coalesce(b.internal_store, true) AS internal_store
        FROM live.npcs n
        LEFT JOIN live.business_settings b ON b.world_id = n.world_id AND b.npc_id = n.id
        WHERE n.world_id = p_world
        UNION ALL
        SELECT 1, s.position, c.id, c.name, p.behavior, coalesce(nullif(s.work_label, ''), p.work_label),
               coalesce(c.profile->>'description', ''), coalesce(c.profile->>'greeting', ''), c.age, c.profile->'appearance',
               (c.profile->>'voice')::integer, s.paid, s.hours_start, s.hours_end, s.route_id, s.purse, s.herbs, s.meals,
               s.home_area, s.home_x, s.home_y, s.work_area, s.work_x, s.work_y, s.evening_area, s.evening_x, s.evening_y,
               trim(coalesce(c.profile->>'personality', '') ||
                    CASE WHEN jsonb_array_length(coalesce(c.profile->'traits', '[]')) > 0
                         THEN ' Traits: ' || (SELECT string_agg(t, ', ') FROM jsonb_array_elements_text(c.profile->'traits') t) || '.'
                         ELSE '' END),
               coalesce(c.profile->>'backstory', ''), NULL, false, false, true
        FROM live.profession_slots s
        JOIN live.professions p ON p.id = s.profession
        JOIN live.characters c ON c.status = 'active' AND c.assignment = jsonb_build_object('world', s.world_id, 'slot', s.id)
        WHERE s.world_id = p_world
    ),
    lines AS (
        SELECT 0 AS part, 0 AS kind, 0 AS position, 0 AS line, 'economy ' || concat_ws(' ', treasury, store_herbs, store_meals, daily_herbs, daily_meals) AS text
        FROM live.economy WHERE world_id = p_world AND p_npc IS NULL
        UNION ALL
        SELECT 0, 0, 0, 1, concat_ws(' ', 'store', CASE WHEN internal_store THEN 'on' ELSE 'off' END, store_markup,
                                     'resale', CASE WHEN store_resale THEN 'on' ELSE 'off' END, resale_markup,
                                     'rule', CASE WHEN store_rule THEN 'on' ELSE 'off' END)
        FROM live.economy WHERE world_id = p_world AND p_npc IS NULL
          AND (NOT internal_store OR store_markup <> 100 OR NOT store_resale OR resale_markup <> 75 OR store_rule)
        UNION ALL
        SELECT 0, 0, 0, 2, concat_ws(' ', 'loop', 'needs', CASE WHEN needs_paused THEN 'off' ELSE 'on' END,
                                     'country', CASE WHEN country THEN 'on' ELSE 'off' END, 'reserve', country_reserve,
                                     'toll', gate_toll, 'carriage', carriage, 'wages', wage_keeper, wage_hand, wage_public, wage_country)
        FROM live.economy WHERE world_id = p_world AND p_npc IS NULL
          AND (needs_paused OR NOT country OR country_reserve <> 200 OR gate_toll <> 30 OR carriage <> 6 OR wage_keeper <> 14
               OR wage_hand <> 8 OR wage_public <> 8 OR wage_country <> 8)
        UNION ALL
        SELECT 0, 0, 0, 3, 'countryoff ' || live.q(g)
        FROM live.economy, unnest(country_off) AS g WHERE world_id = p_world AND p_npc IS NULL
        UNION ALL
        SELECT 0, 0, 0, 4, 'subsidy ' || live.q(s.key) || ' ' || s.value
        FROM live.economy, jsonb_each_text(subsidies) AS s WHERE world_id = p_world AND p_npc IS NULL
          AND NOT (s.key = 'carriage' AND s.value = '25')
        UNION ALL
        SELECT 0, 0, 0, 5, concat_ws(' ', 'comforts', coalesce(comforts_above::text, '-'), coalesce(comforts_share::text, '-'))
        FROM live.economy WHERE world_id = p_world AND p_npc IS NULL AND (comforts_above IS NOT NULL OR comforts_share IS NOT NULL)
        UNION ALL
        SELECT 0, 0, 0, 6, concat_ws(' ', 'season', live.q(g.key),
               'supply', (SELECT string_agg(v, ' ' ORDER BY n) FROM jsonb_array_elements_text(g.value->'supply') WITH ORDINALITY AS x(v, n)),
               'price', (SELECT string_agg(v, ' ' ORDER BY n) FROM jsonb_array_elements_text(g.value->'price') WITH ORDINALITY AS x(v, n)),
               'shelf', (SELECT string_agg(v, ' ' ORDER BY n) FROM jsonb_array_elements_text(g.value->'shelf') WITH ORDINALITY AS x(v, n)),
               'normal', g.value->>'normal')
        FROM live.economy, jsonb_each(seasons) AS g WHERE world_id = p_world AND p_npc IS NULL
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
               hours_start::text, hours_end::text, live.q(CASE WHEN EXISTS (SELECT 1 FROM live.patrol_posts pp WHERE pp.world_id = p_world AND pp.route_id = people.route_id)
                                                     THEN route_id ELSE '-' END), purse, herbs, meals,
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
        SELECT 2, kind, position, 4, 'nostore ' || live.q(id)
        FROM people WHERE (p_npc IS NULL OR id = p_npc) AND NOT internal_store AND home_area IS NOT NULL
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
