-- Exact numbers, author order, and cell overlap checked at commit.

-- Lighting and hours are authored decimals (0.6, 7.5); real would store 0.6000000238.
ALTER TABLE world.cells ALTER light_artificial TYPE double precision, ALTER light_daylight TYPE double precision;
ALTER TABLE world.interiors ALTER light_artificial TYPE double precision, ALTER light_daylight TYPE double precision;
ALTER TABLE live.professions ALTER hours_start TYPE double precision, ALTER hours_end TYPE double precision;
ALTER TABLE live.npcs ALTER hours_start TYPE double precision, ALTER hours_end TYPE double precision;
ALTER TABLE live.profession_slots ALTER hours_start TYPE double precision, ALTER hours_end TYPE double precision;

-- The order authors see lists in; the roster also breaks job-assignment ties by it.
ALTER TABLE world.cells ADD COLUMN position integer NOT NULL DEFAULT 0;
ALTER TABLE world.interiors ADD COLUMN position integer NOT NULL DEFAULT 0;
ALTER TABLE world.links ADD COLUMN position integer NOT NULL DEFAULT 0;
ALTER TABLE world.factions ADD COLUMN position integer NOT NULL DEFAULT 0;
ALTER TABLE world.chapters ADD COLUMN position integer NOT NULL DEFAULT 0;
ALTER TABLE live.professions ADD COLUMN position integer NOT NULL DEFAULT 0;
ALTER TABLE live.characters ADD COLUMN position integer NOT NULL DEFAULT 0;
ALTER TABLE live.npcs ADD COLUMN position integer NOT NULL DEFAULT 0;
ALTER TABLE live.profession_slots ADD COLUMN position integer NOT NULL DEFAULT 0;
ALTER TABLE live.patrol_routes ADD COLUMN position integer NOT NULL DEFAULT 0;

-- Re-cutting moves several cell edges in one save; only the committed result must not overlap.
ALTER TABLE world.cells DROP CONSTRAINT cells_world_id_box_excl;
ALTER TABLE world.cells ADD CONSTRAINT cells_do_not_overlap EXCLUDE USING gist (world_id WITH =,
    box(point(x, y), point(x + width - 1, y + height - 1)) WITH &&) DEFERRABLE INITIALLY DEFERRED;
