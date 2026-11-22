-- 0076_story_facts.sql: the world's facts for its live stories (Docs/Design/85-story-triggers-and-campaign-control.md, 3.4 and
-- Phase 8). While a world story of the story runner is live and asks for them (its watch list, sent with each story.phase), the
-- game writes one small document of the world's facts with every write of its events, in the same transaction: where each wolf
-- in the world stands (indoors or out, the town), their party, what they carry of the items the stories name, their coins, their
-- Chapter's standing with the factions named, their name in the towns named, whether they are downed, hurt, wanted or held;
-- the residents named (alive, where, job, mood, mantle, town); the hour, weekday, season and time of day; the weather in the
-- places named or where the wolves stand; the relations named. A row's `as_of_event` is the newest event written with it, so
-- an event's facts are those of the first row at or after it: never older than the event, and at most the length of one write
-- newer (a second while a story is live). The runner reads them a page of events at a time (story_runner.facts_page).
--
-- One document per world, not per run: the union of what the live stories watch (a condition reads only what it names). A
-- write whose facts are the same as the newest row's moves that row's `as_of_event` on instead of adding one. Rows older than
-- a day go as new ones come, the newest always kept.
--
-- (Numbers: 0076 was kept for Phase 8; 0077 and 0078 are Phases 5's and 7's.) Additive: nothing that exists is changed.
CREATE TABLE game.story_facts (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    as_of_event bigint NOT NULL,                       -- The newest game.events id written with this document.
    written_at  timestamptz NOT NULL DEFAULT now(),
    facts       jsonb NOT NULL CHECK (jsonb_typeof(facts) = 'object'),
    PRIMARY KEY (world_id, as_of_event)
);
CREATE INDEX story_facts_written ON game.story_facts (world_id, written_at);
COMMENT ON TABLE game.story_facts IS 'The world''s facts as of its events, for live world stories (doc 85, Phase 8). Game server writes.';

-- Called by the game server in the transaction that wrote its events (after game.record_events). Returns the as_of_event.
CREATE FUNCTION game.record_story_facts(p_world ratw_id, p_facts jsonb) RETURNS bigint
LANGUAGE plpgsql AS $$
DECLARE
    v_at   bigint := coalesce((SELECT max(id) FROM game.events WHERE world_id = p_world), 0);
    v_last game.story_facts%ROWTYPE;
BEGIN
    SELECT * INTO v_last FROM game.story_facts WHERE world_id = p_world ORDER BY as_of_event DESC LIMIT 1;
    IF FOUND AND v_last.as_of_event >= v_at THEN
        UPDATE game.story_facts SET facts = p_facts, written_at = now() WHERE world_id = p_world AND as_of_event = v_last.as_of_event;
    ELSIF FOUND AND v_last.facts = p_facts THEN
        UPDATE game.story_facts SET as_of_event = v_at, written_at = now() WHERE world_id = p_world AND as_of_event = v_last.as_of_event;
    ELSE
        INSERT INTO game.story_facts (world_id, as_of_event, facts) VALUES (p_world, v_at, p_facts);
    END IF;
    DELETE FROM game.story_facts WHERE world_id = p_world AND written_at < now() - interval '1 day' AND as_of_event < v_at;
    RETURN v_at;
END
$$;
REVOKE ALL ON FUNCTION game.record_story_facts(ratw_id, jsonb) FROM PUBLIC;
GRANT EXECUTE ON FUNCTION game.record_story_facts(ratw_id, jsonb) TO ratw_game;
