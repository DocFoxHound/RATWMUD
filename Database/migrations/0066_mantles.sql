-- 0066_mantles.sql: mantles (Docs/Design/84-living-conversations.md §7.9, §17.1; Phase 9): each wolf's way of talking,
-- name-free, passed on at death. The user's answer Q2 puts the text here, editable live in Atlas (DEV) and the Dungeon
-- Master (live). One row a mantle (its card) and one row an item (a line, a rule, a scene, a symbol, an opinion, a
-- story, a window line, a graft) in the .mantle syntax, so the game's one parser reads rows, drafts and exports alike.
-- The game reads them at start, on voice.sync and on mantle.sync <id>, and never writes them: it writes only the
-- bindings in live.npc_voice. Additive: no existing row is rewritten, and a server built before it reads past it.
--
-- Taken back by Database/rollback/0066_mantles_down.sql, run only after `tools/mantle_load.py backup <world>` has dumped
-- the three tables and the new columns to Saved/Backups/.
--
-- One kind beyond §17.1's: 'scene', a whole written scene the wearer opens (§37, the hybrid: written scenes carry the
-- talk, so a mantle may hold its own).

CREATE TABLE live.mantles (
    world_id     ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    id           text NOT NULL CHECK (id ~ '^[a-z]{2}[0-9]{3,4}$'),
    dialect      text NOT NULL CHECK (dialect ~ '^[a-z_]{1,40}$'),     -- plain, or a live.dialects name (checked on save)
    tier         text NOT NULL CHECK (tier IN ('full', 'standard', 'spare')),
    state        text NOT NULL DEFAULT 'draft' CHECK (state IN ('draft', 'ready', 'retired')),
    format       integer NOT NULL DEFAULT 1 CHECK (format >= 1),        -- the .mantle syntax's version: the server reads
                                                                        -- its own and the one before, as builds do (0065)
    accents      text[] NOT NULL CHECK (cardinality(accents) BETWEEN 1 AND 3
                                        AND accents <@ ARRAY['none', 'light', 'broad']),  -- the first is as written
    title        text NOT NULL DEFAULT '' CHECK (char_length(title) <= 80),      -- for authors and the DM, never spoken
    written_for  ratw_id,                                                         -- metadata, never spoken
    fits         text NOT NULL DEFAULT '' CHECK (char_length(fits) <= 400),      -- hard criteria a wearer must match
    prefers      text NOT NULL DEFAULT '' CHECK (char_length(prefers) <= 400),   -- soft: how well a wearer suits it
    traits       jsonb NOT NULL DEFAULT '{}' CHECK (jsonb_typeof(traits) = 'object'),   -- as live.npc_voice.traits
    quirk        text NOT NULL DEFAULT '' CHECK (quirk ~ '^[a-z_]{0,60}$'),
    card         jsonb NOT NULL DEFAULT '{}' CHECK (jsonb_typeof(card) = 'object'),     -- likes, dislikes, fears, hopes,
                                                                                         -- topics, styles, marks
    persona      text NOT NULL DEFAULT '' CHECK (char_length(persona) <= 4000),  -- for the Mind in the window (Q1)
    revision     integer NOT NULL DEFAULT 1 CHECK (revision >= 1),               -- rises with any change to it or its items
    written_by   text NOT NULL DEFAULT '' CHECK (char_length(written_by) <= 80), -- 'claude-<batch>', 'atlas', 'dm', 'seed'
    created_at   timestamptz NOT NULL DEFAULT now(),
    updated_at   timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, id)
);
COMMENT ON TABLE live.mantles IS 'Each wolf''s way of talking (doc 84): the card of one mantle; its items are live.mantle_lines.';

