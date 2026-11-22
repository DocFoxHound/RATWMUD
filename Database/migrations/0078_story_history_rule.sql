-- 0078_story_history_rule.sql: a running story's trigger history is pruned by a rule (Docs/Design/85-story-triggers-and-
-- campaign-control.md, Phase 7; known problem 11): an entry older than `days` goes unless it is among the newest `keep`
-- of its run. The story runner calls it once an hour with 90 days and 10,000 (story_runner.prune_history). Returns how
-- many rows went.
--
-- (Numbers: 0076 stays Phase 8's; 0077 was Phase 5's.) Additive: nothing that exists is changed.
CREATE INDEX IF NOT EXISTS story_trigger_history_recorded ON dm.story_trigger_history (recorded_at);

CREATE FUNCTION dm.prune_story_history(p_world text, p_days integer, p_keep integer) RETURNS integer
LANGUAGE sql SECURITY DEFINER SET search_path = pg_catalog, pg_temp AS $$
    WITH gone AS (
        DELETE FROM dm.story_trigger_history h
        USING dm.story_releases r
        WHERE r.id = h.run AND r.world_id = p_world
          AND h.recorded_at < now() - make_interval(days => greatest(p_days, 1))
          AND h.id < coalesce((SELECT k.id FROM dm.story_trigger_history k WHERE k.run = h.run
                               ORDER BY k.id DESC OFFSET greatest(p_keep, 1) - 1 LIMIT 1), 0)
        RETURNING 1)
    SELECT count(*)::integer FROM gone
$$;
REVOKE ALL ON FUNCTION dm.prune_story_history(text, integer, integer) FROM PUBLIC;
GRANT EXECUTE ON FUNCTION dm.prune_story_history(text, integer, integer) TO ratw_dm;
