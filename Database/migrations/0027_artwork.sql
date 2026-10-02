-- Portraits players upload for their characters (Docs/Design/29-client-polish.md, phase 9). The server encodes every
-- image itself from raw pixels (no uploaded file is kept), 256 pixels square. A portrait is shown to its owner at once
-- and to everyone else only once a Dungeon Master has approved it; a report sends it back for review.
CREATE TABLE game.artwork (
    world_id     ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    id           text NOT NULL CHECK (id ~ '^art-[a-z0-9-]{1,44}$'),
    account      text NOT NULL CHECK (char_length(account) BETWEEN 1 AND 64),
    character_id text NOT NULL CHECK (char_length(character_id) BETWEEN 1 AND 64),
    status       text NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'approved', 'rejected')),
    reason       text NOT NULL DEFAULT '' CHECK (char_length(reason) <= 400),
    reported     boolean NOT NULL DEFAULT false,
    sha256       text NOT NULL CHECK (sha256 ~ '^[0-9a-f]{64}$'),
    png_base64   text NOT NULL CHECK (char_length(png_base64) <= 1500000),
    created_at   timestamptz NOT NULL DEFAULT now(),
    reviewed_at  timestamptz,
    PRIMARY KEY (world_id, id)
);
CREATE INDEX artwork_review ON game.artwork (world_id, status) WHERE status = 'pending' OR reported;
COMMENT ON TABLE game.artwork IS 'Uploaded character portraits, re-encoded by the server, and whether a DM has approved them.';
