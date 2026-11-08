-- The honest count (Docs/Design/77-central-accounts-and-hosted-worlds.md, section 7; Phase 4) counts a wolf at a
-- world only if its latest ticket, to any world, was for that one: so it looks a heartbeat's wolves up by wolf across
-- every world, newest first.
CREATE INDEX tickets_wolf ON central.tickets (wolf_id, issued_at);
