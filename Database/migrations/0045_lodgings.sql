-- Lodgings (Docs/Design/54-gathering-places.md, 4): individuals' rented beds and places (an inn bed, a lodger's bed, the
-- inn's upstairs for a night, a place listed for individuals), as a table of their own instead of inside the
-- checkpoint row. Written by the game server's checkpoints like the save's other lists (only changed rows,
-- game.save_checkpoint_delta) and read back by game.load_checkpoint.
--
-- Each row is one lodging: its holder, the cell (and bed), its kind and period, the landlord and the account rent goes
-- to, the rent, when it is paid to, any notice to quit, whether its doors are open for the night, and its guests.
-- Chapters' leases stay with the estates (doc 32). The Dungeon Master reads them; the editor and publisher have no need.
CREATE TABLE game.lodgings (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    key         text NOT NULL,
    position    integer NOT NULL,
    data        jsonb NOT NULL,
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, key)
);
ALTER TABLE game.lodgings ADD COLUMN holder text GENERATED ALWAYS AS (data->>'holder') STORED,
                          ADD COLUMN cell text GENERATED ALWAYS AS (data->>'cell') STORED,
                          ADD COLUMN kind text GENERATED ALWAYS AS (data->>'kind') STORED;
COMMENT ON TABLE game.lodgings IS 'Lodgings (doc 54): beds and places rented by individual wolves.';

INSERT INTO game.sections VALUES ('lodgings', '{people,lodgings}', 'lodgings', $$e->>'id'$$);

REVOKE SELECT ON game.lodgings FROM ratw_editor, ratw_publisher;
