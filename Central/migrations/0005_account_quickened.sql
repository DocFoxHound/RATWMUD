-- Quickened for the whole account (Docs/Design/49-characters-and-earned-gifts.md, 9.1; the user, 2026-11-14: "once
-- per the whole account, not once per world"): when an official world first said the account's standing there had
-- opened Quickened (its heartbeat's "quickened" wolves), and which world. Null: not yet. Central puts it in every ticket
-- (`quickened`), so every world honours it, and shows it to the client's Character Editor. Kept once recorded.
ALTER TABLE central.accounts ADD COLUMN quickened_at timestamptz, ADD COLUMN quickened_world text;
