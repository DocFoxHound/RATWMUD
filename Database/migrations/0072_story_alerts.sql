-- 0072_story_alerts.sql: a running campaign explained (Docs/Design/85-story-triggers-and-campaign-control.md, Phase 3,
-- "Explain it"): each world's own alert rules, and the "needs you" alerts told outside the Campaigns tab, in the game's Dev
-- Console too (5.6). Additive: no row is rewritten, and every new column has a default that changes nothing for what exists.

-- The alert rules a world's owner changed (5.6: "all adjustable per world"; 5.7: changing them is the owner's). Keys are
-- tools/story_runner.py's ALERTS (gameSeconds, behindEvents, behindSeconds, quietHours, runnerSeconds, overdueSeconds,
-- goneDays, fullPercent); a key not here keeps its first value.
CREATE TABLE dm.story_alert_rules (
    world_id   ratw_id PRIMARY KEY REFERENCES world.worlds ON DELETE CASCADE,
    rules      jsonb NOT NULL DEFAULT '{}' CHECK (jsonb_typeof(rules) = 'object'),
    updated_by text NOT NULL DEFAULT '',
    updated_at timestamptz NOT NULL DEFAULT now()
);
REVOKE ALL ON dm.story_alert_rules FROM PUBLIC, ratw_game, ratw_editor, ratw_publisher;
GRANT SELECT, INSERT, UPDATE ON dm.story_alert_rules TO ratw_dm;

-- With its heartbeat the runner leaves, every ten seconds, how many stories are live and their "needs you" alerts (each
-- {key, run, kind, text}: a paused story, one that cannot move on), for the game to tell its Dungeon Masters.
ALTER TABLE dm.story_runner ADD COLUMN live integer NOT NULL DEFAULT 0;
ALTER TABLE dm.story_runner ADD COLUMN alerts jsonb NOT NULL DEFAULT '[]' CHECK (jsonb_typeof(alerts) = 'array');
ALTER TABLE dm.story_runner ADD COLUMN alerts_at timestamptz;

-- What the game may know of it, and nothing else (the story tables stay closed to the game, 0067): how many stories are
-- running, how long since the runner last beat, the threshold for "not running", and the runner's alerts. The game tells
-- its Dungeon Masters in the Dev Console when the runner is down while stories should be running, and each alert once.
CREATE FUNCTION dm.story_alerts_for_game(w text) RETURNS jsonb
    LANGUAGE sql STABLE SECURITY DEFINER SET search_path = pg_catalog, pg_temp AS $$
    SELECT jsonb_build_object(
        'running', (SELECT count(*) FROM dm.story_releases WHERE world_id = w AND status IN ('running', 'stopping')),
        'beatAge', (SELECT round(extract(epoch FROM now() - beat_at)::numeric, 1) FROM dm.story_runner WHERE world_id = w),
        'runnerSeconds', coalesce((SELECT (rules->>'runnerSeconds')::float8 FROM dm.story_alert_rules
                                   WHERE world_id = w AND jsonb_typeof(rules->'runnerSeconds') = 'number'), 30),
        'alerts', coalesce((SELECT alerts FROM dm.story_runner WHERE world_id = w
                            AND alerts_at > now() - interval '5 minutes'), '[]'::jsonb))
$$;
REVOKE ALL ON FUNCTION dm.story_alerts_for_game(text) FROM PUBLIC;
GRANT EXECUTE ON FUNCTION dm.story_alerts_for_game(text) TO ratw_game, ratw_dm;
