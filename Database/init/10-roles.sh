#!/bin/bash
# Runs once, when the data volume is first created. Creates the login roles and
# the two databases; tables come from Database/migrations (tools/world_db.py migrate).
#   ratw_owner      owns every schema and table; only migrations use it
#   ratw_editor     Atlas Workshop: reads and writes DEV, cannot connect to PROD
#   ratw_publisher  Push to live: copies DEV into PROD
#   ratw_game       the game server: reads the world, writes live and game data
#   ratw_dm         the Dungeon Master: runs the live world, never writes terrain
set -euo pipefail
psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname postgres \
    -v owner_pw="$RATW_OWNER_PASSWORD" -v editor_pw="$RATW_EDITOR_PASSWORD" \
    -v publisher_pw="$RATW_PUBLISHER_PASSWORD" -v game_pw="$RATW_GAME_PASSWORD" -v dm_pw="$RATW_DM_PASSWORD" <<'SQL'
CREATE ROLE ratw_owner LOGIN PASSWORD :'owner_pw';
CREATE ROLE ratw_editor LOGIN PASSWORD :'editor_pw';
CREATE ROLE ratw_publisher LOGIN PASSWORD :'publisher_pw';
CREATE ROLE ratw_game LOGIN PASSWORD :'game_pw';
CREATE ROLE ratw_dm LOGIN PASSWORD :'dm_pw';
CREATE DATABASE ratw_dev OWNER ratw_owner;
CREATE DATABASE ratw_prod OWNER ratw_owner;
REVOKE ALL ON DATABASE ratw_dev, ratw_prod FROM PUBLIC;
GRANT CONNECT ON DATABASE ratw_dev TO ratw_editor, ratw_publisher, ratw_game;
GRANT CONNECT ON DATABASE ratw_prod TO ratw_publisher, ratw_game;
GRANT CONNECT ON DATABASE ratw_dev, ratw_prod TO ratw_dm;
SQL
