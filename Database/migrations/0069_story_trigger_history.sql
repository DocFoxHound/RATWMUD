-- A trigger trace commits with the same event cursor and state as its effects.
CREATE TABLE dm.story_trigger_history (
    id bigserial PRIMARY KEY,
    run text NOT NULL REFERENCES dm.story_releases ON DELETE CASCADE,
    visit integer NOT NULL,
    entry jsonb NOT NULL,
    recorded_at timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX story_trigger_history_run ON dm.story_trigger_history (run, id DESC);
REVOKE ALL ON dm.story_trigger_history FROM PUBLIC, ratw_game, ratw_editor, ratw_publisher;
GRANT SELECT, INSERT ON dm.story_trigger_history TO ratw_dm;
GRANT USAGE ON SEQUENCE dm.story_trigger_history_id_seq TO ratw_dm;
