-- Deleting a world removes everything in it. Resources and places are tied to
-- their area (so an area in use cannot be removed on its own), but they must
-- also go when their whole world does.
ALTER TABLE world.resources ADD FOREIGN KEY (world_id) REFERENCES world.worlds ON DELETE CASCADE;
ALTER TABLE world.places ADD FOREIGN KEY (world_id) REFERENCES world.worlds ON DELETE CASCADE;
