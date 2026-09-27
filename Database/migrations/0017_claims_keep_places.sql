-- A territory claim no longer disappears with its place: removing a claimed cell or interior must be deliberate.
-- The editor removes a deleted place's claims itself; a push or rollback that would strand live claims is refused
-- with their names, like NPCs and NPC areas.
ALTER TABLE live.faction_claims
    DROP CONSTRAINT faction_claims_world_id_area_fkey,
    ADD FOREIGN KEY (world_id, area) REFERENCES world.areas (world_id, id) DEFERRABLE INITIALLY DEFERRED;
