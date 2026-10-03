-- Chapters, their camps and their dealings with the factions (Docs/Design/32-parties-chapters-factions.md), as tables
-- of their own instead of inside the checkpoint row: one row per Chapter, camp site, building, camp hand, treaty, levy
-- and House request. Written by the game server's checkpoints like the save's other lists (only changed rows, with
-- game.save_checkpoint_delta) and read back by game.load_checkpoint; readable by the tools and the Dungeon Master,
-- whose decisions still go through dm.actions (treaty.decide, house.decide).
DO $$
DECLARE t text;
BEGIN
    FOREACH t IN ARRAY ARRAY['chapters', 'camp_sites', 'camp_structures', 'camp_staff', 'treaties', 'levies', 'house_requests']
    LOOP
        EXECUTE format($f$
            CREATE TABLE game.%I (
                world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
                key         text NOT NULL,
                position    integer NOT NULL,
                data        jsonb NOT NULL,
                updated_at  timestamptz NOT NULL DEFAULT now(),
                PRIMARY KEY (world_id, key)
            )$f$, t);
    END LOOP;
END $$;

ALTER TABLE game.chapters ADD COLUMN name text GENERATED ALWAYS AS (data->>'name') STORED,
                          ADD COLUMN level integer GENERATED ALWAYS AS ((data->>'level')::integer) STORED;
ALTER TABLE game.camp_sites ADD COLUMN chapter text GENERATED ALWAYS AS (data->>'chapter') STORED,
                            ADD COLUMN cell text GENERATED ALWAYS AS (data->>'cell') STORED;
ALTER TABLE game.camp_structures ADD COLUMN site text GENERATED ALWAYS AS (data->>'site') STORED;
ALTER TABLE game.camp_staff ADD COLUMN site text GENERATED ALWAYS AS (data->>'site') STORED;
ALTER TABLE game.treaties ADD COLUMN chapter text GENERATED ALWAYS AS (data->>'chapter') STORED,
                          ADD COLUMN state text GENERATED ALWAYS AS (data->>'state') STORED;
ALTER TABLE game.levies ADD COLUMN chapter text GENERATED ALWAYS AS (data->>'chapter') STORED;
ALTER TABLE game.house_requests ADD COLUMN chapter text GENERATED ALWAYS AS (data->>'chapter') STORED,
                                ADD COLUMN state text GENERATED ALWAYS AS (data->>'state') STORED;
CREATE INDEX camp_sites_cell ON game.camp_sites (world_id, cell);
CREATE INDEX camp_structures_site ON game.camp_structures (world_id, site);

COMMENT ON TABLE game.chapters IS 'Chapters (player clans): members, ranks, renown, log, Hold and House.';
COMMENT ON TABLE game.camp_sites IS 'Camps and Holds Chapters have set up, one per cell.';
COMMENT ON TABLE game.camp_structures IS 'The buildings of the camps, raised or still being built, and their condition.';
COMMENT ON TABLE game.camp_staff IS 'Residents working at a camp for a wage.';
COMMENT ON TABLE game.treaties IS 'Treaties between a faction and a Chapter (pending, in force, broken, ended).';
COMMENT ON TABLE game.levies IS 'Levies a faction has called on a Chapter that holds land under it.';
COMMENT ON TABLE game.house_requests IS 'Chapters asking to be made a House of a faction.';

INSERT INTO game.sections VALUES
    ('chapters',       '{chapters,chapters}', 'chapters',        $$e->>'id'$$),
    ('campSites',      '{camps,sites}',       'camp_sites',      $$e->>'id'$$),
    ('campStructures', '{camps,structures}',  'camp_structures', $$e->>'id'$$),
    ('campStaff',      '{camps,staff}',       'camp_staff',      $$e->>'npc'$$),
    ('treaties',       '{factions,treaties}', 'treaties',        $$e->>'id'$$),
    ('levies',         '{factions,levies}',   'levies',          $$e->>'id'$$),
    ('houseRequests',  '{factions,houses}',   'house_requests',  $$concat(e->>'chapter', '|', e->>'faction')$$);
