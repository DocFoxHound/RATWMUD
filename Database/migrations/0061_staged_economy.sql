-- 0061_staged_economy.sql: the staged economy's tables (Docs/Design/79-a-staged-economy.md, 14; Phase 9). Schema only:
-- no row is rewritten (doc 78: migrations never rewrite authored content).
-- live.economy keeps world_id, treasury, store_herbs, store_meals, needs_paused, seasons, services and rules (jsonb: the
-- integer rules incl. prosperity and harvestBias; prosperityCause; neverImported; the host's status and market overrides).
-- Every other column (store_*, internal_store, store_markup, store_resale, resale_markup, store_rule, country*, gate_toll,
-- carriage, wage_*, subsidies, comforts_*, farms, daily_herbs, daily_meals) stays and is ignored (D20). Never synced.

-- 1. An authored band per resident, for the household (NULL: from rank). First: the function below names it.
ALTER TABLE live.npcs ADD COLUMN band_override text
    CHECK (band_override IS NULL OR band_override IN ('destitute', 'poor', 'comfortable', 'rich'));
COMMENT ON COLUMN live.npcs.band_override IS
    'An authored band for this resident''s household; NULL = from rank. Doc 79, 14 (the Band picker).';

-- 2. The manifest, redefined before the drop (0060's body joins live.business_settings). As 0060's, less the old
-- economy's lines (store, loop extras, countryoff, subsidy, comforts, farms, farmarea, farm, nostore; the seasons' supply
-- and normal), with the rules' numbers only, one `overrides` line (the rules' prosperityCause, neverImported, status and
-- market as one JSON object, whatever its length: a line too long is Core's to skip with a warning, never dropped here
-- unsaid) and a `band` line after a resident's protected one. The economy line keeps two numbers after the store's
-- herbs and meals, always "10 12" (the old daily herbs and meals at their defaults, read past since): a server built
-- before doc 79's Phase 9 still reads a world with no overrides or band lines (DEV as it stands) after this migration.
-- tools/manifest_lines.py writes the same lines for an exported project.
CREATE OR REPLACE FUNCTION live.people_manifest(p_world ratw_id, p_npc text DEFAULT NULL) RETURNS text
LANGUAGE sql STABLE AS $$
    WITH people AS (
        SELECT 0 AS kind, n.position, n.id, n.name, n.role, n.work_label, n.description, n.greeting, n.age, n.appearance,
               n.voice, n.paid, n.hours_start, n.hours_end, n.route_id, n.purse, n.herbs, n.meals,
               n.home_area, n.home_x, n.home_y, n.work_area, n.work_x, n.work_y, n.evening_area, n.evening_x, n.evening_y,
               n.personality, n.backstory, n.wander_area, n.joinable, n.protected, n.band_override
        FROM live.npcs n
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
               coalesce(c.profile->>'backstory', ''), NULL, false, false, NULL::text
        FROM live.profession_slots s
        JOIN live.professions p ON p.id = s.profession
        JOIN live.characters c ON c.status = 'active' AND c.assignment = jsonb_build_object('world', s.world_id, 'slot', s.id)
        WHERE s.world_id = p_world
    ),
    lines AS (
        SELECT 0 AS part, 0 AS kind, 0 AS position, 0 AS line,
               'economy ' || concat_ws(' ', treasury, store_herbs, store_meals, 10, 12) AS text
        FROM live.economy WHERE world_id = p_world AND p_npc IS NULL
        UNION ALL
        SELECT 0, 0, 0, 2, 'loop needs off'
        FROM live.economy WHERE world_id = p_world AND p_npc IS NULL AND needs_paused
        UNION ALL
        SELECT 0, 0, 0, 6, concat_ws(' ', 'season', live.q(g.key),
               'price', (SELECT string_agg(v, ' ' ORDER BY n) FROM jsonb_array_elements_text(g.value->'price') WITH ORDINALITY AS x(v, n)),
               'shelf', (SELECT string_agg(v, ' ' ORDER BY n) FROM jsonb_array_elements_text(g.value->'shelf') WITH ORDINALITY AS x(v, n)))
        FROM live.economy, jsonb_each(seasons) AS g
        WHERE world_id = p_world AND p_npc IS NULL AND jsonb_typeof(g.value) = 'object'
          AND jsonb_typeof(g.value->'price') = 'array' AND jsonb_array_length(g.value->'price') = 4
          AND jsonb_typeof(g.value->'shelf') = 'array' AND jsonb_array_length(g.value->'shelf') = 4
        UNION ALL
        SELECT 0, 0, 0, 7, 'rule ' || live.q(r.key) || ' ' || r.value::text
        FROM live.economy, jsonb_each(rules) AS r
        WHERE world_id = p_world AND p_npc IS NULL AND jsonb_typeof(r.value) = 'number'
        UNION ALL
        SELECT 0, 0, 0, 8, concat_ws(' ', 'service', live.q(s.key), CASE WHEN (s.value->>'on')::boolean THEN 'on' ELSE 'off' END,
               'price', s.value->>'price', 'every', s.value->>'every')
        FROM live.economy, jsonb_each(services) AS s WHERE world_id = p_world AND p_npc IS NULL
        UNION ALL
        SELECT 0, 0, 0, 9, 'overrides ' || o.v::text
        FROM live.economy,
             LATERAL (SELECT jsonb_object_agg(key, value) AS v FROM jsonb_each(rules)
                      WHERE key IN ('prosperityCause', 'neverImported', 'status', 'market')) AS o
        WHERE world_id = p_world AND p_npc IS NULL AND o.v IS NOT NULL
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
        SELECT 2, kind, position, 4, 'band ' || live.q(id) || ' ' || band_override
        FROM people WHERE (p_npc IS NULL OR id = p_npc) AND band_override IS NOT NULL
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

-- 3. The store room's per-business switches go (D10). No CASCADE: a hidden dependent fails the migration loudly. (Only
-- live.people_manifest named it, in 0055-0060; redefined above without it.)
DROP TABLE live.business_settings;

COMMENT ON TABLE live.economy IS 'Doc 79, 14: read are world_id, treasury, store_herbs, store_meals, needs_paused, seasons, '
    'services, rules; every other column is dead (D20). Never synced.';
COMMENT ON COLUMN live.economy.rules IS 'Doc 79, 12/14: {rule: int (incl. prosperity, harvestBias), prosperityCause: text, '
    'neverImported: [good], status: {status.json override}, market: {market.json override}}';
