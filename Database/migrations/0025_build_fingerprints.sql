-- What a build was made from (Docs/Design/26-living-npcs.md, Phase 6): a digest of the world's revision, its live
-- layers (people, posts, routes, factions, claims, economy), the roster and the exporter's own code. Launching DEV
-- builds again only when it differs from the newest build's, which saves the half minute an export takes.
ALTER TABLE world.builds ADD COLUMN fingerprint text;
