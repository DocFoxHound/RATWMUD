-- The document store (Docs/Design/55-letters-gifts-favours.md, 1): in-world writing (letters between players and from
-- residents, and later notices and pacts) as a table of its own instead of inside the checkpoint row. Written by the
-- game server's checkpoints like the save's other lists (only changed rows, game.save_checkpoint_delta) and read back
-- by game.load_checkpoint.
--
-- Each row is one document: its kind, its author (always kept, for the Dungeon Master and reports), its scent (the
-- author, or empty when masked), whom it is to, its text and signature, when it was written and is delivered, its post
-- town, its state (travelling, waiting, delivered, read, returned), whether it is kept, and an enclosure. Letters are
-- mail: held until the reader burns them. The Dungeon Master sees the metadata (the generated columns), and a letter's
-- text only inside a report; the editor and publisher have no need of them at all.
CREATE TABLE game.documents (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    key         text NOT NULL,
    position    integer NOT NULL,
    data        jsonb NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, key)
);
ALTER TABLE game.documents ADD COLUMN kind text GENERATED ALWAYS AS (data->>'kind') STORED,
                           ADD COLUMN author text GENERATED ALWAYS AS (data->>'author') STORED,
                           ADD COLUMN recipient text GENERATED ALWAYS AS (data->>'to') STORED,
                           ADD COLUMN state text GENERATED ALWAYS AS (data->>'state') STORED;
CREATE INDEX documents_recipient ON game.documents (world_id, recipient);
COMMENT ON TABLE game.documents IS 'The document store (doc 55): letters and other in-world writing, apart from every ledger.';

INSERT INTO game.sections VALUES ('documents', '{people,documents}', 'documents', $$e->>'id'$$);

REVOKE SELECT ON game.documents FROM ratw_editor, ratw_publisher;
