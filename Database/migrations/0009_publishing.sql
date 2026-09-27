-- Push to live (tools/publish.py): releases keep what they published, so any
-- one can be restored, and failed password attempts are counted.

ALTER TABLE admin.releases
    ADD COLUMN kind text NOT NULL DEFAULT 'push' CHECK (kind IN ('push', 'rollback')),
    -- For a rollback: the release it restored.
    ADD COLUMN restored integer,
    -- The authored world content this release put live, table by table.
    ADD COLUMN content jsonb,
    -- What changed, for the release history.
    ADD COLUMN summary jsonb NOT NULL DEFAULT '{}';

CREATE TABLE admin.publish_attempts (
    id      bigserial PRIMARY KEY,
    at      timestamptz NOT NULL DEFAULT now(),
    editor  text NOT NULL DEFAULT '' CHECK (char_length(editor) <= 60),
    action  text NOT NULL CHECK (action IN ('push', 'rollback')),
    ok      boolean NOT NULL
);
CREATE INDEX publish_attempts_recent ON admin.publish_attempts (at DESC);