CREATE TABLE live.mantle_lines (
    world_id     ratw_id NOT NULL,
    mantle_id    text NOT NULL,
    key          text NOT NULL CHECK (key ~ '^[a-z0-9_.:]{1,120}$'),      -- <section>.<move>.<name> (§7.9), stable
    section      text NOT NULL CHECK (section IN ('any', 'grown', 'young', 'adolescent', 'old')),
    kind         text NOT NULL CHECK (kind IN ('symbol', 'opinion', 'line', 'rule', 'scene', 'story', 'dlg', 'graft')),
    move         text NOT NULL DEFAULT '' CHECK (move ~ '^[a-z0-9_.:]{0,80}$'),  -- a line's `does`; a dlg's or graft's node
    position     integer NOT NULL,                                        -- order within the section
    body         text NOT NULL CHECK (char_length(body) BETWEEN 1 AND 6000),  -- the item in .mantle syntax, as exported
    revision     integer NOT NULL DEFAULT 1 CHECK (revision >= 1),
    updated_at   timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, mantle_id, key),
    FOREIGN KEY (world_id, mantle_id) REFERENCES live.mantles (world_id, id) ON DELETE CASCADE
);
CREATE INDEX mantle_lines_in_order ON live.mantle_lines (world_id, mantle_id, section, position);
COMMENT ON TABLE live.mantle_lines IS 'One item of a mantle (doc 84 §7.9): its text in the .mantle syntax, keyed for diffs and history.';

