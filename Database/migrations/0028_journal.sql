-- The journal (Docs/Design/31-responsiveness.md, Phase 2): what changed in money, goods, positions held, contracts,
-- companions, the law and accounts, one record per valuable command, written in batches by the game server's
-- journal thread while the game plays on. A player is told a trade is done once its record is here.
--
-- Each record is a JSON array of changes to the checkpoint document (RatwJournal.h). On start-up the server loads
-- the checkpoint and applies every record after the one the checkpoint was taken at ("journal" in its document);
-- once a newer checkpoint is stored, the records it covers are removed.
CREATE TABLE game.journal (
    world_id    ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    seq         bigint NOT NULL CHECK (seq > 0),
    record      text NOT NULL,
    recorded_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, seq)
);
COMMENT ON TABLE game.journal IS 'Valuable changes since the last checkpoint, replayed over it on start-up (game server writes and trims).';
