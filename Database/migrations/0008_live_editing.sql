-- One true world, edited live by several people at once (tools/live_edit.py).

-- There is exactly one world; every cell and interior belongs to it.
CREATE UNIQUE INDEX one_world ON world.worlds ((true));

-- Every change anyone makes, in order. Editors poll it for changes they have
-- not seen, and it names who last touched something when edits collide.
-- key: what changed, e.g. "tile:12,-40", "cell:town", "person:mara" (see live_edit.py).
CREATE TABLE world.edits (
    seq        bigserial PRIMARY KEY,
    world_id   ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    batch      bigint NOT NULL,
    at         timestamptz NOT NULL DEFAULT now(),
    editor     text NOT NULL CHECK (char_length(editor) BETWEEN 1 AND 60),
    client_id  text NOT NULL CHECK (char_length(client_id) <= 64),
    label      text NOT NULL DEFAULT '' CHECK (char_length(label) <= 200),
    key        text NOT NULL CHECK (char_length(key) <= 160),
    before     jsonb,
    after      jsonb
);
CREATE INDEX edits_by_key ON world.edits (world_id, key, seq DESC);

-- Who is editing right now and where: a heartbeat every few seconds.
CREATE TABLE world.editors (
    client_id  text PRIMARY KEY CHECK (char_length(client_id) <= 64),
    editor     text NOT NULL CHECK (char_length(editor) BETWEEN 1 AND 60),
    color      text NOT NULL CHECK (color ~ '^#[0-9a-fA-F]{6}$'),
    state      jsonb NOT NULL DEFAULT '{}' CHECK (jsonb_typeof(state) = 'object'),
    seen_at    timestamptz NOT NULL DEFAULT now()
);