-- Every change, for audit, review and revert. Append-only; each world keeps its own (never synced, never packaged).
CREATE TABLE live.mantle_history (
    seq          bigserial PRIMARY KEY,
    world_id     ratw_id NOT NULL,
    mantle_id    text NOT NULL,
    key          text NOT NULL DEFAULT '',          -- '' for the card (the live.mantles row)
    revision     integer NOT NULL,                  -- the mantle's revision after the change
    action       text NOT NULL CHECK (action IN ('insert', 'update', 'delete')),
    before       jsonb,
    after        jsonb,
    source       text NOT NULL DEFAULT '',          -- mantle_load:<batch>, atlas, dm, import, pull, migration
    account      text NOT NULL DEFAULT '',          -- who (an Atlas or DM login, or the session that loaded a batch)
    note         text NOT NULL DEFAULT '' CHECK (char_length(note) <= 400),
    at           timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX mantle_history_of ON live.mantle_history (world_id, mantle_id, seq);
COMMENT ON TABLE live.mantle_history IS 'Every change to a mantle or its items (doc 84 §17.1): who, when, from what source, before and after.';

-- On each changed row: stamps updated_at, raises the item's revision, raises its mantle's revision once a transaction (a
-- batch of 60 items is one new revision), and writes one history row, taking source, account and note from the
-- transaction's settings (SET LOCAL ratw.edit_source / ratw.edit_by / ratw.edit_note, which tools/mantle_store.py sets).
-- An update that changes nothing (the same body, section, kind, move and position) is left alone: loading the same files
-- twice changes nothing. SECURITY DEFINER, so nobody needs, or has, a right to write the history directly (as 0059 does
-- for live.game_changes).
CREATE FUNCTION live.mantle_changed() RETURNS trigger LANGUAGE plpgsql SECURITY DEFINER
SET search_path = pg_catalog, pg_temp AS $$
DECLARE
    any_row  record;
    mantle   text;
    flag     text;
    rev      integer;
    old_j    jsonb := CASE WHEN TG_OP <> 'INSERT' THEN to_jsonb(OLD) - 'updated_at' - 'revision' END;
    new_j    jsonb := CASE WHEN TG_OP <> 'DELETE' THEN to_jsonb(NEW) - 'updated_at' - 'revision' END;
    item_key text := '';
BEGIN
    IF TG_OP = 'DELETE' THEN
        any_row := OLD;
    ELSE
        any_row := NEW;
    END IF;
    IF TG_TABLE_NAME = 'mantles' THEN
        mantle := any_row.id;
    ELSE
        mantle := any_row.mantle_id;
        item_key := any_row.key;
    END IF;
    -- (One raise a transaction for each mantle: the flag is local to it.)
    flag := 'ratw.mantle_' || md5(any_row.world_id || '|' || mantle);
    IF TG_TABLE_NAME = 'mantles' THEN
        IF pg_trigger_depth() > 1 THEN
            -- (Raised from an item's change, below, or deleted with its world: no history row of its own.)
            IF TG_OP = 'DELETE' THEN
                RETURN OLD;
            END IF;
            NEW.updated_at := now();
            RETURN NEW;
        END IF;
        IF TG_OP = 'UPDATE' AND (old_j - 'created_at') IS NOT DISTINCT FROM (new_j - 'created_at') THEN
            NEW.revision := OLD.revision;
            RETURN NEW;
        END IF;
        IF TG_OP = 'INSERT' THEN
            PERFORM set_config(flag, '1', true);
            NEW.updated_at := now();
            rev := NEW.revision;
        ELSIF TG_OP = 'UPDATE' THEN
            IF coalesce(current_setting(flag, true), '') = '1' THEN
                NEW.revision := OLD.revision;
            ELSE
                NEW.revision := OLD.revision + 1;
                PERFORM set_config(flag, '1', true);
            END IF;
            NEW.updated_at := now();
            rev := NEW.revision;
        ELSE
            rev := OLD.revision;
        END IF;
    ELSE
        IF TG_OP = 'UPDATE' AND old_j IS NOT DISTINCT FROM new_j THEN
            NEW.revision := OLD.revision;
            NEW.updated_at := OLD.updated_at;
            RETURN NEW;
        END IF;
        IF TG_OP = 'UPDATE' THEN
            NEW.revision := OLD.revision + 1;
        END IF;
        IF TG_OP <> 'DELETE' THEN
            NEW.updated_at := now();
        END IF;
        IF coalesce(current_setting(flag, true), '') = '1' THEN
            SELECT m.revision INTO rev FROM live.mantles m WHERE m.world_id = any_row.world_id AND m.id = mantle;
        ELSE
            UPDATE live.mantles m SET revision = m.revision + 1 WHERE m.world_id = any_row.world_id AND m.id = mantle
                RETURNING m.revision INTO rev;
            PERFORM set_config(flag, '1', true);
        END IF;
        rev := coalesce(rev, 1);
    END IF;
    INSERT INTO live.mantle_history (world_id, mantle_id, key, revision, action, before, after, source, account, note)
    VALUES (any_row.world_id, mantle, item_key, rev, lower(TG_OP),
            CASE WHEN TG_OP <> 'INSERT' THEN to_jsonb(OLD) - 'updated_at' END,
            CASE WHEN TG_OP <> 'DELETE' THEN to_jsonb(NEW) - 'updated_at' END,
            left(coalesce(current_setting('ratw.edit_source', true), ''), 120),
            left(coalesce(current_setting('ratw.edit_by', true), ''), 120),
            left(coalesce(current_setting('ratw.edit_note', true), ''), 400));
    RETURN CASE WHEN TG_OP = 'DELETE' THEN OLD ELSE NEW END;
END $$;
REVOKE ALL ON FUNCTION live.mantle_changed() FROM PUBLIC;

CREATE TRIGGER mantle_changed BEFORE INSERT OR UPDATE OR DELETE ON live.mantles
    FOR EACH ROW EXECUTE FUNCTION live.mantle_changed();
CREATE TRIGGER mantle_changed BEFORE INSERT OR UPDATE OR DELETE ON live.mantle_lines
    FOR EACH ROW EXECUTE FUNCTION live.mantle_changed();

-- The history: written by the trigger only. (The live schema's default privileges give the editor, the publisher and the
-- game writing.) The game reads mantles and never writes them.
REVOKE INSERT, UPDATE, DELETE, TRUNCATE ON live.mantle_history FROM ratw_editor, ratw_publisher, ratw_game, ratw_dm;
REVOKE INSERT, UPDATE, DELETE, TRUNCATE ON live.mantles, live.mantle_lines FROM ratw_game;
GRANT SELECT ON live.mantles, live.mantle_lines, live.mantle_history TO ratw_editor, ratw_publisher, ratw_game, ratw_dm;
GRANT INSERT, UPDATE, DELETE ON live.mantles, live.mantle_lines TO ratw_editor, ratw_publisher, ratw_dm;

-- Who wears which mantle. A binding is the wearer's voice row; a mantle's wearers, in order, are the rows naming it.
ALTER TABLE live.npc_voice
    ADD COLUMN mantle_id    text,
    ADD COLUMN bound_day    double precision,
    ADD COLUMN bound_how    text NOT NULL DEFAULT '' CHECK (bound_how IN ('', 'authored', 'spare', 'reused', 'echo', 'arrival', 'dm')),
    ADD COLUMN released_day double precision,
    -- A mantle once worn is never deleted, only retired.
    ADD CONSTRAINT npc_voice_mantle FOREIGN KEY (world_id, mantle_id) REFERENCES live.mantles (world_id, id)
        ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED;
COMMENT ON COLUMN live.npc_voice.mantle_id IS 'The mantle this resident wears or wore (doc 84); released_day set when they died.';
-- A mantle is worn by one living wolf at a time (goal 1).
CREATE UNIQUE INDEX npc_voice_mantle_worn ON live.npc_voice (world_id, mantle_id)
    WHERE mantle_id IS NOT NULL AND released_day IS NULL;
-- Rows the game writes (births, deaths, arrivals) show in Atlas's conflict list, as live.npcs's do (migration 0059).
CREATE TRIGGER game_changes AFTER INSERT OR UPDATE OR DELETE ON live.npc_voice FOR EACH ROW
    WHEN (current_user = 'ratw_game') EXECUTE FUNCTION live.log_game_change('npc_id');

-- A mantle saved in Atlas (its own role, which can't write dm.actions: doc 78 M7) or by tools/mantle_load.py tells the
-- running server, as live_edit's economy.sync does through dm.queue_economy_sync (0061): mantle.sync <id> (one mantle's
-- rows read again) or voice.sync (every voice; a batch's one action). Only those two kinds, and a target of the shape
-- they take.
CREATE FUNCTION dm.queue_talk_sync(p_who text, p_kind text, p_target text) RETURNS bigint
LANGUAGE plpgsql SECURITY DEFINER SET search_path = pg_catalog, dm AS $$
DECLARE
    queued bigint;
BEGIN
    IF p_kind NOT IN ('mantle.sync', 'voice.sync') THEN
        RAISE EXCEPTION 'queue_talk_sync queues mantle.sync or voice.sync only, not %', p_kind;
    END IF;
    IF p_kind = 'mantle.sync' AND coalesce(p_target, '') !~ '^[a-z]{2}[0-9]{3,4}$' THEN
        RAISE EXCEPTION 'mantle.sync names one mantle';
    END IF;
    IF p_kind = 'voice.sync' AND coalesce(p_target, '') !~ '^[a-z0-9_]{0,80}$' THEN
        RAISE EXCEPTION 'voice.sync names one resident, or nobody';
    END IF;
    INSERT INTO dm.actions (kind, target_id, requested_by, payload)
    VALUES (p_kind, coalesce(p_target, ''), coalesce(nullif(left(p_who, 60), ''), 'Atlas'), '{}'::jsonb)
    RETURNING id INTO queued;
    PERFORM pg_notify('ratw_dm', json_build_object('action', queued)::text);
    RETURN queued;
END
$$;
REVOKE ALL ON FUNCTION dm.queue_talk_sync(text, text, text) FROM PUBLIC;
GRANT EXECUTE ON FUNCTION dm.queue_talk_sync(text, text, text) TO ratw_editor, ratw_dm;
