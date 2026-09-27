-- Schemas, shared types and access rules. Every later table inherits its grants
-- from the default privileges below, so migrations never grant table by table.
--
--   world  authored world content: terrain, cells, interiors, doors, places
--   live   simulation data the server changes: NPCs, routes, jobs, economy, roster
--   game   game-server state: accounts, characters, memories, relationships
--   admin  publish password, release history
-- The same grants apply in DEV and PROD; who may connect to which database is
-- decided when the roles are created (Database/init/10-roles.sh).

CREATE EXTENSION IF NOT EXISTS btree_gist;

CREATE DOMAIN ratw_id AS text CHECK (VALUE ~ '^[a-z][a-z0-9_-]{0,47}$');

CREATE SCHEMA world;
CREATE SCHEMA live;
CREATE SCHEMA game;
CREATE SCHEMA admin;

GRANT USAGE ON SCHEMA world, live, game TO ratw_editor, ratw_publisher, ratw_game;
GRANT USAGE ON SCHEMA admin TO ratw_publisher;

-- Authored content: the editor and Push to live write it; the game only reads it.
ALTER DEFAULT PRIVILEGES FOR ROLE ratw_owner IN SCHEMA world
    GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO ratw_editor, ratw_publisher;
ALTER DEFAULT PRIVILEGES FOR ROLE ratw_owner IN SCHEMA world GRANT SELECT ON TABLES TO ratw_game;

-- Live data: the game owns it at runtime; the editor places seeds in DEV and Push to live adds them to PROD.
ALTER DEFAULT PRIVILEGES FOR ROLE ratw_owner IN SCHEMA live
    GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO ratw_editor, ratw_publisher, ratw_game;

-- Game state belongs to the game server alone; tools may read it.
ALTER DEFAULT PRIVILEGES FOR ROLE ratw_owner IN SCHEMA game GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO ratw_game;
ALTER DEFAULT PRIVILEGES FOR ROLE ratw_owner IN SCHEMA game GRANT SELECT ON TABLES TO ratw_editor, ratw_publisher;

-- Publishing reads the password hash and records releases. Only the owner (tools/world_db.py) changes settings.
ALTER DEFAULT PRIVILEGES FOR ROLE ratw_owner IN SCHEMA admin GRANT SELECT, INSERT ON TABLES TO ratw_publisher;

ALTER DEFAULT PRIVILEGES FOR ROLE ratw_owner GRANT USAGE, SELECT ON SEQUENCES TO ratw_editor, ratw_publisher, ratw_game;
