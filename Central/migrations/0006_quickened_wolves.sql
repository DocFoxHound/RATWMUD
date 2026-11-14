-- Quickened is chosen in the Character Editor (Docs/Design/49-characters-and-earned-gifts.md, 9.1; the user,
-- 2026-11-14: "once it's unlocked ... they can create a quickened wolf character in the character editor ... This isn't
-- offered to a user when they join a server"). A wolf is Quickened from its making, for good, and only on an account
-- that has Quickened (quickened_at): central refuses it otherwise. Each ticket carries it (`tier`), and a world takes
-- the wolf's tier from there. The account's announcement (the client's fanfare, once per account, not per device):
-- when the client said it had shown it. Null: not yet.
ALTER TABLE central.wolves ADD COLUMN quickened boolean NOT NULL DEFAULT false;
ALTER TABLE central.accounts ADD COLUMN quickened_announced_at timestamptz;
