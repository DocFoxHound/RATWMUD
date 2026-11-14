-- A wolf's own choices for each world's first arrival (the user, 2026-11-14: Strengths and Tie are set in the
-- character editor, not at the arrival): {"grades": {attribute: "weak" | "strong"}, "tie": a story starter's id or ""},
-- as ratw_identity's "choices" check makes them. A world still keeps what it was given at the arrival; this is what the
-- client gives it.
ALTER TABLE central.wolves ADD COLUMN choices jsonb NOT NULL DEFAULT '{}';
