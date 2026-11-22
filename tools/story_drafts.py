"""World-story drafts kept on the server (Docs/Design/85-story-triggers-and-campaign-control.md, Phase 4, 4.1 decision 5;
migration 0073): many per world, each with a name, an owner and its last saved versions. Ops draft.list, draft.open,
draft.save and draft.delete on /api/dm/story-program, a Dungeon Master's (a viewer's drafts stay in their browser).

A draft is whatever the author has, sound or not: a program the forms cannot show is still kept, and opened in Source.
A save names the version it was made on, and is refused over a newer one (two Dungeon Masters never overwrite each
other unawares)."""

import json

import story_program as rules

KEEP_VERSIONS = 20
MAX_BYTES = 1024 * 1024
LIST_LIMIT = 200


def _ready(conn):
    rules.require(conn.execute("SELECT to_regclass('dm.story_drafts') IS NOT NULL").fetchone()[0],
                  'This world needs migration 0073 to keep drafts on the server; your drafts stay in this browser until then.')


def _iso(moment):
    return moment.isoformat() if moment is not None else None


def _summary(program):
    phases = program.get('phases') if isinstance(program.get('phases'), list) else []
    rules_ = sum(len(p.get('triggers') or []) for p in phases if isinstance(p, dict)) + len(program.get('triggers') or [])
    return {'title': program.get('title') if isinstance(program.get('title'), str) else '', 'phases': len(phases), 'triggers': rules_}


def _view(row, with_program=False):
    draft_id, name, owner, program, version, saved_by, saved_at, created_at = row
    out = {'id': draft_id, 'name': name, 'owner': owner, 'version': version, 'savedBy': saved_by, 'savedAt': _iso(saved_at),
           'createdAt': _iso(created_at), **_summary(program)}
    if with_program:
        out['program'] = program
    return out


COLUMNS = 'id,name,owner,program,version,saved_by,saved_at,created_at'


def draft_list(conn, world):
    _ready(conn)
    rows = conn.execute(f'SELECT {COLUMNS} FROM dm.story_drafts WHERE world_id=%s ORDER BY saved_at DESC, id DESC LIMIT %s',
                        (world, LIST_LIMIT)).fetchall()
    return {'drafts': [_view(r) for r in rows]}


def _row(conn, world, draft_id, lock=False):
    rules.require(type(draft_id) is int and draft_id > 0, 'Choose a draft.')
    row = conn.execute(f'SELECT {COLUMNS} FROM dm.story_drafts WHERE world_id=%s AND id=%s' + (' FOR UPDATE' if lock else ''),
                       (world, draft_id)).fetchone()
    rules.require(row, 'That draft is no longer kept: someone deleted it.')
    return row


def draft_open(conn, world, data):
    _ready(conn)
    row = _row(conn, world, data.get('id'))
    out = _view(row, True)
    versions = conn.execute('SELECT version,saved_by,saved_at FROM dm.story_draft_versions WHERE draft=%s ORDER BY version DESC',
                            (row[0],)).fetchall()
    out['versions'] = [{'version': v, 'savedBy': by, 'savedAt': _iso(at)} for v, by, at in versions]
    wanted = data.get('version')
    if wanted is not None and wanted != row[4]:
        old = conn.execute('SELECT program,saved_by,saved_at FROM dm.story_draft_versions WHERE draft=%s AND version=%s',
                           (row[0], wanted)).fetchone()
        rules.require(old, 'That version is no longer kept (the newest 20 are).')
        out['program'], out['openedVersion'] = old[0], wanted
    return out


def _check(data):
    name = data.get('name')
    rules.require(isinstance(name, str) and 1 <= len(name.strip()) <= 120 and all(ord(c) >= 32 for c in name),
                  'Give the draft a name of up to 120 characters.')
    program = data.get('program')
    rules.require(isinstance(program, dict), 'Only a program (a JSON object) can be saved: fix the JSON in Source first.')
    text = json.dumps(program, ensure_ascii=False, allow_nan=False)
    rules.require(len(text.encode()) <= MAX_BYTES, 'A draft is at most 1 MiB.')
    return name.strip(), text


def draft_save(conn, world, who, data):
    """A new draft (no id), or a new version of one: refused when it was made on an older version than the one kept."""
    _ready(conn)
    name, text = _check(data)
    draft_id = data.get('id')
    if draft_id is None:
        draft_id, version, saved_at = conn.execute(
            'INSERT INTO dm.story_drafts (world_id,name,owner,program,saved_by) VALUES (%s,%s,%s,%s::json,%s) RETURNING id,version,saved_at',
            (world, name, who, text, who)).fetchone()
    else:
        row = _row(conn, world, draft_id, lock=True)
        made_on = data.get('version')
        rules.require(made_on == row[4], f'{row[5]} saved this draft at {row[6]:%H:%M} (version {row[4]}) after you opened it: '
                      'open it again to see their changes, or save yours as a new draft.')
        version, saved_at = conn.execute(
            'UPDATE dm.story_drafts SET name=%s, program=%s::json, version=version+1, saved_by=%s, saved_at=now() '
            'WHERE id=%s RETURNING version,saved_at', (name, text, who, draft_id)).fetchone()
    conn.execute('INSERT INTO dm.story_draft_versions (draft,version,program,saved_by) VALUES (%s,%s,%s::json,%s)',
                 (draft_id, version, text, who))
    conn.execute('DELETE FROM dm.story_draft_versions WHERE draft=%s AND version <= %s', (draft_id, version - KEEP_VERSIONS))
    return {'id': draft_id, 'version': version, 'savedAt': _iso(saved_at), 'name': name}


def draft_delete(conn, world, who, role_rank, data):
    """The draft's owner deletes it; or a builder, the world's owner or an admin (role_rank >= 2)."""
    _ready(conn)
    row = _row(conn, world, data.get('id'), lock=True)
    rules.require(row[2] == who or role_rank >= 2, f'Only {row[2]}, who made this draft, or a builder or the owner may delete it.')
    conn.execute('DELETE FROM dm.story_drafts WHERE id=%s', (row[0],))
    return {'deleted': row[0]}


def request(conn, world, who, role_rank, data):
    op = data.get('op')
    if op == 'draft.list':
        return draft_list(conn, world)
    if op == 'draft.open':
        return draft_open(conn, world, data)
    if op == 'draft.save':
        return draft_save(conn, world, who, data)
    if op == 'draft.delete':
        return draft_delete(conn, world, who, role_rank, data)
    raise ValueError('Unknown draft operation.')


OPERATIONS = ('draft.list', 'draft.open', 'draft.save', 'draft.delete')
