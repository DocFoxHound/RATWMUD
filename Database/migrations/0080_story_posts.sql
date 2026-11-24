-- 0080_story_posts.sql: the careers book's posts as the world was last saved, for the Story Maker (Docs/Design/86-quests-
-- and-the-ninth-howl.md, engine spec C14 and D5, and "Phase 8 as built"): the effect "Give a resident a post" (npc.post)
-- names a post by its id, and approval checks it here, against the last checkpoint, never a live server. A post is
-- job:<the resident who founded it> (Society::buildPositions): its title and role are its founder's as authored (live.npcs,
-- or a profession slot's), its holder the checkpoint's society.careers.positions. A view: refreshed with every checkpoint.
-- (0081, dm.story_realm_vars, is the same phase's.) Additive: nothing that exists is changed.
CREATE VIEW dm.story_posts AS
WITH founders AS (
    SELECT n.world_id, n.id, n.role, n.work_label FROM live.npcs n
    UNION ALL
    SELECT s.world_id, c.id, p.behavior, coalesce(nullif(s.work_label, ''), p.work_label)
    FROM live.profession_slots s
    JOIN live.professions p ON p.id = s.profession
    JOIN live.characters c ON c.status = 'active' AND c.assignment = jsonb_build_object('world', s.world_id, 'slot', s.id)
)
SELECT k.world_id, k.id, coalesce(nullif(f.work_label, ''), k.id) AS title, coalesce(f.role, '') AS role,
       nullif(k.state->>'holder', '') AS holder, nullif(k.state->>'lastHolder', '') AS last_holder
FROM (SELECT c.world_id, p.key AS id, p.value AS state
      FROM game.checkpoints c
      CROSS JOIN LATERAL jsonb_each(CASE WHEN jsonb_typeof(c.payload::jsonb->'society'->'careers'->'positions') = 'object'
                                         THEN c.payload::jsonb->'society'->'careers'->'positions' ELSE '{}'::jsonb END) p) k
LEFT JOIN founders f ON f.world_id = k.world_id AND 'job:' || f.id = k.id;

REVOKE ALL ON dm.story_posts FROM PUBLIC, ratw_game, ratw_editor, ratw_publisher;
GRANT SELECT ON dm.story_posts TO ratw_dm;
