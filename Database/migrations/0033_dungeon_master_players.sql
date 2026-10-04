-- Players marked Dungeon Master in the game (Docs/Design/34-dungeon-master-refresh.md, 1.1b): the Dev Console is theirs.
--
-- The mark is saved with the character, in its record ("dungeonMaster": true, written by the game server like the rest
-- of the character); this column is that, as true or false, kept in step by PostgreSQL itself, so the tools and anyone
-- reading the table can see and filter it. The game server never writes the column (its saves name their columns), and
-- it cannot drift from the record.
ALTER TABLE game.characters
    ADD COLUMN dungeon_master boolean GENERATED ALWAYS AS (coalesce(data->'dungeonMaster' = 'true'::jsonb, false)) STORED;
COMMENT ON COLUMN game.characters.dungeon_master IS 'Marked a Dungeon Master in the game (the Dev Console), from the saved record.';
CREATE INDEX characters_dungeon_masters ON game.characters (world_id) WHERE dungeon_master;
