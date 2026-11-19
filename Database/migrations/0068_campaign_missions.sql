-- The last known mission ledger for each phase visit, including closed and repeated visits.
ALTER TABLE dm.story_transitions ADD COLUMN missions jsonb NOT NULL DEFAULT '[]';
