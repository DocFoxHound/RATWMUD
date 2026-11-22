"""World-scoped approval, delivery and deterministic event consumption for DM story programs."""

import argparse
import datetime
import hashlib
import json
import math
import secrets
import time
import uuid
from collections import Counter
from pathlib import Path

import story_library
import story_program as rules
import story_scenes

DATA = Path(__file__).resolve().parents[1] / 'Data'
EXAMPLES = DATA / 'Storylines/programs'
EVENT_COLUMNS = ('id', 'kind', 'day', 'actor', 'target', 'cell', 'item', 'quantity', 'coins', 'detail', 'x', 'y')


def world_id(conn):
    rows = conn.execute('SELECT id FROM world.worlds LIMIT 2').fetchall()
    rules.require(len(rows) == 1, 'Select a database containing exactly one game world.')
    return rows[0][0]


def catalogue():
    return {
        'blank': story_library.blank(),
        'sceneExamples': story_scenes.examples(),
        'sceneOperations': story_scenes.OPERATIONS,
        'events': rules.EVENTS,
        'missionEvents': rules.MISSION_EVENTS,
        'missionMeasures': rules.MISSION_MEASURES,
        'eventTypes': [{**e, 'components': story_library.components(e)} for e in story_library.entries()],
        'triggerRules': {
            'events': [e for e in rules.EVENTS if e != 'dm signal'] + [rules.triggers.SIGNAL],
            'conditions': ['all', 'any', 'not', 'counter', 'field', 'mission', 'region'],
            'comparisons': list(rules.triggers.COMPARE),
            'actions': ['set', 'add', 'emit', 'schedule', 'cancel', 'effect', 'enter'],
            'scopes': ['story', 'player'],
            'timerOwners': ['story', 'player'],
            'timerClocks': ['game', 'real'],
            'timerLifetimes': ['visit', 'campaign'],
            'repeat': ['each', 'visit', 'player', 'campaign', 'member'],
        },
        'filters': rules.FIELDS,
        'effects': rules.EFFECTS,
        'injuryTypes': rules.INJURY_TYPES,
        # Every event, condition, action and effect with typed parameters and a sentence (doc 85, Phase 4): the Story
        # Creator's workbench draws its sentences and pickers from it.
        'catalogue': rules.CATALOGUE,
        'examples': {f.stem: json.loads(f.read_text()) for f in sorted(EXAMPLES.glob('*.json'))},
    }


def ticket_hash(ticket):
    rules.require(isinstance(ticket, str) and 1 <= len(ticket) <= 128, 'Missing approval ticket.')
    return hashlib.sha256(ticket.encode()).hexdigest()


def game_day(conn, world):
    row = conn.execute(
        "SELECT (payload::jsonb->>'calendarDays')::float8 FROM game.checkpoints WHERE world_id=%s", (world,)
    ).fetchone()
    rules.require(row and row[0] is not None, 'The world needs its first saved clock.')
    return row[0]


def check_tile(conn, world, cell, x, y):
    dimensions = conn.execute(
        'SELECT width,height FROM world.cells WHERE world_id=%s AND id=%s '
        'UNION ALL SELECT width,height FROM world.interiors WHERE world_id=%s AND id=%s',
        (world, cell, world, cell),
    ).fetchone()
    rules.require(dimensions and x < dimensions[0] and y < dimensions[1], f'Tile ({x},{y}) lies outside {cell}.')


def references(conn, world, program):
    for r in program.get('regions', []):
        check_tile(conn, world, r['cell'], r['x2'], r['y2'])
    if any(ph.get('triggers') for ph in rules.phases_of(program)):
        rules.require(
            conn.execute("SELECT to_regclass('dm.story_trigger_history')").fetchone()[0],
            'Apply migration 0069 before approving trigger rules.',
        )
    tables = {
        'npc': ('dm.story_residents', 'id'),
        'resident': ('dm.story_residents', 'id'),
        'player': ('game.characters', 'key'),
        'faction': ('live.factions', 'id'),
        'other': ('live.factions', 'id'),
        'place': ('world.areas', 'id'),
        'cell': ('world.areas', 'id'),
    }
    refs = {
        (key, value)
        for s in rules.phases_of(program)
        for e in rules.possible_effects(s)
        for key, value in e.items()
        if key in tables and not (e['kind']=='scene.play' and key=='player' and value=='world')
    }
    for scene in program.get('scenes', []):
        if 'area' in scene:
            area = scene['area']
            check_tile(conn, world, area['cell'], area['x2'], area['y2'])
        if 'camera' in scene:
            camera = scene['camera']
            check_tile(conn, world, camera['cell'], camera['x'], camera['y'])
        for role in scene['cast']:
            if 'npc' in role:
                refs.add(('npc', role['npc']))
            elif 'cell' in role['selector']:
                refs.add(('cell', role['selector']['cell']))
        for beat in scene['beats']:
            if beat['type'] == 'move':
                check_tile(conn, world, beat['cell'], beat['x'], beat['y'])
    refs.update(('npc', npc) for s in rules.phases_of(program) for npc in s.get('guestVoices', []))
    refs.update(('player', p) for p in program['audience'].get('players', []))
    if any(phase.get('missions') for phase in rules.phases_of(program)):
        rules.require(
            conn.execute(
                "SELECT 1 FROM information_schema.columns WHERE table_schema='dm' AND "
                "table_name='story_transitions' AND column_name='missions'"
            ).fetchone(),
            'Apply database migration 0068 before approving campaign missions.',
        )
    for phase in rules.phases_of(program):
        evidence = [o for m in phase.get('missions', []) for o in m['objectives']]
        for objective in [*evidence, *phase.get('triggers', [])]:
            filters = objective.get('match', {})
            if filters.get('cell'):
                refs.add(('cell', filters['cell']))
            if objective['on'] == 'enter place' and filters.get('target'):
                refs.add(('place', filters['target']))
            if objective['on'] in ('conversation', 'gift', 'gift again') and filters.get('target'):
                refs.add(('character', filters['target']))
    for key, value in sorted(refs):
        if key == 'character':
            rules.require(
                conn.execute(
                    'SELECT 1 FROM dm.story_residents WHERE world_id=%s AND id=%s '
                    'UNION ALL SELECT 1 FROM game.characters WHERE world_id=%s AND key=%s',
                    (world, value, world, value),
                ).fetchone(),
                f'Unknown mission character in this world: {value}',
            )
            continue
        if value in ('$actor', '$each'):
            continue
        table, column = tables[key]
        rules.require(
            conn.execute(f'SELECT 1 FROM {table} WHERE world_id=%s AND {column}=%s', (world, value)).fetchone(),
            f'Unknown {key} in this world: {value}',
        )
    for s in rules.phases_of(program):
        for e in rules.possible_effects(s):
            if e['kind'] == 'storyline.give':
                rules.require(
                    e['template']
                    in {
                        t['id']
                        for t in json.loads((DATA / 'Storylines/templates.json').read_text())['templates']
                        if t.get('source') == 'dm'
                    },
                    'Unknown DM quest template.',
                )
            if e['kind'] == 'npc.walk':
                check_tile(conn, world, e['cell'], e['x'], e['y'])
            if e['kind'] == 'story.area.item':
                rules.require(
                    e['item'] in {item['id'] for item in json.loads((DATA / 'Items/items.json').read_text())['items']},
                    'Unknown item for a region grant.',
                )
            if e['kind'] == 'project.post':
                rules.require(
                    e['project'] in {k['id'] for k in json.loads((DATA / 'Town/projects.json').read_text())['kinds']},
                    'Unknown town project.',
                )


def approve(conn, world, who, program, digest, for_run=None):
    report = rules.review(program)
    rules.require(
        report['passed'] and digest == report['digest'], 'Rehearse this exact version successfully before approval.'
    )
    rules.require(not report['uncovered'], 'Rehearse the remaining phases: ' + ', '.join(report['uncovered']))
    references(conn, world, program)
    if for_run is not None:
        return approve_version(conn, world, who, program, digest, for_run)
    run, ticket = 'world-' + uuid.uuid4().hex, secrets.token_urlsafe(32)
    conn.execute(
        """INSERT INTO dm.story_releases(id,world_id,digest,program,approved_by,ticket_hash,status)
                    VALUES (%s,%s,%s,%s::jsonb,%s,%s,'approved')""",
        (run, world, digest, json.dumps(program), who, ticket_hash(ticket)),
    )
    return {'run': run, 'ticket': ticket, 'digest': digest}


# A dry run of a console command (doc 85, 5.5: "this will do") records the orders it would send; the transaction it runs in
# is rolled back, so nothing it queued reaches the game.
import contextvars as _contextvars
_recording = _contextvars.ContextVar('story_orders_recorded', default=None)


def queue(conn, who, kind, target, payload):
    recording = _recording.get()
    if recording is not None:
        recording.append({'kind': kind, 'target': target, 'payload': payload})
    receipt = conn.execute(
        """INSERT INTO dm.actions(kind,target_id,payload,requested_by)
                           VALUES (%s,%s,%s::jsonb,%s) RETURNING id""",
        (kind, target, json.dumps(payload), who),
    ).fetchone()[0]
    conn.execute("SELECT pg_notify('ratw_dm',%s)", (json.dumps({'action': receipt}),))
    return receipt


def entry_effects(phase, state):
    """A visit's entry effects as the runner sends them (none when an admin entered it "without its effects")."""
    # (After a move onto a new version, doc 85 Phase 5b, the visit's entry effects are those it already sent: none more.)
    return [] if state.get('quietEntry') or state.get('entrySent') else [e for e in phase['effects'] if e['kind'] != 'npc.line']


def enter(conn, run, program, who, state, cursor, cause, quiet=False):
    phase = rules.phase_of(program, state)
    state.pop('arrivalEffects', None)
    state.pop('skipped', None)
    state.pop('quietEntry', None)
    state.pop('entrySent', None)
    if quiet:
        state['quietEntry'] = True
    state['visitCursor'] = cursor   # (Doc 85 Phase 3: "why not?" tells this visit's events from the last one's by it.)
    if state.get('triggerEffects'):
        state['arrivalEffects'] = state.pop('triggerEffects')
    previous = state.pop('previousMissions', [])
    if previous:
        conn.execute(
            'UPDATE dm.story_transitions SET missions=%s::jsonb WHERE run=%s AND visit=%s',
            (json.dumps(previous), run, state['visits'] - 1),
        )
    opening = rules.mission_snapshot(phase, state)
    receipt = queue(
        conn,
        who,
        'story.phase',
        run,
        {
            'phase': state['phase'],
            'journal': phase['journal'],
            'title': program['title'],
            'players': state['cohort'],
            'visit': state['visits'],
            'end': not (phase['exits'] or phase.get('triggers'))
            and not entry_effects(phase, state)
            and not state.get('arrivalEffects'),
            'guestVoices': phase.get('guestVoices', []),
            'missions': opening,
            'previousMissions': previous,
            'regions': program.get('regions', []),
            'lines': [e for e in phase['effects'] if e['kind'] == 'npc.line'],
        },
    )
    conn.execute(
        """UPDATE dm.story_releases SET state=%s::jsonb,cursor=%s,receipts=ARRAY[%s]::bigint[],status='running'
                    WHERE id=%s""",
        (json.dumps(state), cursor, receipt, run),
    )
    conn.execute(
        'INSERT INTO dm.story_transitions(run,visit,phase,game_day,cause,receipts) VALUES (%s,%s,%s,%s,%s,ARRAY[%s]::bigint[])',
        (run, state['visits'], state['phase'], state['entered'], cause, receipt),
    )

    if opening:
        conn.execute(
            'UPDATE dm.story_transitions SET missions=%s::jsonb WHERE run=%s AND visit=%s',
            (json.dumps(opening), run, state['visits']),
        )


def publish(conn, world, ticket):
    row = conn.execute(
        """SELECT id,program,approved_by,status,expires_at>now() FROM dm.story_releases
                          WHERE ticket_hash=%s AND world_id=%s FOR UPDATE""",
        (ticket_hash(ticket), world),
    ).fetchone()
    rules.require(row, 'Unknown ticket for this world.')
    run, program, who, status, valid = row
    if status != 'approved':
        return {'run': run, 'status': status}
    rules.require(valid, 'Approval expired. Approve the story again.')
    # Serialize two different approvals of the same story in this world.
    conn.execute('SELECT pg_advisory_xact_lock(hashtext(%s),hashtext(%s))', (world, program['id']))
    other = conn.execute(
        """SELECT id FROM dm.story_releases WHERE world_id=%s AND program->>'id'=%s
                            AND status IN ('running','paused','held','stopping')""",
        (world, program['id']),
    ).fetchone()
    rules.require(not other, 'This story already has a live run.')
    references(conn, world, program)
    a = program['audience']
    # Read the calendar and event watermark from a single MVCC snapshot.
    snapshot = conn.execute(
        """SELECT (payload::jsonb->>'calendarDays')::float8,
                              (SELECT coalesce(max(id),0) FROM game.events WHERE world_id=%s), extract(epoch from statement_timestamp())::float8
                              FROM game.checkpoints WHERE world_id=%s""",
        (world, world),
    ).fetchone()
    rules.require(snapshot and snapshot[0] is not None, 'The world needs its first saved clock.')
    day, cursor, published_at = snapshot
    cohort = a.get('players')
    if cohort is None:
        cohort = [
            r[0]
            for r in conn.execute(
                """SELECT DISTINCT e.actor FROM game.events e JOIN game.characters c
                    ON c.world_id=e.world_id AND c.key=e.actor WHERE e.world_id=%s AND e.id<=%s AND
                    e.recorded_at >= now() - %s * interval '1 day' AND e.kind=ANY(%s) LIMIT 501""",
                (
                    world,
                    cursor,
                    a['days'],
                    [
                        'arrival',
                        'enter place',
                        'departure',
                        'conversation',
                        'gift',
                        'gift again',
                        'contract done',
                        'festival entered',
                        'project gift',
                    ],
                ),
            )
        ]
    rules.require(0 < len(cohort) <= 500, 'The opening audience needs 1–500 eligible characters.')
    for phase in rules.phases_of(program):
        for e in rules.possible_effects(phase):
            if e['kind'] == 'scene.play' and e['player'] not in ('$actor', '$each', 'world'):
                rules.require(e['player'] in cohort, f'Scene initiator {e["player"]} must be in the opening audience.')
        exits = rules.triggers.routes(phase)
        rules.require(
            not exits
            or any(
                edge.get('players', 1) <= len(cohort)
                and all(
                    max(m.get('players', 1), m.get('contributors', 2) if m['mode'] == 'group' else 1) <= len(cohort)
                    for m in phase.get('missions', [])
                    if m['id'] in edge.get('missions', [])
                )
                for edge in exits
            ),
            f'Phase {phase["id"]} needs more distinct players than the opening audience contains.',
        )
    enter(
        conn,
        run,
        program,
        who,
        {**rules.begin(program, cohort, day, run), 'publishedAt': published_at},
        cursor,
        'published',
    )
    return {'run': run, 'status': 'running', 'eligible': len(cohort)}


def action(effect, actor):
    kind = effect['kind']
    key = {
        'bandits.call': 'player',
        'story.practice': 'player',
        'npc.move': 'npc',
        'npc.protect': 'npc',
        'npc.unprotect': 'npc',
        'npc.revive': 'npc',
        'character.injury': 'player',
        'ship.storm': 'ship',
        'harvest.override': 'area',
        'band.override': 'npc',
        'festival.call': 'town',
        'deed.award': 'player',
        'storyline.give': 'player',
        'project.post': 'town',
        'times.nudge': 'market',
    }[kind]
    target = effect[key]
    payload = {k: v for k, v in effect.items() if k not in ('kind', key)}
    if kind == 'storyline.give':
        payload = {'template': effect['template'], 'cast': {'resident': effect['resident'], 'place': effect['place']}}
    if kind == 'project.post':
        payload['kind'] = payload.pop('project')
    return kind, actor if target == '$actor' else target, payload


def write_relation(conn, w, faction_id, other_id, disposition, stance, reason, username):
    conn.execute(
        """INSERT INTO live.faction_relations (world_id, faction_id, other_id, disposition, stance, reason, updated_by)
                    VALUES (%s, %s, %s, %s, %s, %s, %s)
                    ON CONFLICT (world_id, faction_id, other_id) DO UPDATE SET disposition = excluded.disposition,
                        stance = excluded.stance, reason = excluded.reason, updated_by = excluded.updated_by, updated_at = now()""",
        (w, faction_id, other_id, disposition, stance, reason, username),
    )
    conn.execute(
        """INSERT INTO live.faction_relation_log (world_id, faction_id, other_id, disposition, stance, reason, by)
                    VALUES (%s, %s, %s, %s, %s, %s, %s)""",
        (w, faction_id, other_id, disposition, stance, reason, username),
    )


def submit_effect(conn, world, run, who, state, effect, phase):
    kind = effect['kind']
    if kind == 'scene.play':
        player = state['actor'] if effect['player'] == '$actor' else effect['player']
        return queue(
            conn,
            who,
            'story.scene',
            run,
            {
                'scene': phase['_scenes'][effect['scene']],
                'cue': effect['cue'],
                'player': player,
                'visit': state['visits'],
            },
        )
    if kind in rules.SCENE_EFFECTS:
        payload = {k: v for k, v in effect.items() if k not in ('kind', 'npc')}
        return queue(conn, who, kind, effect['npc'], {**payload, 'run': run, 'visit': state['visits']})
    if kind.startswith('story.area.'):
        bounds = phase['_regions'][effect['region']]
        payload = {k: v for k, v in bounds.items() if k != 'id'}
        payload.update({k: v for k, v in effect.items() if k != 'kind'})
        return queue(conn, who, kind.replace('story.area.', 'story.area_'), run, {**payload, 'visit': state['visits']})
    if effect['kind'] == 'faction.relation':
        faction, other = effect['faction'], effect['other']
        found = conn.execute(
            'SELECT id FROM live.factions WHERE world_id=%s AND id IN (%s,%s) FOR KEY SHARE', (world, faction, other)
        ).fetchall()
        if len(found) != 2:
            conn.execute(
                "UPDATE dm.story_releases SET status='paused',problem='A faction no longer exists.' WHERE id=%s",
                (run,),
            )
            return None
        write_relation(conn, world, faction, other, effect['disposition'], effect['stance'], effect['reason'], who)
        receipt = queue(conn, who, 'factions.sync', faction, {'story': run, 'other': other})
    elif effect['kind'] == 'story.finish':
        receipt = queue(conn, who, 'story.finish', run, {})
    else:
        receipt = queue(conn, who, *action(effect, state['actor']))
    return receipt


def delivered(conn, world, run, phase, state, who, receipts):
    status, result = conn.execute('SELECT status,result FROM dm.actions WHERE id=%s', (receipts[-1],)).fetchone()
    if status != 'applied' and receipts[-1] not in state.get('skipped', ()):
        if status != 'queued':
            conn.execute(
                "UPDATE dm.story_releases SET status='paused',problem=%s WHERE id=%s",
                (f'Action {receipts[-1]} {status}: {result}', run),
            )
        return False
    effects = [*state.get('arrivalEffects', []), *entry_effects(phase, state)]
    if effects and not (phase['exits'] or phase.get('triggers')):
        effects.append({'kind': 'story.finish'})
    if len(receipts) > len(effects):
        return True
    effect = effects[len(receipts) - 1]
    if effect['kind'] == 'story.finish':
        # Acceptance starts playback. Keep this visit open until every terminal presentation has ended.
        pending = Counter(e['cue'] for e in effects if e['kind'] == 'scene.play')
        if pending:
            rows = conn.execute(
                """SELECT detail,count(*) FROM game.events WHERE world_id=%s AND target=%s AND quantity=%s
                       AND kind IN ('scene finished','scene failed') AND detail=ANY(%s) GROUP BY detail""",
                (world, run, state['visits'], list(pending)),
            )
            if pending - Counter(dict(rows)):
                return False
    receipt = submit_effect(conn, world, run, who, state, effect, phase)
    if receipt is None:
        return False
    conn.execute(
        'UPDATE dm.story_releases SET receipts=array_append(receipts,%s::bigint) WHERE id=%s', (receipt, run)
    )
    conn.execute(
        'UPDATE dm.story_transitions SET receipts=array_append(receipts,%s::bigint) WHERE run=%s AND visit=%s',
        (receipt, run, state['visits']),
    )
    return False


# ----- world variables (doc 85, 3.2: the `world` scope, kept after a run ends and shared by every story of the world)

def _world_names(program):
    return {n: d for n, d in rules.variables_of(program).items() if d['scope'] == 'world'} if program.get('version') == 2 else {}


def world_load(conn, world, program, state):
    """The world's values of the variables a version 2 story declares in the `world` scope, into the run's state (a value
    kept under another type by another story is not read: it has its first value here). The values as loaded, to save
    only what changed."""
    names = _world_names(program)
    if not names or not conn.execute("SELECT to_regclass('dm.story_world_vars')").fetchone()[0]:
        return None
    rows = conn.execute('SELECT name,type,value FROM dm.story_world_vars WHERE world_id=%s AND name=ANY(%s)',
                        (world, sorted(names))).fetchall()
    store = {name: value for name, kind, value in rows if names[name]['type'] == kind}
    counters = state.setdefault('counters', {})
    if store:
        counters['world'] = store
    else:
        counters.pop('world', None)
        if not counters:
            state.pop('counters')
    return json.loads(json.dumps(store))


def world_save(conn, world, run, program, state, loaded):
    """Writes back the world variables a tick or a command changed."""
    if loaded is None:
        return
    names = _world_names(program)
    now = state.get('counters', {}).get('world', {})
    for name in sorted(names):
        if name in now and now[name] != loaded.get(name, rules.triggers.MISSING):
            conn.execute(
                """INSERT INTO dm.story_world_vars(world_id,name,type,value,run) VALUES (%s,%s,%s,%s::jsonb,%s)
                   ON CONFLICT (world_id,name) DO UPDATE SET type=excluded.type,value=excluded.value,run=excluded.run,updated_at=now()""",
                (world, name, names[name]['type'], json.dumps(now[name]), run))
            loaded[name] = now[name]


def tick_run(conn, world, run, clock):
    holder = {}
    try:
        _tick_run(conn, world, run, clock, holder)
    finally:
        if holder.get('loaded') is not None:
            world_save(conn, world, run, holder['program'], holder['state'], holder['loaded'])


def _finish(conn, run, who, state):
    """Version 2's finish: the story ended by its own action, once every effect before it went. Tells the game the story
    is over (as an ending phase does), then the run is complete."""
    if state.get('finishSent'):
        conn.execute("UPDATE dm.story_releases SET status='complete',state=%s::jsonb WHERE id=%s", (json.dumps(state), run))
        return
    # (The game ends the storyline "done", or "abandoned" for a story abandoned; the outcome and its line go with the order.)
    kind = 'story.stop' if state['outcome']['outcome'] == 'abandoned' else 'story.finish'
    receipt = queue(conn, who, kind, run, {'outcome': state['outcome']['outcome'], 'text': state['outcome']['text']})
    state.update(triggerReceipt=receipt, finishSent=True)
    conn.execute('UPDATE dm.story_releases SET state=%s::jsonb WHERE id=%s', (json.dumps(state), run))
    conn.execute('UPDATE dm.story_transitions SET receipts=array_append(receipts,%s::bigint) WHERE run=%s AND visit=%s',
                 (receipt, run, state['visits']))


def _tick_run(conn, world, run, clock, holder):
    row = conn.execute(
        """SELECT program,state,cursor,approved_by,receipts FROM dm.story_releases
                          WHERE world_id=%s AND id=%s AND status='running' FOR UPDATE SKIP LOCKED""",
        (world, run),
    ).fetchone()
    if not row:
        return
    p, state, cursor, who, receipts = row
    if 'moveReceipt' in state:
        return   # (Moving onto a new version: nothing is read until the game has the new version's words; settle_move.)
    holder.update(program=p, state=state, loaded=world_load(conn, world, p, state))
    phase = rules.phase_of(p, state)
    receipt_key = next((key for key in ('missionReceipt', 'triggerReceipt') if key in state), None)
    if receipt_key:
        pending = state[receipt_key]
        delivery, message = conn.execute('SELECT status,result FROM dm.actions WHERE id=%s', (pending,)).fetchone()
        if delivery != 'applied' and pending not in state.get('skipped', ()):
            if delivery != 'queued':
                conn.execute(
                    "UPDATE dm.story_releases SET status='paused',problem=%s WHERE id=%s",
                    (f'Story action {pending} {delivery}: {message}', run),
                )
            return
        del state[receipt_key]
        conn.execute('UPDATE dm.story_releases SET state=%s::jsonb WHERE id=%s', (json.dumps(state), run))
    if not delivered(conn, world, run, phase, state, who, receipts):
        return
    if state.get('triggerEffects'):
        effect = state['triggerEffects'].pop(0)
        receipt = submit_effect(conn, world, run, who, state, effect, phase)
        if receipt is not None:
            state['triggerReceipt'] = receipt
            conn.execute('UPDATE dm.story_releases SET state=%s::jsonb WHERE id=%s', (json.dumps(state), run))
            conn.execute(
                'UPDATE dm.story_transitions SET receipts=array_append(receipts,%s::bigint) WHERE run=%s AND visit=%s',
                (receipt, run, state['visits']),
            )
        return
    if state.get('outcome'):
        _finish(conn, run, who, state)
        return
    if not (phase['exits'] or phase.get('triggers')):
        conn.execute("UPDATE dm.story_releases SET status='complete' WHERE id=%s", (run,))
        return
    # The clock read precedes the log read: an older query can never advance past a newly committed backlog.
    has_clock = bool(state.get('timers')) or bool(state.get('waiting')) or any(
        e['on'] == 'clock' for e in [*phase['exits'], *phase.get('triggers', [])]
    )
    day = clock() if has_clock else None
    prior_clock = json.dumps([state.get('fired'), state.get('timers'), state.get('waiting')]) if has_clock else None
    published_at = state.get('publishedAt', 0)
    wall_elapsed = max(0, (time.time() - published_at) / 86400) if published_at else 0
    rows = conn.execute(
        """SELECT id,kind,game_day,actor,target,cell,item,quantity,coins,detail,x,y,extract(epoch from recorded_at)::float8 FROM game.events
                           WHERE world_id=%s AND id>%s ORDER BY id LIMIT 128""",
        (world, cursor),
    ).fetchall()
    before = json.dumps(state.get('missions', {})) if phase.get('missions') else None

    def dispatch(fact, day, elapsed):
        trace = []
        visit = state['visits']
        before = _values(state, fact.get('actor', ''))
        moved = rules.advance(p, state, fact, day, trace, elapsed)
        if trace:
            _attach_changes(phase, trace, before, _values(state, fact.get('actor', '')))
            conn.execute(
                """INSERT INTO dm.story_trigger_history(run,visit,entry)
                         SELECT %s,%s,entry FROM jsonb_array_elements(%s::jsonb) WITH ORDINALITY AS t(entry,n) ORDER BY n""",
                (run, visit, json.dumps(trace)),
            )
        return moved

    for row in rows:
        fact = dict(zip(EVENT_COLUMNS, row))
        elapsed = max(0, (row[-1] - published_at) / 86400) if published_at else 0
        if dispatch({'kind': 'clock'}, fact['day'], elapsed):
            enter(conn, run, p, who, state, cursor, 'clock')
            return
        if state.get('triggerEffects') or state.get('outcome'):
            break
        cursor = fact['id']
        if fact['kind'] in ('dm signal', 'clock', rules.triggers.SIGNAL) or (
            fact['kind'] == 'story phase' and fact['actor'] == run
        ):
            continue
        if fact['kind'] in rules.SCENE_EVENTS and (fact['target'] != run or fact['quantity'] != state['visits']):
            continue
        if fact['kind'] in rules.REGION_EVENTS and fact['detail'] != run:
            continue
        if dispatch(fact, fact['day'], elapsed):
            enter(conn, run, p, who, state, cursor, 'event ' + str(cursor))
            return
        if state.get('triggerEffects') or state.get('outcome'):
            break
    if (
        not state.get('triggerEffects')
        and not state.get('outcome')
        and day is not None
        and len(rows) < 128
        and dispatch({'kind': 'clock'}, day, wall_elapsed)
    ):
        enter(conn, run, p, who, state, cursor, 'clock')
    elif rows or state.get('outcome') or (has_clock and prior_clock != json.dumps([state.get('fired'), state.get('timers'), state.get('waiting')])):
        if before is not None and before != json.dumps(state.get('missions', {})):
            snapshot = rules.mission_snapshot(phase, state)
            receipt = queue(conn, who, 'story.missions', run, {'visit': state['visits'], 'missions': snapshot})
            state['missionReceipt'] = receipt
            conn.execute(
                'UPDATE dm.story_transitions SET missions=%s::jsonb,receipts=array_append(receipts,%s::bigint) WHERE run=%s AND visit=%s',
                (json.dumps(snapshot), receipt, run, state['visits']),
            )
        conn.execute(
            'UPDATE dm.story_releases SET state=%s::jsonb,cursor=%s WHERE id=%s', (json.dumps(state), cursor, run)
        )


def tick(conn, world):
    steering = conn.execute("SELECT to_regclass('dm.story_commands') IS NOT NULL").fetchone()[0]
    if steering:
        heartbeat(conn, world)
    runs = conn.execute(
        """SELECT id,cursor,state->>'visits' FROM dm.story_releases r WHERE world_id=%s AND (status IN ('running','stopping')
           OR (%s AND status IN ('paused','held') AND EXISTS (SELECT 1 FROM dm.story_commands c WHERE c.run=r.id AND c.status='queued'))
           OR (status IN ('paused','held') AND state ? 'moveReceipt'))
           ORDER BY id""",
        (world, steering),
    ).fetchall()
    day = None

    def clock():
        nonlocal day
        if day is None:
            day = game_day(conn, world)
        return day

    import psycopg

    for run, cursor, visit in runs:
        try:
            with conn.transaction():
                if steering:
                    settle_move(conn, run)
                    take_commands(conn, world, run, clock)
                    stopping(conn, run)
                tick_run(conn, world, run, clock)
        except (psycopg.errors.DeadlockDetected, psycopg.errors.SerializationFailure):
            continue  # The poll can retry the transaction with an unchanged cursor.
        except (
            ValueError,
            KeyError,
            TypeError,
            IndexError,
            StopIteration,
            psycopg.IntegrityError,
            psycopg.DataError,
        ) as error:
            # A damaged release must not block the other campaigns. Persist the pause only if a second
            # worker hasn't already advanced this run. Connection failures still reach the supervisor.
            with conn.transaction():
                conn.execute(
                    """UPDATE dm.story_releases SET status='paused',problem=%s
                             WHERE id=%s AND world_id=%s AND status='running' AND cursor=%s
                             AND state->>'visits' IS NOT DISTINCT FROM %s""",
                    (
                        'Cannot process this story: ' + type(error).__name__ + ' ' + str(error)[:240],
                        run,
                        world,
                        cursor,
                        visit,
                    ),
                )


def direct(conn, world, op, data):
    if op in ('signal', 'voice'):
        permit = conn.execute(
            """SELECT s.run,s.visit,s.signal,s.used,s.expires_at>now(),s.receipt,s.granted_by FROM dm.story_signals s
                    JOIN dm.story_releases r ON r.id=s.run WHERE r.world_id=%s AND s.ticket_hash=%s AND s.kind=%s FOR UPDATE OF s""",
            (world, ticket_hash(data.get('ticket')), op),
        ).fetchone()
        rules.require(permit, 'Unknown direction ticket for this world.')
        run, visit, signal, used, valid, receipt, grantor = permit
        if used:
            return {'run': run, 'status': 'already delivered', 'receipt': receipt}
        rules.require(valid, 'Direction ticket expired.')
    else:
        run = data.get('run', '')
        signal = data.get('npc' if op == 'voice-ticket' else 'signal', '')
    row = conn.execute(
        """SELECT program,state,status,cursor,approved_by,receipts FROM dm.story_releases
                          WHERE world_id=%s AND id=%s FOR UPDATE""",
        (world, run),
    ).fetchone()
    rules.require(row, 'Unknown run.')
    p, state, status, cursor, who, receipts = row
    pending_key = next((k for k in ('missionReceipt', 'triggerReceipt') if state and k in state), None)
    if op == 'retry':
        rules.require(status == 'paused' and receipts, 'Choose a paused run with a refused action.')
        previous = conn.execute(
            'SELECT status,kind,target_id,payload FROM dm.actions WHERE id=%s',
            (state[pending_key] if pending_key else receipts[-1],),
        ).fetchone()
        rules.require(previous and previous[0] in ('refused', 'expired', 'applied'),
                      'Only a refused or expired game action can be retried.')
        if previous[0] == 'applied':
            # The game answered after the pause (an act applied before a restart is answered from its journal): the
            # story carries on from that receipt, and nothing is sent twice.
            conn.execute("UPDATE dm.story_releases SET status='running',problem='' WHERE id=%s", (run,))
            return {'run': run, 'status': 'running', 'receipt': state[pending_key] if pending_key else receipts[-1]}
        # Expired: no server took it within ten minutes (the world was down), so it was never applied; send it again.
        receipt = queue(conn, data['operator'], *previous[1:])
        if pending_key:
            state[pending_key] = receipt
            conn.execute('UPDATE dm.story_releases SET state=%s::jsonb WHERE id=%s', (json.dumps(state), run))
        else:
            receipts[-1] = receipt
        conn.execute(
            "UPDATE dm.story_releases SET status='running',problem='',receipts=%s WHERE id=%s", (receipts, run)
        )
        conn.execute(
            'UPDATE dm.story_transitions SET receipts=array_append(receipts,%s::bigint) WHERE run=%s AND visit=%s',
            (receipt, run, state['visits']),
        )
        return {'run': run, 'status': 'running', 'receipt': receipt}
    if op == 'stop':
        if status == 'stopped':
            return {'run': run, 'status': 'stopped'}
        rules.require(status in ('approved', 'running', 'paused'), 'This run has ended.')
        if state:
            receipt = queue(conn, data['operator'], 'story.stop', run, {})
            conn.execute(
                'UPDATE dm.story_releases SET receipts=array_append(receipts,%s::bigint) WHERE id=%s', (receipt, run)
            )
        conn.execute("UPDATE dm.story_releases SET status='stopped' WHERE id=%s", (run,))
        return {'run': run, 'status': 'stopped'}
    rules.require(status == 'running', 'The run must be running.')
    phase = rules.phase_of(p, state)
    voice = op in ('voice', 'voice-ticket')
    if voice:
        rules.require(signal in phase.get('guestVoices', []), 'This phase has no guest voice for that resident.')
    else:
        rules.require(
            any(e['on'] == 'dm signal' and e['match']['detail'] == signal for e in phase['exits']),
            'No such direction in this phase.',
        )
    rules.require(
        not pending_key
        and not state.get('triggerEffects')
        and len(receipts)
        == 1 + len(state.get('arrivalEffects', [])) + sum(e['kind'] != 'npc.line' for e in phase['effects'])
        and conn.execute('SELECT status FROM dm.actions WHERE id=%s', (receipts[-1],)).fetchone()[0] == 'applied',
        'Wait for the game to acknowledge the phase and its effects.',
    )
    if op in ('voice-ticket', 'signal-ticket'):
        token = secrets.token_urlsafe(32)
        conn.execute(
            'INSERT INTO dm.story_signals(ticket_hash,run,visit,signal,kind,granted_by) VALUES (%s,%s,%s,%s,%s,%s)',
            (ticket_hash(token), run, state['visits'], signal, 'voice' if voice else 'signal', data['operator']),
        )
        return {'run': run, 'signal': signal, 'ticket': token}
    rules.require(state['visits'] == visit, 'The story has moved past this direction.')
    if voice:
        text = data.get('text')
        rules.require(
            rules.word(text, 200) and '\n' not in text, 'Supply a single spoken line of up to 200 characters.'
        )
        receipt = queue(conn, grantor, 'story.voice', run, {'npc': signal, 'text': text, 'visit': visit})
        conn.execute(
            'UPDATE dm.story_signals SET used=true,receipt=%s WHERE ticket_hash=%s',
            (receipt, ticket_hash(data['ticket'])),
        )
        return {'run': run, 'receipt': receipt}
    day = game_day(conn, world)
    pending = conn.execute(
        'SELECT 1 FROM game.events WHERE world_id=%s AND id>%s LIMIT 1', (world, cursor)
    ).fetchone()
    rules.require(not pending, 'World events are waiting. Retry after the runner catches up.')
    rules.require(rules.advance(p, state, {'kind': 'dm signal', 'detail': signal}, day), 'Direction did not match.')
    enter(conn, run, p, who, state, cursor, 'DM signal: ' + signal)
    conn.execute('UPDATE dm.story_signals SET used=true WHERE ticket_hash=%s', (ticket_hash(data['ticket']),))
    return {'run': run, 'phase': state['phase']}


# ----------------------------------------------------------------------------------------------- the Campaigns console
# Doc 85, section 5 and Phase 1 ("See it"): reads over the tables the runner already keeps (dm.story_releases,
# dm.story_transitions, dm.story_trigger_history, dm.actions), with names for every id. Nothing here writes.

LIVE_STATUSES = ('approved', 'running', 'paused', 'held', 'stopping')
# The first values of doc 85, 5.6; each world's owner may change them (dm.story_alert_rules, migration 0072: Phase 3), and a
# program may set its own quiet hours (`quietAfterHours`, 0 for "slow on purpose").
ALERTS = {'gameSeconds': 60, 'behindEvents': 2000, 'behindSeconds': 60, 'quietHours': 24, 'runnerSeconds': 30,
          'overdueSeconds': 300, 'goneDays': 14, 'fullPercent': 90}
ALERT_BOUNDS = {'gameSeconds': (5, 86400), 'behindEvents': (10, 10**7), 'behindSeconds': (5, 86400), 'quietHours': (0, 8760),
                'runnerSeconds': (5, 3600), 'overdueSeconds': (10, 86400), 'goneDays': (1, 365), 'fullPercent': (50, 100)}
AUDIENCE_CAP = 500
RECEIPT_KEYS = ('missionReceipt', 'triggerReceipt', 'moveReceipt')   # (moveReceipt: Phase 5b's move.)
STEERED = ('running', 'paused', 'held')   # (The statuses a command may steer.)
TIMELINE_PAGE = 60


def _iso(moment):
    return moment.isoformat() if moment is not None else None


def names(conn, world, ids):
    """Names for the ids a console shows (residents, wolves, places, factions): never a bare id where a name exists."""
    ids = sorted({i for i in ids if isinstance(i, str) and i and i != '$actor'})
    if not ids:
        return {}
    rows = conn.execute(
        """SELECT id,name FROM dm.story_residents WHERE world_id=%s AND id=ANY(%s)
           UNION ALL SELECT key,name FROM game.characters WHERE world_id=%s AND key=ANY(%s) AND name IS NOT NULL
           UNION ALL SELECT id,name FROM world.cells WHERE world_id=%s AND id=ANY(%s)
           UNION ALL SELECT id,name FROM world.interiors WHERE world_id=%s AND id=ANY(%s)
           UNION ALL SELECT id,name FROM live.factions WHERE world_id=%s AND id=ANY(%s)""",
        (world, ids) * 5,
    )
    return {i: n for i, n in rows if n}


def _now(conn, world):
    """The world's clock and log as the console sees them: game day (None before the first save), newest event, now."""
    row = conn.execute(
        """SELECT (SELECT (payload::jsonb->>'calendarDays')::float8 FROM game.checkpoints WHERE world_id=%s),
                  (SELECT id FROM game.events WHERE world_id=%s ORDER BY id DESC LIMIT 1), statement_timestamp()""",
        (world, world),
    ).fetchone()
    return {'day': row[0], 'newest': row[1] or 0, 'at': row[2]}


def _effects_due(program, state):
    """A visit's orders in the order the runner sends them: the phase order (already sent), then entry effects."""
    phase = rules.phase_of(program, state)
    effects = [*state.get('arrivalEffects', []), *entry_effects(phase, state)]
    if effects and not (phase['exits'] or phase.get('triggers')):
        effects.append({'kind': 'story.finish'})
    return phase, effects


def _orders(conn, ids):
    if not ids:
        return {}
    rows = conn.execute(
        """SELECT id,kind,target_id,status,result,requested_by,requested_at,done_at,
                  extract(epoch from statement_timestamp()-requested_at)::float8 FROM dm.actions WHERE id=ANY(%s)""",
        (list(ids),),
    )
    return {
        r[0]: {'id': r[0], 'kind': r[1], 'target': r[2], 'status': r[3], 'result': r[4], 'by': r[5],
               'at': _iso(r[6]), 'doneAt': _iso(r[7]), 'age': round(r[8], 1)}
        for r in rows
    }


def _listening(phase):
    """What the phase can still react to: its exits and triggers, as plain facts for the console to word."""
    out = []
    for index, edge in enumerate(phase['exits']):
        out.append({'source': 'exit', 'index': index, 'on': edge['on'], 'to': edge['to'],
                    **{k: edge[k] for k in ('match', 'afterDays', 'missions', 'players', 'percent', 'signal') if k in edge}})
    for t in phase.get('triggers', []):
        out.append({'source': 'trigger', 'id': t['id'], 'on': t['on'],
                    'scope': 'campaign' if t['id'] in phase.get('_sharedIds', ()) else 'phase',
                    'once': t.get('once', 'each'), 'audience': t.get('audience', 'world'),
                    **{k: t[k] for k in ('match', 'afterDays', 'signal') if k in t}})
    return out


def _waiting(program, state, status, problem, receipts, orders):
    """What a run is waiting for, most pressing first (doc 85, 5.2): the admin, the game, the clock, the wolves.
    Raw facts; Editor/src/lib/campaigns.ts words them."""
    if status == 'approved':
        return [{'on': 'publish'}]
    if status == 'stopping' and state:
        stop = orders.get(state.get('stopReceipt'))
        return [{'on': 'game', 'order': stop, 'stopping': True}] if stop else []
    if status not in STEERED or not state:
        return []
    out = []
    pending = [state[k] for k in RECEIPT_KEYS if k in state]
    if status == 'held':
        out.append({'on': 'admin', 'held': state.get('held', {}), 'problem': ''})
    if status == 'paused':
        stuck = next((orders[i] for i in [*pending, *(receipts[-1:])] if i in orders and orders[i]['status'] != 'applied'), None)
        out.append({'on': 'admin', 'problem': problem, **({'order': stuck} if stuck else {})})
    for i in [*pending, *(receipts[-1:])]:
        if i in orders and orders[i]['status'] == 'queued':
            out.append({'on': 'game', 'order': orders[i]})
            break
    phase, effects = _effects_due(program, state)
    unsent = effects[max(0, len(receipts) - 1):] if receipts else effects
    unsent = [*unsent, *state.get('triggerEffects', [])]
    if unsent:
        out.append({'on': 'send', 'effects': unsent})
    timers = sorted(state.get('timers', {}).items(), key=lambda kv: (kv[1].get('clock', 'game'), kv[1]['at'], kv[0]))
    for key, timer in timers[:8]:
        out.append({'on': 'timer', 'name': key, 'signal': timer.get('signal', key), 'at': timer['at'],
                    'clock': timer.get('clock', 'game'), 'actor': timer.get('actor', ''),
                    'repeat': timer.get('interval', 0), 'scope': timer.get('scope', 'visit')})
    # (Triggers turned off or used up wait for nothing: doc 85 Phase 3, the same reading as "cannot move on".)
    spent = {i['id'] for i in listeners(program, state, per_wolf=False) if i['source'] == 'trigger' and (i.get('off') or i.get('used'))}
    listening = [e for e in _listening(phase) if e.get('id') not in spent or e['source'] != 'trigger']
    for edge in listening:
        if edge['on'] == 'clock':
            out.append({'on': 'clock', 'at': state['entered'] + edge.get('afterDays', 0), 'what': edge})
    for edge in listening:
        if edge['on'] == 'dm signal':
            out.append({'on': 'direction', 'signal': edge.get('match', {}).get('detail', ''), 'what': edge})
    wolves = [e for e in listening if e['on'] not in ('clock', 'dm signal', rules.triggers.SIGNAL)]
    if wolves:
        out.append({'on': 'wolves', 'events': sorted({e['on'] for e in wolves}), 'what': wolves[:16]})
    if not (phase['exits'] or phase.get('triggers')) and not unsent:
        out.append({'on': 'end'})
    return out


def _last_activity(conn, runs):
    """For each run: its newest firing and its newest phase visit (with the real time of its newest order)."""
    last = {r: {} for r in runs}
    if not runs:
        return last
    for run, id_, entry, at in conn.execute(
        """SELECT DISTINCT ON (run) run,id,entry,recorded_at FROM dm.story_trigger_history WHERE run=ANY(%s)
           ORDER BY run,id DESC""",
        (list(runs),),
    ):
        last[run]['trigger'] = {'type': 'trigger', 'id': id_, 'trigger': entry.get('trigger'), 'actor': entry.get('actor', ''),
                                'day': entry.get('day'), 'phase': entry.get('phase'), 'at': at}
    for run, visit, phase, day, cause, first, newest in conn.execute(
        """SELECT DISTINCT ON (t.run) t.run,t.visit,t.phase,t.game_day,t.cause,
                  (SELECT requested_at FROM dm.actions WHERE id=t.receipts[1]),
                  (SELECT requested_at FROM dm.actions WHERE id=(SELECT max(x) FROM unnest(t.receipts) x))
           FROM dm.story_transitions t WHERE t.run=ANY(%s) ORDER BY t.run,t.visit DESC""",
        (list(runs),),
    ):
        last[run]['phase'] = {'type': 'phase', 'visit': visit, 'phase': phase, 'day': day, 'cause': cause, 'at': first}
        last[run]['order'] = newest
    return last


def _summary_missions(phase, state):
    return [
        {k: m[k] for k in ('id', 'title', 'mode', 'completed', 'required', 'ready', 'optional')}
        for m in rules.mission_snapshot(phase, state)
    ] if phase.get('missions') else []


def _behind(conn, world, cursor, newest, now):
    if newest <= cursor:
        return {'events': 0, 'seconds': 0}
    first = conn.execute(
        'SELECT recorded_at FROM game.events WHERE world_id=%s AND id>%s ORDER BY id LIMIT 1', (world, cursor)
    ).fetchone()
    return {'events': newest - cursor, 'seconds': round((now - first[0]).total_seconds(), 1) if first else 0}


def _alerts(conn, world, status, problem, waiting, behind, last, cohort, now, runner=None, held=None, ctx=None):
    """Doc 85, 5.6: paused, held, the runner not running (Phase 2's heartbeat), the game not answering, falling behind,
    quiet; and Phase 3's: cannot move on, a timer overdue, the audience nearly full. `ctx` carries the world's rules,
    the program and state, the world's speed and day."""
    ctx = ctx or {}
    limits = ctx.get('rules') or ALERTS
    out = []
    if status == 'paused':
        out.append({'kind': 'paused', 'needsYou': True, 'problem': problem})
    if status == 'held':
        out.append({'kind': 'held', 'needsYou': False, 'by': (held or {}).get('by', ''), 'reason': (held or {}).get('reason', ''),
                    'events': behind['events']})
    if status not in (*STEERED, 'stopping'):
        return out
    down = runner is not None and status in ('running', 'stopping') and (runner.get('age') is None or runner['age'] > limits['runnerSeconds'])
    if down:
        out.append({'kind': 'runner', 'needsYou': True, 'age': runner.get('age'), 'beatAt': runner.get('beatAt')})
    game = next((w for w in waiting if w['on'] == 'game'), None)
    if game and game['order']['age'] > limits['gameSeconds']:
        out.append({'kind': 'game', 'needsYou': False, 'order': game['order']})
    elif status == 'running' and not down and (behind['events'] > limits['behindEvents'] or behind['seconds'] > limits['behindSeconds']):
        out.append({'kind': 'behind', 'needsYou': False, **behind, 'runner': runner is not None})
    program, state = ctx.get('program'), ctx.get('state')
    quiet_hours = limits['quietHours']
    if program and 'quietAfterHours' in program:
        quiet_hours = program['quietAfterHours']
    if status == 'running' and cohort and quiet_hours:
        moments = [m for m in (last.get('trigger', {}).get('at'), last.get('phase', {}).get('at'), last.get('order')) if m]
        latest = max(moments) if moments else None
        if latest and (now - latest).total_seconds() > quiet_hours * 3600:
            # Quiet only counts while its wolves were about: someone of the audience did something since.
            seen = conn.execute(
                """SELECT max(e.at) FROM unnest(%s::text[]) a(id) CROSS JOIN LATERAL
                   (SELECT recorded_at AS at FROM game.events WHERE world_id=%s AND actor<>'' AND actor=a.id
                    ORDER BY id DESC LIMIT 1) e""",
                (cohort, world),
            ).fetchone()[0]
            if seen and seen > latest:
                out.append({'kind': 'quiet', 'needsYou': False, 'hours': round((now - latest).total_seconds() / 3600, 1),
                            'since': _iso(latest), 'wolfSeen': _iso(seen)})
    if program and state and status == 'running':
        settled = not any(w['on'] in ('game', 'send', 'admin') for w in waiting)
        if settled:
            stall = _stall(conn, world, program, state, limits)
            if stall:
                out.append({'kind': 'stuck', 'needsYou': True, **stall})
        for key, timer in sorted(state.get('timers', {}).items()):
            late = _overdue_seconds(timer, ctx.get('day'), state, ctx.get('speed'))
            if late is not None and late > limits['overdueSeconds']:
                out.append({'kind': 'overdue', 'needsYou': False, 'timer': key, 'signal': timer.get('signal', key),
                            'at': timer['at'], 'clock': timer.get('clock', 'game'), 'seconds': round(late)})
                break
    if cohort and len(cohort) >= AUDIENCE_CAP * limits['fullPercent'] / 100:
        out.append({'kind': 'full', 'needsYou': False, 'wolves': len(cohort), 'cap': AUDIENCE_CAP})
    return out


def _run_receipts(conn, run, receipts, state):
    """Every order a run has sent: each visit's receipts, the current visit's, and one waiting for its answer."""
    by_visit = {}
    for visit, ids in conn.execute('SELECT visit,receipts FROM dm.story_transitions WHERE run=%s', (run,)):
        for i in ids:
            by_visit.setdefault(i, visit)
    extra = [*(receipts or []), *((state[k] for k in (*RECEIPT_KEYS, 'stopReceipt') if k in state) if state else [])]
    for i in extra:
        by_visit.setdefault(i, state.get('visits') if state else None)
    return by_visit


def _row_view(conn, world, row, now, last, runner=None, ctx=None):
    run, title, st, digest, who, approved_at, state, problem, cursor, receipts, program = row
    view = {'run': run, 'title': title, 'status': st, 'digest': digest, 'approver': who, 'approvedAt': _iso(approved_at),
            'phase': state.get('phase') if state else None, 'problem': problem}
    if st == 'approved':
        view['waiting'] = [{'on': 'publish'}]
    if program is not None:
        view.update(storyId=program.get('id'), format=program.get('version'))
    if state and 'moveReceipt' in state:
        view['moving'] = True
    if not state or program is None:
        if st in ('complete', 'stopped') and state:
            view.update(visit=state.get('visits'), wolves=len(state.get('cohort', [])))
        return view
    phase = rules.phase_of(program, state)
    pending = [state[k] for k in (*RECEIPT_KEYS, 'stopReceipt') if k in state]
    orders = _orders(conn, [*pending, *receipts[-1:]])
    waiting = _waiting(program, state, st, problem, receipts, orders)
    behind = _behind(conn, world, cursor, now['newest'], now['at']) if st in ('running', 'held') else {'events': 0, 'seconds': 0}
    moment = [m for m in (last.get('trigger'), last.get('phase')) if m and m.get('at')]
    view.update(
        visit=state['visits'], entered=state['entered'], cohort=state.get('cohort', []), wolves=len(state.get('cohort', [])),
        missions=_summary_missions(phase, state), waiting=waiting, behind=behind,
        # (A firing and the visit it opened share a moment: the visit is the newer.)
        last={**max(moment, key=lambda m: (m['at'], m['type'] == 'phase')), 'at': _iso(max(m['at'] for m in moment))}
        if moment else None,
        alerts=_alerts(conn, world, st, problem, waiting, behind, last, state.get('cohort', []), now['at'], runner, state.get('held'),
                       {**(ctx or {}), 'program': program, 'state': state, 'day': now['day']}),
        ending=not (phase['exits'] or phase.get('triggers')),
    )
    return view


def overview(conn, world):
    """The Campaigns list (doc 85, 5.2): every run of this world (100 newest), and for the live ones where each is,
    who is in it, what happened last, what it is waiting for and its alerts."""
    now = _now(conn, world)
    rows = conn.execute(
        """SELECT id,program->>'title',status,digest,approved_by,approved_at,
                  CASE WHEN status=ANY(%s) THEN state END,problem,cursor,receipts,
                  CASE WHEN status=ANY(%s) THEN program END
           FROM dm.story_releases WHERE world_id=%s ORDER BY approved_at DESC LIMIT 100""",
        (list(LIVE_STATUSES), list(LIVE_STATUSES), world),
    ).fetchall()
    # (An ended run's state is read only for its counts: its audience can be 500 ids.)
    ended = {r: (v, n) for r, v, n in conn.execute(
        """SELECT id,(state->>'visits')::int,jsonb_array_length(coalesce(state->'cohort','[]'::jsonb)) FROM dm.story_releases
           WHERE world_id=%s AND NOT status=ANY(%s) AND state IS NOT NULL ORDER BY approved_at DESC LIMIT 100""",
        (world, list(LIVE_STATUSES)),
    )}
    last = _last_activity(conn, [r[0] for r in rows if r[2] in LIVE_STATUSES])
    beat = runner_beat(conn, world)
    queued = _queued_counts(conn, [r[0] for r in rows if r[2] in LIVE_STATUSES])
    limits = alert_rules(conn, world)
    ctx = {'rules': limits, 'speed': world_speed(conn, world)}
    runs = []
    for r in rows:
        try:
            view = _row_view(conn, world, r, now, last.get(r[0], {}), beat, ctx)
        except (ValueError, KeyError, TypeError, IndexError, StopIteration, AttributeError) as error:
            # (A damaged release must not hide the others, nor stop the heartbeat that reads this list.)
            view = {'run': r[0], 'title': r[1], 'status': r[2], 'digest': r[3], 'approver': r[4], 'approvedAt': _iso(r[5]),
                    'phase': (r[6] or {}).get('phase') if isinstance(r[6], dict) else None,
                    'problem': r[7] or f'Cannot read this story: {type(error).__name__} {error}'[:240], 'alerts': [], 'waiting': []}
        if queued.get(r[0]):
            view['commandsQueued'] = queued[r[0]]
        if r[0] in ended:
            view.update(visit=ended[r[0]][0], wolves=ended[r[0]][1])
        runs.append(view)
    ids = {i for v in runs for i in [*v.get('cohort', []), (v.get('last') or {}).get('actor', '')]}
    return {'runs': runs, 'now': {'day': now['day'], 'newest': now['newest'], 'at': _iso(now['at'])},
            'names': names(conn, world, ids), 'thresholds': limits, 'speed': ctx['speed'], 'runner': beat}


def _program_ids(program):
    """The ids a program names (residents, wolves, places, factions), for the console's names."""
    ids = set(program['audience'].get('players', []))
    for phase in rules.phases_of(program):
        for e in rules.possible_effects(phase):
            ids.update(v for k, v in e.items() if k in ('npc', 'player', 'resident', 'faction', 'other', 'cell', 'place', 'town'))
        ids.update(phase.get('guestVoices', []))
        for t in [*phase['exits'], *phase.get('triggers', []), *(o for m in phase.get('missions', []) for o in m['objectives'])]:
            ids.update(v for k, v in t.get('match', {}).items() if k in ('actor', 'target', 'cell'))
    for r in program.get('regions', []):
        ids.add(r['cell'])
    for scene in program.get('scenes', []):
        ids.update(role['npc'] for role in scene.get('cast', []) if 'npc' in role)
    return ids


def directed(program):
    """The residents a program directs or names (effects, guest voices, scene casts): marked on the console's map."""
    out = set()
    for phase in rules.phases_of(program):
        for e in rules.possible_effects(phase):
            if 'npc' in e:
                out.add(e['npc'])
        out.update(phase.get('guestVoices', []))
    for scene in program.get('scenes', []):
        out.update(role['npc'] for role in scene.get('cast', []) if 'npc' in role)
    return sorted(out)


def status(conn, world, run=None, before_visit=None, before_trigger=None, since=None, have=None):
    """One run's state, history (100 visits a page), trigger firings (50 a page) and orders; with no run, the list.
    Grown for the Campaigns console (doc 85, 5.9): the program (unless `have` is its digest), names for every id,
    the current missions, what it waits for, its alerts, and `since` ({visit, trigger}) so a poll gets only what is
    new."""
    if not run:
        return overview(conn, world)
    row = conn.execute(
        """SELECT id,program->>'title',status,digest,approved_by,approved_at,state,problem,cursor,receipts,program
           FROM dm.story_releases WHERE world_id=%s AND id=%s""",
        (world, run),
    ).fetchone()
    rules.require(row, 'Unknown run.')
    st, state, problem, ids, program = row[2], row[6], row[7], row[9], row[10]
    mission_program = any(p.get('missions') for p in rules.phases_of(program))
    if state:
        ids = [*ids, *(state[k] for k in RECEIPT_KEYS if k in state)]
    projection = ',missions' if mission_program else ''
    since = since if isinstance(since, dict) else {}
    for boundary in (before_visit, before_trigger, since.get('visit'), since.get('trigger')):
        rules.require(boundary is None or rules.integer(boundary, 0, 2**63 - 1), 'Invalid history cursor.')
    clauses, args = [], [run]
    if before_visit is not None:
        clauses.append(' AND visit < %s')
        args.append(before_visit)
    if since.get('visit') is not None:
        clauses.append(' AND visit > %s')
        args.append(since['visit'])
    history = conn.execute(
        f'SELECT visit,phase,game_day,cause,receipts{projection} FROM dm.story_transitions WHERE run=%s{"".join(clauses)} '
        'ORDER BY visit DESC LIMIT 101',
        args,
    ).fetchall()
    trace = []
    if any(ph.get('triggers') for ph in rules.phases_of(program)):
        clauses, args = [], [run]
        if before_trigger is not None:
            clauses.append(' AND id < %s')
            args.append(before_trigger)
        if since.get('trigger') is not None:
            clauses.append(' AND id > %s')
            args.append(since['trigger'])
        trace = conn.execute(
            f'SELECT id,visit,entry FROM dm.story_trigger_history WHERE run=%s{"".join(clauses)} ORDER BY id DESC LIMIT 51',
            args,
        ).fetchall()
    now = _now(conn, world)
    beat = runner_beat(conn, world)
    ctx = {'rules': alert_rules(conn, world), 'speed': world_speed(conn, world)}
    view = _row_view(conn, world, row, now, _last_activity(conn, [run]).get(run, {}) if st in LIVE_STATUSES else {}, beat, ctx)
    receipt_visits = _run_receipts(conn, run, row[9], state)
    orders = _orders(conn, sorted(receipt_visits, reverse=True)[:60])
    trace_rows = [{'id': r[0], 'visit': r[1], **r[2]} for r in trace[:50]]
    who = {*(state.get('cohort', []) if state else []), *_program_ids(program),
           *(t.get('actor', '') for t in trace_rows), *(o['target'] for o in orders.values())}
    result = {
        **view,
        'run': run,
        'status': st,
        'state': state,
        'problem': problem,
        'approvedBy': row[4],
        'cursor': row[8],
        'now': {'day': now['day'], 'newest': now['newest'], 'at': _iso(now['at'])},
        'guestVoices': rules.phase_of(program, state).get('guestVoices', []) if state and st == 'running' else [],
        'directions': [
            dict(zip(('npc', 'visit', 'receipt', 'status'), r))
            for r in conn.execute(
                """SELECT s.signal,s.visit,s.receipt,a.status FROM dm.story_signals s JOIN dm.actions a ON a.id=s.receipt
                   WHERE s.run=%s ORDER BY a.id DESC LIMIT 30""",
                (run,),
            )
        ],
        'signals': [e['match']['detail'] for e in rules.phase_of(program, state)['exits'] if e['on'] == 'dm signal']
        if state and st == 'running'
        else [],
        'history': [dict(zip(('visit', 'phase', 'day', 'cause', 'receipts', 'missions'), r)) for r in history[:100]],
        'beforeVisit': history[99][0] if len(history) > 100 else None,
        'trace': trace_rows,
        'beforeTrigger': trace[49][0] if len(trace) > 50 else None,
        'actions': [
            dict(zip(('id', 'kind', 'status', 'result'), r))
            for r in conn.execute('SELECT id,kind,status,result FROM dm.actions WHERE id=ANY(%s) ORDER BY id', (ids,))
        ],
        'orders': [{**o, 'visit': receipt_visits.get(i)} for i, o in sorted(orders.items(), reverse=True)],
        'missionsNow': rules.mission_snapshot(rules.phase_of(program, state), state) if state and mission_program else [],
        'directed': directed(program),
        'regions': program.get('regions', []),
        'phases': [{'id': p['id'], 'to': sorted({e['to'] for e in p['exits']} | {
            a['phase'] for t in [*rules.effective_phase(program, p).get('triggers', [])] for _, a in rules.triggers.walk(t['actions'])
            if a['do'] == 'enter'}),
            'missions': len(p.get('missions', []))} for p in program['phases']],
        'start': program['start'],
        'listening': _listening(rules.phase_of(program, state)) if state else [],
        'names': names(conn, world, who),
        'runner': beat,
        'commands': commands_of(conn, run),
        'controls': controls(program, state) if state and st in STEERED else None,
        # (Doc 85, Phase 5b: the versions it has been on, and those approved for it and not moved onto yet.)
        'versions': versions_of(conn, run, program, state, st),
        'speed': ctx['speed'],
    }
    if have != row[3]:
        result['program'] = program
    return result


def timeline(conn, world, run, before=None, since=None, kinds=None, wolf=None, trigger=None, limit=TIMELINE_PAGE):
    """One list, newest first, merged from phase visits, triggers fired and orders with their receipts (doc 85, 5.3;
    admin commands join it in Phase 2). `before` pages back ({visit, trigger, order}); `since` ({visit, trigger,
    order, at}) returns only what is newer, and the orders answered since `at`."""
    row = conn.execute('SELECT program,state,receipts FROM dm.story_releases WHERE world_id=%s AND id=%s', (world, run)).fetchone()
    rules.require(row, 'Unknown run.')
    program, state, receipts = row
    before = before if isinstance(before, dict) else {}
    since = since if isinstance(since, dict) else {}
    for value in (*before.values(), *(v for k, v in since.items() if k != 'at')):
        rules.require(value is None or rules.integer(value, 0, 2**63 - 1), 'Invalid timeline cursor.')
    rules.require(rules.integer(limit, 1, 200), 'Ask for 1 to 200 timeline rows.')
    kinds = set(kinds or ('phase', 'trigger', 'order', 'command', 'version'))
    rules.require(kinds <= {'phase', 'trigger', 'order', 'command', 'version'}, 'Unknown timeline filter.')
    for value in (wolf, trigger):
        rules.require(value is None or (isinstance(value, str) and 0 < len(value) <= 120), 'Invalid timeline filter.')
    paging = not since
    take = limit + 1 if paging else 201
    mission_program = any(p.get('missions') for p in rules.phases_of(program))
    items = []
    if 'phase' in kinds:
        clauses, args = [], [run]
        for key, op, source in (('visit', '<', before), ('visit', '>', since)):
            if source.get(key) is not None:
                clauses.append(f' AND t.visit {op} %s')
                args.append(source[key])
        rows = conn.execute(
            f"""SELECT t.visit,t.phase,t.game_day,t.cause,t.receipts,{'t.missions' if mission_program else 'NULL'},
                       (SELECT requested_at FROM dm.actions WHERE id=t.receipts[1])
                FROM dm.story_transitions t WHERE t.run=%s{''.join(clauses)} ORDER BY t.visit DESC LIMIT {take}""",
            args,
        ).fetchall()
        items += [{'key': f'v:{r[0]}', 'type': 'phase', 'visit': r[0], 'phase': r[1], 'day': r[2], 'cause': r[3],
                   'receipts': r[4], 'missions': r[5], 'at': r[6], 'seq': r[0]} for r in rows]
    if 'trigger' in kinds:
        clauses, args = [], [run]
        for key, op, source in (('trigger', '<', before), ('trigger', '>', since)):
            if source.get(key) is not None:
                clauses.append(f' AND id {op} %s')
                args.append(source[key])
        if wolf:
            clauses.append(" AND entry->>'actor'=%s")
            args.append(wolf)
        if trigger:
            clauses.append(" AND entry->>'trigger'=%s")
            args.append(trigger)
        rows = conn.execute(
            f"""SELECT id,visit,entry,recorded_at FROM dm.story_trigger_history WHERE run=%s{''.join(clauses)}
                ORDER BY id DESC LIMIT {take}""",
            args,
        ).fetchall()
        items += [{**r[2], 'key': f't:{r[0]}', 'type': 'trigger', 'id': r[0], 'visit': r[1], 'at': r[3], 'seq': r[0]}
                  for r in rows]
    receipt_visits = _run_receipts(conn, run, receipts, state)
    if 'order' in kinds and receipt_visits:
        clauses, args = [], [sorted(receipt_visits)]
        if before.get('order') is not None:
            clauses.append(' AND id < %s')
            args.append(before['order'])
        if since:
            # New orders, and older ones answered since the last poll (with a few seconds' overlap: the console
            # merges by key).
            clauses.append(" AND (id > %s OR status='queued' OR done_at >= %s::timestamptz - interval '5 seconds')")
            args += [since.get('order') or 0, since.get('at') or '-infinity']
        if wolf:
            clauses.append(' AND target_id=%s')
            args.append(wolf)
        rows = conn.execute(
            f"""SELECT id,kind,target_id,status,result,requested_by,requested_at,done_at FROM dm.actions
                WHERE id=ANY(%s){''.join(clauses)} ORDER BY id DESC LIMIT {take}""",
            args,
        ).fetchall()
        items += [{'key': f'o:{r[0]}', 'type': 'order', 'id': r[0], 'kind': r[1], 'target': r[2], 'status': r[3],
                   'result': r[4], 'by': r[5], 'at': r[6], 'doneAt': _iso(r[7]), 'visit': receipt_visits.get(r[0]),
                   'seq': r[0]} for r in rows]
    if 'command' in kinds and _steering(conn):
        # The admin's commands (doc 85, 5.3's fourth source): when taken (or asked, while queued); a poll gets new ones,
        # those still queued, and those answered since `at`.
        clauses, args = [], [run]
        if before.get('command') is not None:
            clauses.append(' AND id < %s')
            args.append(before['command'])
        if since:
            clauses.append(" AND (id > %s OR status='queued' OR done_at >= %s::timestamptz - interval '5 seconds')")
            args += [since.get('command') or 0, since.get('at') or '-infinity']
        if wolf:
            clauses.append(" AND (payload->>'actor'=%s OR payload->>'player'=%s)")
            args += [wolf, wolf]
        if trigger:
            clauses.append(" AND payload->>'trigger'=%s")
            args.append(trigger)
        rows = conn.execute(
            f"""SELECT {COMMAND_COLUMNS} FROM dm.story_commands WHERE run=%s{''.join(clauses)} ORDER BY id DESC LIMIT {take}""",
            args,
        ).fetchall()
        for r in rows:
            view = _command_view(r)
            items.append({**view, 'key': f'c:{r[0]}', 'type': 'command', 'command': view.pop('kind'),
                          'at': r[11] or r[8], 'askedAt': view.pop('at'), 'seq': r[0]})
    if 'version' in kinds and not wolf and not trigger and _versions_table(conn):
        # Moves onto a new version (doc 85, Phase 5b: "moved to version b7e1 by martin"), when taken; a poll gets new ones,
        # one still waiting for the game, and those the game answered since `at`.
        clauses, args = [], [run]
        if before.get('version') is not None:
            clauses.append(' AND id < %s')
            args.append(before['version'])
        if since:
            clauses.append(" AND (id > %s OR status='moving' OR settled_at >= %s::timestamptz - interval '5 seconds')")
            args += [since.get('version') or 0, since.get('at') or '-infinity']
        rows = conn.execute(
            f"""SELECT {VERSION_COLUMNS} FROM dm.story_versions WHERE run=%s AND from_digest IS NOT NULL{''.join(clauses)}
                ORDER BY id DESC LIMIT {take}""", args).fetchall()
        for r in rows:
            view = _version_view(r)
            report = view.pop('report') or {}
            items.append({**view, 'key': f'm:{r[0]}', 'type': 'version', 'at': r[6], 'seq': r[0],
                          'counts': {k: len(report.get(k, [])) for k in ('kept', 'reset', 'defaulted', 'dropped', 'added')},
                          'phase': (report.get('phase') or {}).get('to'), 'phaseFrom': (report.get('phase') or {}).get('from'),
                          'warnings': report.get('warnings', [])})
    # Newest first. One tick's work shares a moment: a firing (still in the visit it fired in) comes before the visit it
    # opened, and in one visit the visit opened, then its firings, then its orders, then the admin's commands (and a move
    # onto a new version after the command that made it).
    rank = {'phase': 0, 'trigger': 1, 'order': 2, 'command': 3, 'version': 4}
    epoch = datetime.datetime.min.replace(tzinfo=datetime.timezone.utc)
    items.sort(key=lambda i: (i['at'] or epoch, i.get('visit') or 0, rank[i['type']], i['seq']), reverse=True)
    more = False
    if paging and len(items) > limit:
        more, items = True, items[:limit]
    cursor = dict(before)
    for name, key in (('phase', 'visit'), ('trigger', 'trigger'), ('order', 'order'), ('command', 'command'), ('version', 'version')):
        seqs = [i['seq'] for i in items if i['type'] == name]
        if seqs:
            cursor[key] = min(seqs)
    events = {}
    event_ids = [i['event'] for i in items if i['type'] == 'trigger' and isinstance(i.get('event'), int)]
    if event_ids:
        for r in conn.execute(
            """SELECT id,kind,actor,target,cell,item,quantity,coins,detail,game_day,x,y,recorded_at FROM game.events
               WHERE world_id=%s AND id=ANY(%s)""",
            (world, event_ids),
        ):
            events[str(r[0])] = {**dict(zip(EVENT_COLUMNS[:2], r[:2])), 'actor': r[2], 'target': r[3], 'cell': r[4], 'item': r[5],
                                 'quantity': r[6], 'coins': r[7], 'detail': r[8], 'day': r[9], 'x': r[10], 'y': r[11],
                                 'at': _iso(r[12])}
    now = _now(conn, world)
    newest = {k: max([i['seq'] for i in items if i['type'] == name] + [since.get(k) or 0])
              for name, k in (('phase', 'visit'), ('trigger', 'trigger'), ('order', 'order'), ('command', 'command'), ('version', 'version'))}
    for i in items:
        i['at'] = _iso(i['at'])
        del i['seq']
    who = {*(i.get('actor', '') for i in items), *(i.get('target', '') for i in items),
           *((i['payload'] or {}).get(k, '') for i in items if i['type'] == 'command' for k in ('actor', 'player')),
           *(e[k] for e in events.values() for k in ('actor', 'target', 'cell')), *(i.get('cell', '') for i in items)}
    return {'run': run, 'items': items, 'events': events, 'names': names(conn, world, who),
            'before': cursor if more else None, 'since': {**newest, 'at': _iso(now['at'])}, 'now': _iso(now['at'])}


# ------------------------------------------------------------------------------------------- steering: the command inbox
# Doc 85, 5.4 and Phase 2 ("Steer it"). Every intervention is a row in dm.story_commands (migration 0071) that the runner
# takes first in each tick, in order, in the same transaction as the trigger history: one writer for the state, and a
# command lands between two world events. `command()` (Studio's request) checks and queues; `take_commands()` (the
# runner) checks again against the state as it then is, applies, and records the result with the values before and
# after, in the command's row and in dm.audit. A command can release only what was approved (5.1, rule 4): it fires a
# trigger the program has, enters a phase it has, sets a counter it names; nothing adds a trigger or changes a rule.

RUNNER_VERSION = 'doc85-phase5b'
COMMAND_KINDS = ('hold', 'resume', 'stop', 'signal', 'fire', 'enable', 'disable', 'enter', 'set', 'timer', 'mission',
                 'order', 'tell', 'scene', 'migrate')
# Flow commands move the story (dispatch a fact, run actions, open a visit, send the journal): they wait while the run is
# still sending a visit's orders, and are refused on a paused or held run that is part-way through sending.
FLOW = ('signal', 'fire', 'enter', 'mission')
# Taken even while a flow command ahead of them waits for the game: they are how an admin gets out of that wait.
URGENT = ('hold', 'resume', 'stop', 'order')
# The visit a command was made for still has to be the run's (5.8, rule 3), unless forced. Hold, resume and stop are
# about the run, not a visit.
VISITED = tuple(k for k in COMMAND_KINDS if k not in ('hold', 'resume', 'stop'))
FLOW_WAIT_SECONDS = 60
COMMAND_KEY = __import__('re').compile(r'[A-Za-z0-9_-]{8,64}\Z')
WOLF_EFFECTS = frozenset({'character.injury', 'story.area.damage'})   # (studio.py's STORY_WOLF_KINDS: "may act on wolves".)
RELEASING = ('fire', 'enter', 'signal', 'timer', 'mission')          # (Controls that can deliver the program's effects.)


def heartbeat(conn, world):
    """The runner is alive (doc 85, 5.6): written every tick, with how far the furthest-behind running story is."""
    import os

    behind = conn.execute(
        """SELECT coalesce(max((SELECT coalesce(max(id),0) FROM game.events WHERE world_id=%s) - cursor),0)
           FROM dm.story_releases WHERE world_id=%s AND status='running'""",
        (world, world),
    ).fetchone()[0]
    conn.execute(
        """INSERT INTO dm.story_runner(world_id,beat_at,started_at,pid,version,behind)
           VALUES (%s,statement_timestamp(),statement_timestamp(),%s,%s,%s)
           ON CONFLICT (world_id) DO UPDATE SET beat_at=excluded.beat_at,pid=excluded.pid,version=excluded.version,
               behind=excluded.behind,
               started_at=CASE WHEN dm.story_runner.pid=excluded.pid THEN dm.story_runner.started_at ELSE excluded.started_at END""",
        (world, os.getpid(), RUNNER_VERSION, max(0, behind)),
    )
    _beat_alerts(conn, world)


def runner_beat(conn, world):
    """The heartbeat as the console shows it, or None on a world without migration 0071."""
    if not conn.execute("SELECT to_regclass('dm.story_runner') IS NOT NULL").fetchone()[0]:
        return None
    row = conn.execute(
        """SELECT beat_at,started_at,pid,version,behind,extract(epoch from statement_timestamp()-beat_at)::float8
           FROM dm.story_runner WHERE world_id=%s""",
        (world,),
    ).fetchone()
    if not row:
        return {'age': None, 'beatAt': None}
    return {'beatAt': _iso(row[0]), 'startedAt': _iso(row[1]), 'pid': row[2], 'version': row[3], 'behind': row[4],
            'age': round(max(0.0, row[5]), 1)}


def _steering(conn):
    return conn.execute("SELECT to_regclass('dm.story_commands') IS NOT NULL").fetchone()[0]


def _queued_counts(conn, runs):
    if not runs or not _steering(conn):
        return {}
    return dict(conn.execute(
        "SELECT run,count(*) FROM dm.story_commands WHERE run=ANY(%s) AND status='queued' GROUP BY run", (list(runs),)))


COMMAND_COLUMNS = 'id,key,visit,digest,kind,payload,reason,requested_by,requested_at,status,result,done_at'


def _command_view(r):
    return {'id': r[0], 'key': r[1], 'visit': r[2], 'digest': r[3], 'kind': r[4], 'payload': r[5], 'reason': r[6],
            'by': r[7], 'at': _iso(r[8]), 'status': r[9], 'result': r[10], 'doneAt': _iso(r[11])}


def commands_of(conn, run, limit=60):
    """A run's commands, newest first (the console's Audit tab)."""
    if not _steering(conn):
        return []
    return [_command_view(r) for r in conn.execute(
        f'SELECT {COMMAND_COLUMNS} FROM dm.story_commands WHERE run=%s ORDER BY id DESC LIMIT %s', (run, limit))]


def variables(program):
    """What an admin may set (doc 85, 5.4), each {key, scope, type} (and its first value, label and note): a version 2
    story's declared variables; a version 1 story's counters as it reads or writes them (numbers)."""
    if program.get('version') == 2:
        return [{'key': d['id'], 'scope': d['scope'], 'type': d['type'], 'initial': rules.triggers.initial(d),
                 **{k: d[k] for k in ('show', 'note', 'clock') if k in d}} for d in program.get('variables', [])]
    return [{'key': k, 'scope': sc, 'type': 'number', 'initial': 0} for k, sc in _counters_named(program)]


def _counters_named(program):
    """Every counter the program reads or writes, with its scope (v1 counters are not declared: these are the ones an
    admin may set)."""
    if program.get('version') == 2:
        return sorted((d['id'], d['scope']) for d in program.get('variables', []))
    out = set()

    def walk(c):
        if c.get('test') in ('all', 'any'):
            for child in c['conditions']:
                walk(child)
        elif c.get('test') == 'not':
            walk(c['condition'])
        elif c.get('test') == 'counter':
            out.add((c['key'], c['scope']))

    for phase in rules.phases_of(program):
        for t in phase.get('triggers', []):
            if 'if' in t:
                walk(t['if'])
            out.update((a['key'], a['scope']) for a in t['actions'] if a['do'] in ('set', 'add'))
    return sorted(out)


def _schedules(program):
    """The timers the program can start, by signal: the first schedule action's settings."""
    out = {}
    for phase in rules.phases_of(program):
        for t in phase.get('triggers', []):
            for _, a in rules.triggers.walk(t['actions']):
                if a['do'] == 'schedule' and a['signal'] not in out:
                    out[a['signal']] = {k: a[k] for k in ('days', 'repeat', 'scope', 'clock', 'owner') if k in a}
    return out


def _signals_heard(phase):
    """The signal names a phase listens for: story-signal triggers and objectives, and its DM-direction exits."""
    heard = {t['signal'] for t in phase.get('triggers', []) if t['on'] == rules.triggers.SIGNAL}
    heard |= {o['signal'] for m in phase.get('missions', []) for o in m['objectives'] if 'signal' in o}
    heard |= {e['match']['detail'] for e in phase['exits'] if e['on'] == 'dm signal'}
    return heard


def controls(program, state):
    """What the console's controls may name for this run as it stands (bounded by the program: 5.8, rule 4)."""
    phase = rules.phase_of(program, state)
    disabled = set(state.get('disabled', ()))
    triggers = []
    for t in phase.get('triggers', []):
        key = rules.triggers.trigger_key(phase, t['id'])
        triggers.append({'id': t['id'], 'on': t['on'], 'signal': t.get('signal'), 'key': key, 'off': key in disabled,
                         'scope': 'campaign' if t['id'] in phase.get('_sharedIds', ()) else 'phase', 'once': t.get('once', 'each'),
                         'needsWolf': t.get('audience') == 'participant' or t.get('once') in ('player', 'member'),
                         'hasConditions': 'if' in t,
                         'actions': [a['effect']['kind'] if a['do'] == 'effect' else a['do'] for a in t['actions']]})
    off_elsewhere = sorted(k for k in disabled if k not in {t['key'] for t in triggers})
    return {
        'triggers': triggers, 'offElsewhere': off_elsewhere,
        'phases': [p['id'] for p in program['phases']],
        'signals': sorted(_signals_heard(phase)),
        'counters': variables(program),
        'schedules': _schedules(program),
        'missions': [{'id': m['id'], 'title': m['title'], 'mode': m['mode'], 'objectives': len(m['objectives'])}
                     for m in phase.get('missions', [])],
        'scenes': [sc['id'] for sc in program.get('scenes', [])],
        'regions': [r['id'] for r in program.get('regions', [])],
    }


# ----- values before and after (the timeline's "watches 5 -> 6")

def _values(state, actor):
    """The values a firing can change, as they stand: story (and world) values, the acting wolf's and their party's,
    timers' due times."""
    counters = state.get('counters', {})
    party = rules.triggers.party_of(state, actor) if actor else None
    return json.loads(json.dumps({
        'story': counters.get('story', {}),
        'world': counters.get('world', {}),
        'player': counters.get('player', {}).get(actor, {}) if actor else {},
        'party': counters.get('party', {}).get(party, {}) if actor else {},
        'timers': {k: t['at'] for k, t in state.get('timers', {}).items()},
        'actor': actor,
    }))


def _diff(before, after, decls=None):
    out = []
    for scope in ('story', 'world', 'player', 'party'):
        for key in sorted(set(before.get(scope, {})) | set(after.get(scope, {}))):
            first = rules.triggers.initial(decls[key]) if decls and key in decls else 0
            a, b = before.get(scope, {}).get(key, first), after.get(scope, {}).get(key, first)
            if a != b:
                out.append({'what': 'counter', 'scope': scope, 'key': key, 'before': a, 'after': b,
                            **({'wolf': after['actor']} if scope in ('player', 'party') else {}),
                            **({'type': decls[key]['type']} if decls and key in decls else {})})
    for key in sorted(set(before['timers']) | set(after['timers'])):
        a, b = before['timers'].get(key), after['timers'].get(key)
        if a != b:
            out.append({'what': 'timer', 'key': key, 'before': a, 'after': b})
    return out


def _attach_changes(phase, trace, before, after):
    """Each firing's history entry gets the values its own actions touched, before and after the fact they answered
    (several firings of one fact touching one counter each show the fact's whole change)."""
    decls = phase.get('_vars')
    changes = _diff(before, after, decls)
    if not changes:
        return
    by_id = {t['id']: t for t in phase.get('triggers', [])}
    for entry in trace:
        t = by_id.get(entry.get('trigger'))
        if not t:
            continue
        actions = [a for _, a in rules.triggers.walk(t['actions'])]
        counters = {(a['scope'], a['key']) for a in actions if a['do'] in ('set', 'add') and 'key' in a}
        counters |= {(decls[a['var']]['scope'], a['var']) for a in actions if decls and a.get('var') in decls}
        timers = {a['signal'] for a in actions if a['do'] in ('schedule', 'cancel', 'shift')}
        mine = [c for c in changes if (c['what'] == 'counter' and (c['scope'], c['key']) in counters)
                or (c['what'] == 'timer' and c['key'].split('/')[0] in timers)]
        if mine:
            entry['changes'] = mine


# ----- the request

def _reason(reason):
    rules.require(isinstance(reason, str) and 6 <= len(reason.strip()) <= 300 and len(reason.split()) >= 2,
                  'Say why, in a few words (5.8, rule 1).')
    return ' '.join(reason.split())


def _days(v, low=-36500, high=36500):
    return isinstance(v, (int, float)) and not isinstance(v, bool) and low <= v <= high and v == v


def _check_payload(program, kind, payload):
    """The checks a command passes whatever the run's state (made again when it is taken, against the state then)."""
    rules.require(isinstance(payload, dict) and len(json.dumps(payload)) <= 4000, 'A small command, please.')
    known = {'force', 'catchUp', 'name', 'actor', 'trigger', 'check', 'phase', 'effects', 'key', 'scope', 'player',
             'value', 'do', 'days', 'signal', 'mission', 'receipt', 'to', 'region', 'text', 'scene', 'cue', 'version'}
    rules.require(set(payload) <= known, 'Unknown field in the command: ' + ', '.join(sorted(set(payload) - known)))
    phases = rules.phases_of(program)
    if kind == 'signal':
        rules.require(rules.identifier(payload.get('name')) and any(payload['name'] in _signals_heard(ph) for ph in phases),
                      'Nothing in this story listens for that signal.')
    elif kind in ('fire', 'enable', 'disable'):
        rules.require(rules.identifier(payload.get('trigger')) and any(
            t['id'] == payload['trigger'] for ph in phases for t in ph.get('triggers', [])), 'This story has no such trigger.')
    elif kind == 'enter':
        rules.require(any(p['id'] == payload.get('phase') for p in program['phases']), 'This story has no such phase.')
    elif kind == 'set':
        rules.require((payload.get('key'), payload.get('scope')) in _counters_named(program),
                      'This story names no such counter (a counter must be one its triggers read or write).'
                      if program.get('version') != 2 else 'This story declares no such variable.')
        decl = rules.variables_of(program).get(payload['key']) if program.get('version') == 2 else {'type': 'number'}
        rules.require(decl['type'] != 'timer', 'A timer variable is started and cleared by the story, not set.')
        if decl['type'] == 'number' and program.get('version') != 2:
            rules.require(rules.integer(payload.get('value'), -rules.triggers.LIMIT, rules.triggers.LIMIT), 'Choose a whole number.')
        else:
            rules.require(rules._initial_ok(decl['type'], payload.get('value'), set(rules.region_index(program))),
                          f'Choose a value of its type ({decl["type"]}).')
    elif kind == 'timer':
        rules.require(payload.get('do') in ('fire', 'cancel', 'shift', 'start'), 'Fire, cancel, shift or start a timer.')
        if payload['do'] == 'start':
            rules.require(payload.get('signal') in _schedules(program), 'This story never starts a timer with that signal.')
        else:
            rules.require(isinstance(payload.get('name'), str) and 0 < len(payload['name']) <= 200, 'Name the timer.')
        if payload['do'] == 'shift' or 'days' in payload:
            rules.require(_days(payload.get('days')) and payload['days'] != 0, 'Choose a number of days.')
    elif kind == 'mission':
        rules.require(payload.get('do') in ('complete', 'reset'), 'Complete or reset a mission (today\'s missions: doc 85, 5.4).')
        rules.require(any(m['id'] == payload.get('mission') for ph in phases for m in ph.get('missions', [])), 'This story has no such mission.')
    elif kind == 'order':
        rules.require(payload.get('do') in ('retry', 'skip') and rules.integer(payload.get('receipt'), 1, 2**63 - 1),
                      'Retry or skip an order by its number.')
    elif kind == 'tell':
        text = payload.get('text')
        rules.require(rules.word(text, 300) and '\n' not in text and '\r' not in text, 'One line of up to 300 characters.')
        rules.require(payload.get('to') in ('participants', 'wolf', 'region'), 'Tell the participants, a wolf or a region.')
        if payload['to'] == 'region':
            rules.require(payload.get('region') in rules.region_index(program), 'This story has no such region.')
    elif kind == 'migrate':
        rules.require(rules.integer(payload.get('version'), 1, 2**63 - 1), 'Name the approved version to move onto.')
        rules.require('phase' not in payload or rules.identifier(payload['phase']), 'Name the phase to land in.')
    elif kind == 'scene':
        rules.require(payload.get('scene') in {sc['id'] for sc in program.get('scenes', [])}, 'This story has no such scene.')
        rules.require(isinstance(payload.get('player'), str) and payload['player'], 'Choose a wolf, or the world.')
        rules.require('cue' not in payload or rules.identifier(payload['cue']), 'A cue is a short name.')
    for field in ('actor', 'player'):
        rules.require(field not in payload or (isinstance(payload[field], str) and len(payload[field]) <= 120), 'Invalid wolf.')


def _may_release(program, kind, payload, may):
    """5.7: a control that could deliver an effect the admin at the console could not have approved is refused for them."""
    if kind not in RELEASING or (kind == 'timer' and payload.get('do') != 'fire') or not may:
        return
    kinds = {e['kind'] for ph in rules.phases_of(program) for e in rules.possible_effects(ph)}
    rules.require(may.get('relations', True) or 'faction.relation' not in kinds,
                  'This story changes faction relations: only a builder or the owner may move it on (5.7).')
    rules.require(may.get('wolves', True) or not (kinds & WOLF_EFFECTS),
                  'This story can hurt players\' wolves: moving it on needs "may act on players\' wolves" (5.7).')


def command(conn, world, who, data, may=None):
    """Queue one command (op `command`): {run, kind, payload, reason, key, visit, digest}. Sending the same key twice
    queues it once (the reply is the first one's). The runner takes it within a tick."""
    rules.require(_steering(conn), 'This world needs migration 0071 to steer its campaigns.')
    kind, payload, key = data.get('kind'), data.get('payload') or {}, data.get('key')
    rules.require(kind in COMMAND_KINDS, 'Unknown command.')
    rules.require(isinstance(key, str) and COMMAND_KEY.match(key), 'A command needs its own id (8-64 letters, digits, - or _).')
    reason = _reason(data.get('reason'))
    run = data.get('run')
    rules.require(isinstance(run, str) and 0 < len(run) <= 80, 'Unknown run.')
    row = conn.execute(
        'SELECT program,state,status,digest FROM dm.story_releases WHERE world_id=%s AND id=%s', (world, run)).fetchone()
    rules.require(row, 'Unknown run.')
    program, state, status, digest = row
    existing = conn.execute(f'SELECT {COMMAND_COLUMNS} FROM dm.story_commands WHERE run=%s AND key=%s', (run, key)).fetchone()
    if existing:
        return {**_command_view(existing), 'again': True}
    rules.require(status in STEERED, f'This run is {status}: there is nothing to steer.')
    rules.require(data.get('digest') == digest, 'This command was made against another version of the story. Look again.')
    visit = data.get('visit')
    rules.require(rules.integer(visit, 1, 2**31 - 1), 'A command names the visit it was made for.')
    _check_payload(program, kind, payload)
    _may_release(program, kind, payload, may)
    inserted = conn.execute(
        f"""INSERT INTO dm.story_commands(run,key,visit,digest,kind,payload,reason,requested_by)
            VALUES (%s,%s,%s,%s,%s,%s::jsonb,%s,%s) ON CONFLICT (run,key) DO NOTHING RETURNING {COMMAND_COLUMNS}""",
        (run, key, visit, digest, kind, json.dumps(payload), reason, str(who)[:80]),
    ).fetchone()
    if not inserted:
        inserted = conn.execute(f'SELECT {COMMAND_COLUMNS} FROM dm.story_commands WHERE run=%s AND key=%s', (run, key)).fetchone()
        return {**_command_view(inserted), 'again': True}
    conn.execute("SELECT pg_notify('ratw_story',%s)", (json.dumps({'run': run, 'command': inserted[0]}),))
    return _command_view(inserted)


def hold_all(conn, world, who, data):
    """Hold every running story (doc 34's "pause all"): one command and one audit row each, one reason."""
    reason, key = _reason(data.get('reason')), data.get('key')
    rules.require(isinstance(key, str) and COMMAND_KEY.match(key), 'A command needs its own id.')
    out = []
    for run, visit, digest in conn.execute(
            "SELECT id,(state->>'visits')::int,digest FROM dm.story_releases WHERE world_id=%s AND status='running' ORDER BY id",
            (world,)).fetchall():
        out.append(command(conn, world, who, {'run': run, 'kind': 'hold', 'payload': {}, 'reason': reason, 'key': key,
                                              'visit': visit, 'digest': digest}))
    return {'commands': out}


# ----- the runner's side

class Refused(Exception):
    """A command the runner will not carry out as the run now stands (recorded, never retried)."""


class Wait(Exception):
    """A flow command that must wait for the run to finish sending its orders."""


def _require(ok, why):
    if not ok:
        raise Refused(why)


def _order_status(conn, receipt):
    row = conn.execute('SELECT status,result FROM dm.actions WHERE id=%s', (receipt,)).fetchone()
    return row if row else (None, None)


def _settled(conn, program, state, receipts):
    """Whether the run has sent and had answered every order of its visit (a flow command may move it then)."""
    if any(k in state for k in RECEIPT_KEYS) or state.get('triggerEffects'):
        return False
    _, effects = _effects_due(program, state)
    if not receipts or len(receipts) <= len(effects):
        return False
    return receipts[-1] in state.get('skipped', ()) or _order_status(conn, receipts[-1])[0] == 'applied'


class Steer:
    """One command, taken: the run as it stands (locked), and the work of each kind. Every method returns the
    result's text and its values before and after, or raises Refused or Wait."""

    def __init__(self, conn, world, run, clock, cmd, lock=True):
        self.conn, self.world, self.run, self.clock = conn, world, run, clock
        self.id, self.key, self.visit, self.digest, self.kind, self.payload, self.reason, self.by, self.age = cmd
        row = conn.execute(
            """SELECT program,state,status,cursor,approved_by,receipts,digest,problem,program->>'title' FROM dm.story_releases
               WHERE world_id=%s AND id=%s""" + (' FOR UPDATE' if lock else ''), (world, run)).fetchone()
        _require(row, 'Unknown run.')
        self.program, self.state, self.status, self.cursor, self.approver, self.receipts, self.run_digest, self.problem, \
            self.title = row
        self.trace = []
        self.moved = None
        self.report = None
        self.world_loaded = world_load(conn, world, self.program, self.state) if self.state else None

    # --- common
    def cohort(self, wolf, required=True):
        if not wolf and not required:
            return ''
        _require(isinstance(wolf, str) and wolf in self.state['cohort'], 'That wolf is not in this story.')
        return wolf

    def save(self, status=None):
        world_save(self.conn, self.world, self.run, self.program, self.state, self.world_loaded)
        self.conn.execute(
            'UPDATE dm.story_releases SET state=%s::jsonb,cursor=%s,status=%s,problem=%s WHERE id=%s',
            (json.dumps(self.state), self.cursor, status or self.status, self.problem, self.run))
        if status:
            self.status = status

    def elapsed(self):
        published = self.state.get('publishedAt', 0)
        return max(0, (time.time() - published) / 86400) if published else 0

    def day(self):
        return max(self.clock(), self.state['entered'])

    def phase(self):
        return rules.phase_of(self.program, self.state)

    def history(self):
        if self.trace:
            for entry in self.trace:
                entry.update(command=self.id, by=self.by)
            self.conn.execute(
                """INSERT INTO dm.story_trigger_history(run,visit,entry)
                   SELECT %s,%s,entry FROM jsonb_array_elements(%s::jsonb) WITH ORDINALITY AS t(entry,n) ORDER BY n""",
                (self.run, self.visit_before, json.dumps(self.trace)))

    def dispatch(self, fact):
        """A fact the command makes, through the rules as a world event's would go: True when it moved the phase."""
        phase = self.phase()
        before = _values(self.state, fact.get('actor', ''))
        start = len(self.trace)
        moved = rules.dispatch(self.program, phase, self.state, fact, self.day(), self.trace, self.elapsed())
        _attach_changes(phase, self.trace[start:], before, _values(self.state, fact.get('actor', '')))
        return moved

    def open_visit(self, cause, quiet=False):
        held = self.status == 'held'
        enter(self.conn, self.run, self.program, self.approver, self.state, self.cursor, cause, quiet)
        self.receipts = self.conn.execute('SELECT receipts FROM dm.story_releases WHERE id=%s', (self.run,)).fetchone()[0]
        self.status = 'running'
        if held:   # (A held run stays held: its new visit's orders go when it is resumed.)
            self.save('held')

    def flow_ready(self):
        if _settled(self.conn, self.program, self.state, self.receipts):
            return
        _require(self.status == 'running', 'It is part-way through sending its orders to the game and is '
                 + ('held: resume it first.' if self.status == 'held' else 'paused: retry or skip the stuck order first.'))
        _require(self.age < FLOW_WAIT_SECONDS, f'The game did not answer the story\'s orders within {FLOW_WAIT_SECONDS} s.')
        raise Wait()

    def note_order(self, receipt):
        """An order a command sent beside the story's own (a message, a scene): the visit's history keeps it."""
        self.conn.execute('UPDATE dm.story_transitions SET receipts=array_append(receipts,%s::bigint) WHERE run=%s AND visit=%s',
                          (receipt, self.run, self.state['visits']))

    # --- the kinds
    def do_hold(self):
        if self.status == 'held':
            return 'It was already held.', {}, {}
        _require(self.status == 'running', 'It is paused on a refused order: retry or skip it (holding adds nothing).')
        self.state['held'] = {'by': self.by, 'reason': self.reason, 'cursor': self.cursor, 'at': time.time(), 'command': self.id}
        self.save('held')
        return 'Held: it reads no events, sends no orders and fires no timers until resumed.', {'status': 'running'}, {'status': 'held'}

    def do_resume(self):
        _require(self.status == 'held', 'Only a held story is resumed (a paused one is retried or skipped).')
        newest = self.conn.execute('SELECT coalesce(max(id),0) FROM game.events WHERE world_id=%s', (self.world,)).fetchone()[0]
        waiting = max(0, newest - self.cursor)
        self.state.pop('held', None)
        catch_up = self.payload.get('catchUp', True) is not False
        if not catch_up:
            self.cursor = max(self.cursor, newest)
        self.save('running')
        text = (f'Resumed: the {waiting} world events of the hold count.' if catch_up
                else f'Resumed from now: the {waiting} world events of the hold were skipped.')
        return text, {'status': 'held', 'cursor': self.cursor if catch_up else self.cursor - waiting}, {
            'status': 'running', 'cursor': self.cursor, 'skipped': 0 if catch_up else waiting}

    def do_stop(self):
        receipt = queue(self.conn, self.by, 'story.stop', self.run, {})
        self.state['stopReceipt'] = receipt
        self.note_order(receipt)
        before = self.status
        self.save('stopping')
        return (f'Stopping: order #{receipt} tells the game; it is stopped when the game acknowledges. What it gave the '
                'world stays.'), {'status': before}, {'status': 'stopping', 'order': receipt}

    def do_signal(self):
        self.flow_ready()
        name, actor = self.payload['name'], self.cohort(self.payload.get('actor', ''), required=False)
        phase = self.phase()
        _require(name in _signals_heard(phase), f'Nothing in phase {phase["id"]} listens for "{name}" now.')
        before_phase = self.state['phase']
        moved = self.dispatch({'kind': rules.triggers.SIGNAL, 'signal': name, 'actor': actor, 'id': None})
        if not moved and any(e['on'] == 'dm signal' and e['match']['detail'] == name for e in phase['exits']):
            moved = self.dispatch({'kind': 'dm signal', 'detail': name, 'actor': actor, 'id': None})
        self.moved = moved
        fired = [t['trigger'] for t in self.trace]
        text = f'Signal "{name}" sent' + (f' as {actor}' if actor else '') + (
            f': {", ".join(fired)} fired' if fired else ': nothing fired') + (f'; it moved to {self.state["phase"]}.' if moved else '.')
        return text, {'phase': before_phase}, {'phase': self.state['phase'], 'fired': fired}

    def do_fire(self):
        self.flow_ready()
        phase = self.phase()
        t = next((t for t in phase.get('triggers', []) if t['id'] == self.payload['trigger']), None)
        _require(t, f'Trigger {self.payload["trigger"]} is not in phase {phase["id"]}.')
        check = self.payload.get('check', True) is not False
        repeat = t.get('once', 'each')
        personal = t.get('audience') == 'participant' or repeat in ('player', 'member')
        actor = self.cohort(self.payload.get('actor', ''), required=personal)
        key = rules.triggers.trigger_key(phase, t['id'])
        _require(not (check and key in self.state.get('disabled', ())), 'It is turned off (fire it without its checks, or turn it on).')
        stand_in = {'kind': t['on'], 'signal': t.get('signal'), 'actor': actor, 'id': None, **t.get('match', {})}
        owner = actor if repeat in ('player', 'member') else '*'
        bucket = 'campaignFired' if repeat in ('campaign', 'member') else 'fired'
        token = t['id'] if bucket == 'fired' or t['id'] in phase.get('_sharedIds', ()) else phase['id'] + '/' + t['id']
        already = repeat != 'each' and owner in self.state.get(bucket, {}).get(token, [])
        _require(not (check and already), f'It has already fired ({repeat}){" for " + actor if actor else ""}.')
        ready = lambda: {m['id'] for m in rules.mission_snapshot(phase, self.state) if m['ready']}
        if check and 'if' in t:
            verdict = rules.triggers.satisfied(t['if'], self.state, stand_in, ready, phase.get('_regions'), phase, self.day(), self.elapsed())
            _require(verdict is True, 'Its conditions are ' + ('unknown (they need a place)' if verdict is None else 'false') + ' just now.')
        if repeat != 'each' and not already:
            self.state.setdefault(bucket, {}).setdefault(token, []).append(owner)
        before = _values(self.state, actor)
        self.trace.append({'phase': phase['id'], 'day': self.day(), 'actor': actor, 'elapsedDays': self.elapsed(),
                           'actions': [a['effect']['kind'] if a['do'] == 'effect' else a['do'] for a in t['actions']],
                           'trigger': t['id'], 'scope': 'campaign' if t['id'] in phase.get('_sharedIds', ()) else 'phase',
                           'event': None, 'on': t['on'], 'signal': t.get('signal'), 'byHand': True, 'checked': check})
        emitted = []
        if rules.triggers.version(phase) == 2:
            rules.triggers.count_firing(phase, self.state, t['id'])
        target = rules.triggers.perform(t, self.state, stand_in, actor, self.day(), self.elapsed(),
                                        lambda name, source: emitted.append(name), phase, self.trace[-1], ready, self.trace)
        _attach_changes(phase, self.trace[-1:], before, _values(self.state, actor))
        moved = False
        if target:
            moved = self._transition(target, actor)
        for name in emitted:
            if moved:
                break
            moved = self.dispatch({**stand_in, 'kind': rules.triggers.SIGNAL, 'signal': name})
        self.moved = moved
        effects = [e['kind'] for e in self.state.get('triggerEffects', [])]
        return (f'{t["id"]} fired by hand{" for " + actor if actor else ""}' + (', its conditions checked' if check else ', unchecked')
                + (f'; it moved to {self.state["phase"]}.' if moved else '.')), {'phase': phase['id']}, {
            'phase': self.state['phase'], 'effects': effects, 'changes': self.trace[0].get('changes', [])}

    def _transition(self, target, actor):
        """As rules.dispatch moves a run: the closing visit's missions kept, the visit's timers and once-marks dropped."""
        _require(any(p['id'] == target for p in self.program['phases']), 'No such phase.')
        rules.leave_visit(self.phase(), self.state, target, self.day(), actor)
        return True

    def do_enter(self):
        self.flow_ready()
        target, effects = self.payload['phase'], self.payload.get('effects', True) is not False
        order = [p['id'] for p in self.program['phases']]
        here = self.state['phase']
        way = 'again' if target == here else 'back' if order.index(target) < order.index(here) else 'on'
        self._transition(target, '')
        self.moved = True
        text = (f'Entered {target} ({"re-opened" if way == "again" else "went back" if way == "back" else "moved on"}), '
                f'{"with" if effects else "without"} its entry effects.' + (' Nothing the world was given is taken back.' if way != 'on' else ''))
        return text, {'phase': here, 'visit': self.state['visits'] - 1}, {'phase': target, 'visit': self.state['visits'], 'effects': effects, 'way': way}

    def do_enable(self, on=True):
        name = self.payload['trigger']
        phase_id = self.payload.get('phase') or self.state['phase']
        _require(any(p['id'] == phase_id for p in self.program['phases']), 'No such phase.')
        phase = rules.effective_phase(self.program, next(p for p in self.program['phases'] if p['id'] == phase_id))
        _require(any(t['id'] == name for t in phase.get('triggers', [])), f'Phase {phase_id} has no trigger {name}.')
        key = rules.triggers.trigger_key(phase, name)
        disabled = set(self.state.get('disabled', ()))
        was = key not in disabled
        (disabled.discard if on else disabled.add)(key)
        self.state['disabled'] = sorted(disabled)
        if not self.state['disabled']:
            self.state.pop('disabled')
        self.save()
        word = 'on' if on else 'off'
        return (f'{key} turned {word}.' if was != on else f'{key} was already {word}.'), {'on': was}, {'on': on}

    def do_disable(self):
        return self.do_enable(False)

    def do_set(self):
        key, scope, value = self.payload['key'], self.payload['scope'], self.payload['value']
        wolf = self.cohort(self.payload.get('player', ''), required=scope in ('player', 'party'))
        if self.program.get('version') == 2:
            store = rules.triggers.var_store(self.state, scope, wolf, True)
            was = store.get(key, rules.triggers.initial(rules.variables_of(self.program)[key]))
        else:
            store = rules.triggers.counter_store(self.state, scope, wolf, True)
            was = store.get(key, 0)
        store[key] = value
        self.save()
        who = f' for {wolf}' if wolf else ''
        return f'{key}{who} set {was} → {value}.', {'value': was}, {'value': value, 'key': key, 'scope': scope, **({'wolf': wolf} if wolf else {})}

    def do_timer(self):
        do, timers = self.payload['do'], self.state.setdefault('timers', {})
        if do == 'start':
            signal = self.payload['signal']
            settings = _schedules(self.program)[signal]
            actor = self.cohort(self.payload.get('actor', ''), required=settings.get('owner') == 'player')
            key = signal + '/' + actor if settings.get('owner') == 'player' else signal
            days = self.payload.get('days', settings['days'])
            _require(_days(days, 1 / 1440, 36500), 'A timer starts at least a minute from now.')
            real = settings.get('clock') == 'real'
            was = timers.get(key, {}).get('at')
            timers[key] = {'signal': signal, 'at': (self.elapsed() if real else self.day()) + days, 'clock': 'real' if real else 'game',
                           'actor': actor, 'interval': days if settings.get('repeat') else 0, 'scope': settings.get('scope', 'visit')}
            self.save()
            return f'Timer {key} started: due in {days:g} days.', {'at': was}, {'at': timers[key]['at']}
        name = self.payload['name']
        _require(name in timers, 'No such timer is running.')
        timer = timers[name]
        was = timer['at']
        if do == 'cancel':
            del timers[name]
            self.save()
            return f'Timer {name} cancelled.', {'at': was}, {'at': None}
        if do == 'shift':
            timer['at'] = max(0, was + self.payload['days'])
            self.save()
            return f'Timer {name} moved {self.payload["days"]:+g} days.', {'at': was}, {'at': timer['at']}
        self.flow_ready()   # fire
        now = self.elapsed() if timer.get('clock') == 'real' else self.day()
        if timer.get('interval'):
            timer['at'] = now + timer['interval']
        else:
            del timers[name]
        moved = self.dispatch({'kind': rules.triggers.SIGNAL, 'signal': timer.get('signal', name), 'actor': timer.get('actor', ''), 'id': None})
        self.moved = moved
        fired = [t['trigger'] for t in self.trace]
        return (f'Timer {name} fired now' + (f': {", ".join(fired)} fired' if fired else ': nothing fired')
                + (f'; it moved to {self.state["phase"]}.' if moved else '.')), {'at': was}, {
            'at': timers.get(name, {}).get('at'), 'fired': fired, 'phase': self.state['phase']}

    def do_mission(self):
        self.flow_ready()
        phase = self.phase()
        m = next((m for m in phase.get('missions', []) if m['id'] == self.payload['mission']), None)
        _require(m, f'Mission {self.payload["mission"]} is not in phase {phase["id"]}.')
        individual = m['mode'] == 'individual'
        wolf = self.cohort(self.payload.get('player', ''), required=individual)
        key = wolf if individual else 'group'
        ledger = self.state.setdefault('missions', {})
        record = ledger.setdefault(m['id'], {'tracks': {}, 'contributors': []})
        was = list(record['tracks'].get(key, [0, 0]))
        if self.payload['do'] == 'complete':
            record['tracks'][key] = [len(m['objectives']), 0]
            if not individual and wolf and wolf not in record['contributors']:
                record['contributors'].append(wolf)
        else:
            record['tracks'].pop(key, None)
            record.get('bindings', {}).pop(key, None)
            if not individual:
                record['contributors'] = []
        snapshot = rules.mission_snapshot(phase, self.state)
        receipt = queue(self.conn, self.by, 'story.missions', self.run, {'visit': self.state['visits'], 'missions': snapshot})
        self.state['missionReceipt'] = receipt
        self.conn.execute(
            'UPDATE dm.story_transitions SET missions=%s::jsonb,receipts=array_append(receipts,%s::bigint) WHERE run=%s AND visit=%s',
            (json.dumps(snapshot), receipt, self.run, self.state['visits']))
        self.save()
        done = self.payload['do'] == 'complete'
        return (f'{m["title"]}{" for " + wolf if wolf else ""} {"completed" if done else "reset"} by hand; the journal is told '
                f'(order #{receipt}).' + (' It counts when a wolf of the story next does something.' if done else '')), {
            'step': was[0], 'of': len(m['objectives'])}, {'step': record['tracks'].get(key, [0, 0])[0], 'of': len(m['objectives'])}

    def do_order(self):
        receipt, do = self.payload['receipt'], self.payload['do']
        _require(self.status == 'paused', 'Only a paused story has a stuck order.')
        pending_key = next((k for k in RECEIPT_KEYS if k in self.state), None)
        stuck = self.state[pending_key] if pending_key else (self.receipts[-1] if self.receipts else None)
        _require(receipt == stuck, f'Order #{receipt} is not the one this story is stuck on (#{stuck}).')
        status, result = _order_status(self.conn, receipt)
        if status == 'applied':
            self.problem = ''
            self.save('running')
            return f'Order #{receipt} was applied after all: it carries on.', {'status': 'paused'}, {'status': 'running'}
        _require(status in ('refused', 'expired'), f'Order #{receipt} is {status}: wait for the game.')
        if do == 'skip':
            self.state.setdefault('skipped', []).append(receipt)
            if pending_key:
                del self.state[pending_key]
            self.problem = ''
            self.save('running')
            return f'Order #{receipt} skipped ({status}: {result}); the story carries on without it.', {
                'status': 'paused', 'order': status}, {'status': 'running', 'order': 'skipped'}
        kind, target, payload = self.conn.execute('SELECT kind,target_id,payload FROM dm.actions WHERE id=%s', (receipt,)).fetchone()
        again = queue(self.conn, self.by, kind, target, payload)
        if pending_key:
            self.state[pending_key] = again
        else:
            self.receipts[-1] = again
            self.conn.execute('UPDATE dm.story_releases SET receipts=%s WHERE id=%s', (self.receipts, self.run))
        self.note_order(again)
        self.problem = ''
        self.save('running')
        return f'Order #{receipt} sent again as #{again}.', {'status': 'paused', 'order': receipt}, {'status': 'running', 'order': again}

    def do_tell(self):
        to, text = self.payload['to'], self.payload['text']
        payload = {'visit': self.state['visits'], 'text': text, 'to': to, 'title': self.title}
        if to == 'wolf':
            payload['player'] = self.cohort(self.payload.get('player', ''))
        if to == 'region':
            payload['region'] = rules.region_index(self.program)[self.payload['region']]
        receipt = queue(self.conn, self.by, 'story.tell', self.run, payload)
        self.note_order(receipt)
        whom = {'participants': 'the story\'s wolves', 'wolf': payload.get('player', ''), 'region': 'the wolves in ' + self.payload.get('region', '')}[to]
        return f'Told {whom}: "{text}" (order #{receipt}).', {}, {'order': receipt}

    def do_scene(self):
        scene = next(sc for sc in self.program['scenes'] if sc['id'] == self.payload['scene'])
        player = self.payload['player']
        if player != 'world':
            self.cohort(player)
        receipt = queue(self.conn, self.by, 'story.scene', self.run, {'scene': scene, 'cue': self.payload.get('cue', 'admin'),
                                                                     'player': player, 'visit': self.state['visits']})
        self.note_order(receipt)
        return f'Scene {scene["id"]} started for {player} (order #{receipt}).', {}, {'order': receipt}

    def do_migrate(self):
        """Move the run onto a newly approved version (doc 85, Phase 5b): the migration report worked out against the run
        as it stands, the release's program and digest changed, the state carried over, and the game told the new
        version's words and regions for this visit (`story.phase` with `revise`). Until the game answers, the run reads
        nothing; if it refuses, the run goes back (settle_move). Nothing is delivered by the move itself."""
        self.flow_ready()
        version = self.conn.execute(
            """SELECT id,digest,program,approved_by,approved_at,status FROM dm.story_versions WHERE id=%s AND run=%s FOR UPDATE""",
            (self.payload['version'], self.run)).fetchone()
        _require(version, 'This run has no such approved version.')
        vid, new_digest, new_program, approver, approved_at, vstatus = version
        _require(vstatus == 'approved', f'That version is {vstatus}, not waiting to be moved onto.')
        _require(new_digest != self.run_digest, 'The run is on that version already.')
        began = self.conn.execute('SELECT game_day FROM dm.story_transitions WHERE run=%s ORDER BY visit LIMIT 1', (self.run,)).fetchone()
        before_state = json.loads(json.dumps(self.state))
        state, report = rules.migration(self.program, new_program, self.state, self.payload.get('phase'), self.run,
                                        began[0] if began else None)
        self.report = report
        _require(state is not None, report['blocked'] or 'It cannot be moved as asked.')
        old_digest, old_program, old_phase = self.run_digest, self.program, self.state['phase']
        current = self.conn.execute("SELECT id FROM dm.story_versions WHERE run=%s AND status='current'", (self.run,)).fetchone()
        if current:
            self.conn.execute("UPDATE dm.story_versions SET status='past' WHERE id=%s", (current[0],))
        else:
            # (The version it was published on, recorded at its first move: one row per version a run has been on.)
            self.conn.execute(
                """INSERT INTO dm.story_versions(run,digest,program,approved_by,approved_at,status,moved_at,visit,game_day)
                   SELECT r.id,r.digest,r.program,r.approved_by,r.approved_at,'past',
                          (SELECT requested_at FROM dm.actions WHERE id=(SELECT receipts[1] FROM dm.story_transitions WHERE run=r.id AND visit=1)),
                          1,(SELECT game_day FROM dm.story_transitions WHERE run=r.id AND visit=1)
                   FROM dm.story_releases r WHERE r.id=%s""", (self.run,))
        phase = rules.phase_of(new_program, state)
        state['entrySent'] = True
        receipt = queue(self.conn, self.by, 'story.phase', self.run, {
            'revise': True, 'phase': state['phase'], 'journal': phase['journal'], 'title': new_program['title'],
            'players': state['cohort'], 'visit': state['visits'], 'end': False, 'guestVoices': phase.get('guestVoices', []),
            'missions': rules.mission_snapshot(phase, state), 'previousMissions': [], 'regions': new_program.get('regions', []),
            'lines': [e for e in phase['effects'] if e['kind'] == 'npc.line']})
        state['moveReceipt'] = receipt
        self.note_order(receipt)
        self.conn.execute(
            """UPDATE dm.story_versions SET status='moving',from_digest=%s,moved_at=statement_timestamp(),moved_by=%s,command=%s,
                      visit=%s,at_event=%s,game_day=%s,report=%s::jsonb,state_before=%s::jsonb,receipt=%s WHERE id=%s""",
            (old_digest, self.by, self.id or None, state['visits'], self.cursor, self.day(), json.dumps(report), json.dumps(before_state),
             receipt, vid))
        self.conn.execute(
            'UPDATE dm.story_releases SET program=%s::jsonb,digest=%s,approved_by=%s WHERE id=%s',
            (json.dumps(new_program), new_digest, approver, self.run))
        # (Older approvals of this run waiting to be moved onto were made against the version it has left.)
        self.conn.execute("UPDATE dm.story_versions SET status='withdrawn',problem=%s WHERE run=%s AND status='approved'",
                          (f'The run moved onto {new_digest[:8]} first.', self.run))
        self.program, self.state, self.run_digest, self.approver = new_program, state, new_digest, approver
        self.world_loaded = None
        counts = {k: len(report[k]) for k in ('kept', 'reset', 'defaulted', 'dropped', 'added')}
        landed = state['phase'] + (f' (from {old_phase}, which it no longer has)' if state['phase'] != old_phase else '')
        text = (f'Moving onto version {new_digest[:8]} (from {old_digest[:8]}), in {landed}, visit {state["visits"]}: '
                f'{counts["kept"]} kept, {counts["reset"]} reset, {counts["defaulted"]} defaulted, {counts["dropped"]} dropped. '
                f'Order #{receipt} tells the game; if it refuses, the run goes back.')
        return text, {'version': old_digest, 'phase': old_phase, 'title': old_program['title']}, {
            'version': new_digest, 'phase': state['phase'], 'order': receipt, 'counts': counts, 'versionRow': vid,
            'warnings': report['warnings']}

    # --- the whole
    def take(self):
        _require(self.digest == self.run_digest, 'Made against another version of the story.')
        _require(self.status in (*STEERED,), f'The run is {self.status}.')
        _require(self.kind not in VISITED or self.visit == self.state['visits'] or self.payload.get('force') is True,
                 f'Made for visit {self.visit}; the story has moved on to visit {self.state["visits"]} ({self.state["phase"]}).')
        _require(self.kind in ('hold', 'resume', 'stop') or 'moveReceipt' not in self.state,
                 'It is moving onto a new version: wait for the game to take it (a few seconds).')
        _check_payload(self.program, self.kind, self.payload)
        self.visit_before, cursor = self.state['visits'], self.cursor
        text, before, after = getattr(self, 'do_' + self.kind)()
        self.history()
        if self.moved:
            self.open_visit(f'admin {self.by}: {self.reason}'[:200], quiet=self.kind == 'enter' and self.payload.get('effects', True) is False)
        else:
            self.save()
        return {'text': text, 'before': before, 'after': after, 'atEvent': cursor, 'visit': self.visit_before,
                **({'firings': len(self.trace)} if self.trace else {})}


def _audit(conn, cmd, title, outcome, result):
    detail = f'"{title}" {cmd[4]}: {result.get("text", "")} Reason: "{cmd[6]}"'
    if outcome == 'refused':
        detail = f'"{title}" {cmd[4]} refused: {result.get("text", "")} Reason: "{cmd[6]}"'
    conn.execute('INSERT INTO dm.audit (at, action, target, detail, who) VALUES (extract(epoch from now()), %s, %s, %s, %s)',
                 ('story.' + cmd[4], cmd[-1], detail[:2000], cmd[7]))


def take_commands(conn, world, run, clock, limit=20):
    """The run's queued commands, in order, at the top of its tick (doc 85, 5.1 rule 3). A flow command waits while the
    run still sends its orders; only hold, resume, stop and order commands go past it."""
    rows = conn.execute(
        """SELECT id,key,visit,digest,kind,payload,reason,requested_by,extract(epoch from statement_timestamp()-requested_at)::float8
           FROM dm.story_commands WHERE run=%s AND status='queued' ORDER BY id LIMIT %s FOR UPDATE SKIP LOCKED""",
        (run, limit)).fetchall()
    blocked = False
    title = None
    for cmd in rows:
        if blocked and cmd[4] not in URGENT:
            continue
        try:
            with conn.transaction():
                steer = Steer(conn, world, run, clock, cmd)
                title = steer.title
                result = steer.take()
            outcome = 'done'
        except Wait:
            blocked = True
            continue
        except (Refused, ValueError, KeyError, TypeError, IndexError, StopIteration) as why:
            outcome, result = 'refused', {'text': str(why) if isinstance(why, (Refused, ValueError)) else
                                          f'Could not carry it out: {type(why).__name__} {why}'[:300]}
        if title is None:
            title = conn.execute("SELECT program->>'title' FROM dm.story_releases WHERE id=%s", (run,)).fetchone()[0]
        conn.execute("UPDATE dm.story_commands SET status=%s,result=%s::jsonb,done_at=statement_timestamp() WHERE id=%s",
                     (outcome, json.dumps(result), cmd[0]))
        _audit(conn, (*cmd[:8], run), title, outcome, result)


def stopping(conn, run):
    """A stopping run is stopped once the game has acknowledged story.stop (known problem 10). Refused means the game
    has no such story: stopped too. Expired (the game was down): sent again."""
    row = conn.execute("SELECT state FROM dm.story_releases WHERE id=%s AND status='stopping' FOR UPDATE", (run,)).fetchone()
    if not row:
        return
    state = row[0]
    receipt = state.get('stopReceipt')
    status, result = _order_status(conn, receipt) if receipt else ('applied', '')
    if status in ('applied', 'refused'):
        conn.execute("UPDATE dm.story_releases SET status='stopped',problem=%s WHERE id=%s",
                     ('' if status == 'applied' else f'The game had no such story: {result}', run))
    elif status == 'expired':
        state['stopReceipt'] = queue(conn, 'story-runner', 'story.stop', run, {})
        conn.execute('UPDATE dm.story_releases SET state=%s::jsonb WHERE id=%s', (json.dumps(state), run))


# ------------------------------------------------------------------------------------------- moving onto a new version
# Doc 85, Phase 5b. A running campaign moved onto a newly approved version of its story keeps its wolves, its phase (or the
# one the admin names when the new version no longer has it), its variables, timers, waits and missions where they still
# fit (story_program.migration). Approving "as a new version of run X" is the ordinary rehearsal and approval, kept in
# dm.story_versions (migration 0074); the move is the inbox's `migrate` command, taken between two world events, with
# the release's program, digest and approver changed and the game told the new version's words and regions for the
# visit the run is in. If the game refuses that, the run goes back to the version it was on (settle_move).

VERSION_COLUMNS = ('id,digest,approved_by,approved_at,status,from_digest,moved_at,moved_by,command,visit,at_event,game_day,'
                   'report,receipt,settled_at,problem')


def _versions_table(conn):
    return conn.execute("SELECT to_regclass('dm.story_versions') IS NOT NULL").fetchone()[0]


def approve_version(conn, world, who, program, digest, run):
    """Approve a program as a new version of a live run (op `approve` with `forRun`): rehearsed and checked as any
    approval (approve() has done that), kept until the run is moved onto it. The reply carries the migration report
    against the run as it stands now."""
    rules.require(_versions_table(conn), 'This world needs migration 0074 to move a running story onto a new version.')
    rules.require(isinstance(run, str) and 0 < len(run) <= 80, 'Unknown run.')
    row = conn.execute('SELECT program,state,status,digest FROM dm.story_releases WHERE world_id=%s AND id=%s FOR UPDATE',
                       (world, run)).fetchone()
    rules.require(row, 'Unknown run.')
    old, state, status, current = row
    rules.require(status in STEERED and state, f'This run is {status}: only a live run is moved onto a new version.')
    rules.require(program['id'] == old['id'], f'A new version keeps its story\'s id ("{old["id"]}"): this is another story.')
    rules.require(digest != current, 'The run is on this version already.')
    same = conn.execute("SELECT id FROM dm.story_versions WHERE run=%s AND digest=%s AND status='approved'", (run, digest)).fetchone()
    if same:
        vid = same[0]
    else:
        conn.execute("UPDATE dm.story_versions SET status='withdrawn',problem=%s WHERE run=%s AND status='approved'",
                     (f'{str(who)[:80]} approved a newer version.', run))
        vid = conn.execute(
            """INSERT INTO dm.story_versions(run,digest,program,approved_by) VALUES (%s,%s,%s::jsonb,%s) RETURNING id""",
            (run, digest, json.dumps(program), str(who)[:80])).fetchone()[0]
    _, report = rules.migration(old, program, state, run=run)
    return {'run': run, 'version': vid, 'digest': digest, 'report': report}


def _version_view(r):
    out = dict(zip(VERSION_COLUMNS.split(','), r))
    for k in ('approved_at', 'moved_at', 'settled_at'):
        out[k] = _iso(out[k])
    return {'id': out['id'], 'digest': out['digest'], 'approvedBy': out['approved_by'], 'approvedAt': out['approved_at'],
            'status': out['status'], 'from': out['from_digest'], 'movedAt': out['moved_at'], 'movedBy': out['moved_by'],
            'command': out['command'], 'visit': out['visit'], 'atEvent': out['at_event'], 'day': out['game_day'],
            'report': out['report'], 'order': out['receipt'], 'settledAt': out['settled_at'], 'problem': out['problem']}


def versions_of(conn, run, program=None, state=None, status=None):
    """A run's versions, newest first (the console's "Move onto a new version…" and the timeline): for an approval not yet
    moved onto, the migration report against the run as it stands now."""
    if not _versions_table(conn):
        return []
    rows = conn.execute(f'''SELECT {VERSION_COLUMNS} FROM dm.story_versions WHERE run=%s
                           ORDER BY coalesce(moved_at,approved_at) DESC,id DESC LIMIT 40''', (run,)).fetchall()
    out = [_version_view(r) for r in rows]
    for v in out:
        if v['status'] == 'approved' and program and state and status in STEERED:
            new = conn.execute('SELECT program FROM dm.story_versions WHERE id=%s', (v['id'],)).fetchone()[0]
            try:
                _, v['report'] = rules.migration(program, new, state, run=run)
            except ValueError as why:
                v['report'] = {'blocked': str(why)}
            v['title'] = new['title']
    return out


def settle_move(conn, run):
    """A move waits for the game's answer to its order (story.phase with `revise`). Applied: the run is on its new version.
    Refused: the run goes back to the version it was on, with what it held before the move (a hold since stays). Expired
    (the game was down): sent again."""
    row = conn.execute("SELECT state,status,program,digest FROM dm.story_releases WHERE id=%s AND state ? 'moveReceipt' FOR UPDATE",
                       (run,)).fetchone()
    if not row:
        return
    state, status, program, digest = row
    receipt = state['moveReceipt']
    delivery, result = _order_status(conn, receipt)
    version = conn.execute(
        "SELECT id,from_digest,state_before FROM dm.story_versions WHERE run=%s AND status='moving' AND receipt=%s", (run, receipt)).fetchone()
    if delivery == 'queued' or delivery is None:
        return
    if delivery == 'expired':
        again = conn.execute('SELECT kind,target_id,payload,requested_by FROM dm.actions WHERE id=%s', (receipt,)).fetchone()
        state['moveReceipt'] = queue(conn, again[3], again[0], again[1], again[2])
        conn.execute('UPDATE dm.story_releases SET state=%s::jsonb WHERE id=%s', (json.dumps(state), run))
        conn.execute('UPDATE dm.story_transitions SET receipts=array_append(receipts,%s::bigint) WHERE run=%s AND visit=%s',
                     (state['moveReceipt'], run, state['visits']))
        if version:
            conn.execute('UPDATE dm.story_versions SET receipt=%s WHERE id=%s', (state['moveReceipt'], version[0]))
        return
    if delivery == 'applied' or not version:
        del state['moveReceipt']
        conn.execute('UPDATE dm.story_releases SET state=%s::jsonb WHERE id=%s', (json.dumps(state), run))
        if version:
            conn.execute("UPDATE dm.story_versions SET status='current',settled_at=statement_timestamp() WHERE id=%s", (version[0],))
        return
    # Refused: back to the version it was on.
    vid, from_digest, before = version
    back = conn.execute("""SELECT id,program,approved_by,approved_at FROM dm.story_versions WHERE run=%s AND digest=%s AND status='past'
                           ORDER BY id DESC LIMIT 1""", (run, from_digest)).fetchone()
    for key in ('held',):
        if key in state:
            before[key] = state[key]
        else:
            before.pop(key, None)
    conn.execute("UPDATE dm.story_versions SET status='refused',settled_at=statement_timestamp(),problem=%s WHERE id=%s",
                 (f'The game refused it: {result}'[:500], vid))
    if back:
        conn.execute("UPDATE dm.story_versions SET status='current' WHERE id=%s", (back[0],))
        conn.execute('UPDATE dm.story_releases SET program=%s::jsonb,digest=%s,approved_by=%s,state=%s::jsonb WHERE id=%s',
                     (json.dumps(back[1]), from_digest, back[2], json.dumps(before), run))
    else:
        # (No record of the version it came from: it stays on the new one, paused, for the admin to look at.)
        del state['moveReceipt']
        conn.execute("UPDATE dm.story_releases SET state=%s::jsonb,status='paused',problem=%s WHERE id=%s",
                     (json.dumps(state), f'The game refused version {digest[:8]} and the version before it is not recorded.', run))
    conn.execute('INSERT INTO dm.audit (at, action, target, detail, who) VALUES (extract(epoch from now()), %s, %s, %s, %s)',
                 ('story.migrate', run, f'"{program["title"]}" went back to version {from_digest[:8]}: the game refused version '
                  f'{digest[:8]} (order #{receipt}: {result}).'[:2000], 'story-runner'))


# ------------------------------------------------------------------------------------------- explaining: "Explain it"
# Doc 85, 5.5 and 5.6, Phase 3. Reads only. `explain` answers: what a run waits for (every trigger and exit of its phase,
# each condition true, false or unknown with the values behind it, and for which of its wolves it could fire); why one
# has not fired (its last events and where each stopped); what a made-up event would do; and what a console command would
# do (the command run by the runner's own code on the run as it stands, in a transaction that is rolled back). Nothing is
# saved. The stall alert ("cannot move on") and the overdue timer are worked out from the same reading.

EXPLAIN_EVENTS = 10


def alert_rules(conn, world):
    """The world's alert rules: doc 85, 5.6's first values, with what its owner changed (dm.story_alert_rules, 0072)."""
    limits = dict(ALERTS)
    if conn.execute("SELECT to_regclass('dm.story_alert_rules') IS NOT NULL").fetchone()[0]:
        row = conn.execute('SELECT rules FROM dm.story_alert_rules WHERE world_id=%s', (world,)).fetchone()
        for key, value in ((row[0] if row else None) or {}).items():
            if key in ALERT_BOUNDS and _days(value, *ALERT_BOUNDS[key]):
                limits[key] = value
    return limits


def set_alert_rules(conn, world, who, data):
    """The owner's change to the alert rules (5.7: "change the alert rules" is the owner's). Unknown keys and values out
    of bounds are refused; a key set to null goes back to the first value. Audited."""
    rules.require(conn.execute("SELECT to_regclass('dm.story_alert_rules') IS NOT NULL").fetchone()[0],
                  'This world needs migration 0072 to keep its own alert rules.')
    asked = data.get('rules')
    rules.require(isinstance(asked, dict) and asked, 'Send the rules to change.')
    rules.require(set(asked) <= set(ALERT_BOUNDS), 'Unknown alert rule: ' + ', '.join(sorted(set(asked) - set(ALERT_BOUNDS))))
    row = conn.execute('SELECT rules FROM dm.story_alert_rules WHERE world_id=%s FOR UPDATE', (world,)).fetchone()
    kept = dict((row[0] if row else None) or {})
    for key, value in asked.items():
        if value is None:
            kept.pop(key, None)
            continue
        low, high = ALERT_BOUNDS[key]
        rules.require(_days(value, low, high), f'{key}: {low} to {high}.')
        kept[key] = value
    conn.execute(
        """INSERT INTO dm.story_alert_rules(world_id,rules,updated_by) VALUES (%s,%s::jsonb,%s)
           ON CONFLICT (world_id) DO UPDATE SET rules=excluded.rules,updated_by=excluded.updated_by,updated_at=now()""",
        (world, json.dumps(kept), str(who)[:80]))
    conn.execute('INSERT INTO dm.audit (at, action, target, detail, who) VALUES (extract(epoch from now()), %s, %s, %s, %s)',
                 ('story.alert-rules', world, ('Alert rules: ' + json.dumps(kept, sort_keys=True))[:2000], str(who)[:80]))
    return {'rules': alert_rules(conn, world), 'changed': kept}


def world_speed(conn, world):
    """Game days per real second, from the world's log over about the last hour (None when it cannot tell): to say "in
    2.1 days (about 50 min)" of a game-clock timer."""
    newest = conn.execute(
        'SELECT id,game_day,extract(epoch from recorded_at)::float8 FROM game.events WHERE world_id=%s ORDER BY id DESC LIMIT 1',
        (world,)).fetchone()
    if not newest or newest[1] is None:
        return None
    older = conn.execute(
        """SELECT game_day,extract(epoch from recorded_at)::float8 FROM game.events WHERE world_id=%s AND id>%s
           AND recorded_at >= to_timestamp(%s) - interval '1 hour' AND game_day IS NOT NULL ORDER BY id LIMIT 1""",
        (world, max(0, newest[0] - 20000), newest[2])).fetchone()
    if not older or newest[2] - older[1] < 120 or newest[1] <= older[0]:
        return None
    return round((newest[1] - older[0]) / (newest[2] - older[1]), 9)


def _estimate(left_days, clock, speed):
    """Real seconds until a timer is due (game days through the world's speed; a real-clock one is in real days)."""
    if clock == 'real':
        return left_days * 86400
    return left_days / speed if speed else None


def _overdue_seconds(timer, day, state, speed):
    """How long ago (real seconds) a timer fell due and did not fire, or None."""
    if timer.get('clock') == 'real':
        published = state.get('publishedAt')
        if not published:
            return None
        late = (time.time() - published) / 86400 - timer['at']
        return late * 86400 if late > 0 else None
    if day is None or timer['at'] >= day or not speed:
        return None
    return (day - timer['at']) / speed


def _filter_miss(rule, fact):
    """The first filter of an exit, trigger or objective that a fact does not meet, as {field, expected, actual}; None when
    it meets them all (story_program.matches, word by word)."""
    kind = fact['kind']
    if rule['on'] == 'gift' and kind == 'gift again':
        kind = 'gift'
    if rule['on'] != kind:
        return {'field': 'kind', 'expected': rule['on'], 'actual': fact['kind']}
    if rule.get('signal') != fact.get('signal'):
        return {'field': 'signal', 'expected': rule.get('signal'), 'actual': fact.get('signal')}
    for key, expected in rule.get('match', {}).items():
        actual = fact.get(key, '') or ''
        if key == 'item' and rule['on'] in ('gift', 'gift again') and not any(mark in expected for mark in '~@'):
            actual = __import__('re').split('[~@]', actual, maxsplit=1)[0]
        if actual != expected:
            return {'field': key, 'expected': expected, 'actual': fact.get(key, '') or ''}
    for key, field in (('minCoins', 'coins'), ('minQuantity', 'quantity')):
        if key in rule and (fact.get(field) or 0) < rule[key]:
            return {'field': field, 'expected': '>= ' + str(rule[key]), 'actual': fact.get(field) or 0}
    return None


def _ready(phase, state):
    return lambda: {m['id'] for m in rules.mission_snapshot(phase, state) if m['ready']}


def _uses_player_counters(c):
    if c['test'] in ('all', 'any'):
        return any(_uses_player_counters(child) for child in c['conditions'])
    if c['test'] == 'not':
        return _uses_player_counters(c['condition'])
    return c['test'] == 'counter' and c['scope'] == 'player'


def _once_marks(phase, state, t):
    """Who a trigger's once-rule has already used up: '*' (the visit or the run), wolves, or nobody."""
    repeat = t.get('once', 'each')
    if repeat == 'each':
        return repeat, []
    bucket = 'campaignFired' if repeat in ('campaign', 'member') else 'fired'
    token = t['id'] if bucket == 'fired' or t['id'] in phase.get('_sharedIds', ()) else phase['id'] + '/' + t['id']
    return repeat, list(state.get(bucket, {}).get(token, []))


def _signal_senders(phase, state, name):
    """What can send a story signal in this phase now: its triggers that emit or schedule it, and timers running for it."""
    disabled = set(state.get('disabled', ()))
    senders = [{'trigger': t['id'], 'off': rules.triggers.trigger_key(phase, t['id']) in disabled}
               for t in phase.get('triggers', []) if any(a['do'] in ('emit', 'schedule') and a['signal'] == name
                                                         for _, a in rules.triggers.walk(t['actions']))]
    timers = [{'timer': k, 'signal': v.get('signal', k), 'at': v['at'], 'clock': v.get('clock', 'game')} for k, v in state.get('timers', {}).items()
              if v.get('signal', k) == name]
    return senders, timers


def _recent_wolves(conn, world, cohort, days):
    """The wolves of the story who did anything in the world in the last `days` real days."""
    if not cohort:
        return set()
    return {r[0] for r in conn.execute(
        """SELECT a.id FROM unnest(%s::text[]) a(id) WHERE EXISTS (SELECT 1 FROM game.events e WHERE e.world_id=%s AND e.actor=a.id
           AND e.recorded_at >= now() - %s * interval '1 day')""", (cohort, world, days))}


def listeners(program, state, per_wolf=True, recent=None, day=None):
    """Every trigger and exit of the run's phase, judged as the run stands (doc 85, 5.5's "what is this story waiting
    for?"): on or off, used up or not (and for whom), its conditions as a tree of true, false and unknown with values (on
    a stand-in event of its own filters: what the event brings is unknown until it comes), its due day, its senders (a
    signal), its gate (an exit), and `can`: whether it can still fire by itself (the stall alert's test). With `per_wolf`
    the wolves of the story it could fire for now. `recent`: the wolves seen lately (None: not asked)."""
    phase = rules.phase_of(program, state)
    cohort = state.get('cohort', [])
    disabled = set(state.get('disabled', ()))
    ready = _ready(phase, state)
    out = []
    for t in phase.get('triggers', []):
        key = rules.triggers.trigger_key(phase, t['id'])
        repeat, used_by = _once_marks(phase, state, t)
        personal = t.get('audience') == 'participant' or repeat in ('player', 'member')
        item = {'source': 'trigger', 'id': t['id'], 'key': key, 'on': t['on'], 'scope': 'campaign' if t['id'] in phase.get('_sharedIds', ()) else 'phase',
                'once': repeat, 'audience': t.get('audience', 'world'), 'off': key in disabled,
                'actions': [a['effect']['kind'] if a['do'] == 'effect' else a['do'] for a in t['actions']],
                'enters': next((a['phase'] for _, a in rules.triggers.walk(t['actions']) if a['do'] == 'enter'), None),
                **({'startsOff': True} if t.get('enabled') is False else {}),
                **{k: t[k] for k in ('match', 'signal', 'afterDays') if k in t}}
        if repeat in ('visit', 'campaign'):
            item['used'] = '*' in used_by
        elif repeat in ('player', 'member'):
            item['usedFor'] = used_by
            item['used'] = bool(cohort) and set(cohort) <= set(used_by)
        else:
            item['used'] = False
        stand_in = {'kind': t['on'], 'signal': t.get('signal'), 'actor': '', 'id': None, **t.get('match', {})}
        known = set(t.get('match', {}))
        if 'if' in t:
            item['conditions'] = rules.triggers.judge(t['if'], state, stand_in, ready, phase.get('_regions'), known, phase, day)
        if t['on'] == 'clock':
            item['due'] = state['entered'] + t['afterDays']
        if t['on'] == rules.triggers.SIGNAL:
            senders, timers = _signal_senders(phase, state, t['signal'])
            item['senders'], item['timers'] = senders, timers
        if per_wolf and (personal or ('if' in t and rules.triggers.uses_wolf(t['if'], phase))):
            could = []
            for wolf in cohort:
                if repeat in ('player', 'member') and wolf in used_by:
                    continue
                if 'if' in t:
                    verdict = rules.triggers.judge(t['if'], state, {**stand_in, 'actor': wolf}, ready, phase.get('_regions'),
                                                   known | {'actor'}, phase, day)['verdict']
                    if verdict is False:
                        continue
                could.append(wolf)
            item['forWolves'] = could
        item['needsWolves'] = personal
        out.append(item)
    for index, edge in enumerate(phase['exits']):
        item = {'source': 'exit', 'index': index, 'on': edge['on'], 'to': edge['to'],
                **{k: edge[k] for k in ('match', 'afterDays', 'missions', 'players', 'percent', 'participant', 'minCoins', 'minQuantity') if k in edge}}
        gate = edge.get('participant') or 'players' in edge or 'percent' in edge or edge['on'] == rules.MISSION_GATE
        item['needsWolves'] = bool(gate)
        if 'players' in edge or 'percent' in edge:
            votes = state.get('votes', {}).get(str(index), [])
            item['votes'] = {'have': len(votes), 'need': edge.get('players', max(1, math.ceil(len(cohort) * edge.get('percent', 100) / 100))),
                             'wolves': votes}
        if edge.get('prerequisites'):
            item['prerequisites'] = {'steps': len(edge['prerequisites']), 'progress': state.get('progress', {}).get(str(index), {})}
        if edge['on'] == rules.MISSION_GATE:
            done = ready()
            item['ready'] = [m for m in edge['missions'] if m in done]
            item['missing'] = [m for m in edge['missions'] if m not in done]
        if edge['on'] == 'clock' or 'afterDays' in edge:
            item['due'] = state['entered'] + edge.get('afterDays', 0)
        out.append(item)
    # Can each still fire by itself? Off or used up: no. On a wolf's doing: only while a wolf of the story is about. A
    # signal: only while something that can still fire sends it, or a timer runs for it (worked out to a fixed point).
    # (A running timer sends its signal as the wolf who started it, gone or not: what it feeds is never "gone".)
    timed = {t.get('signal', k) for k, t in state.get('timers', {}).items()}
    fed = {m['id'] for m in phase.get('missions', []) if any(o.get('signal') in timed for o in m['objectives'])}
    for item in out:
        if item.get('off') or item.get('used'):
            item['can'], item['cannot'] = False, 'off' if item.get('off') else 'used'
        elif item['on'] == rules.triggers.SIGNAL and item['timers']:
            item['can'] = True
        elif item['on'] == rules.MISSION_GATE and set(item.get('missing', [])) & fed:
            item['can'] = True
        elif item['needsWolves'] and recent is not None and not (set(recent) - set(item.get('usedFor', []))):
            item['can'], item['cannot'] = False, 'gone'
        elif item['on'] == rules.triggers.SIGNAL:
            item['can'] = bool(item['timers'])
        else:
            item['can'] = True
    changed = True
    while changed:
        changed = False
        able = {i['id'] for i in out if i['source'] == 'trigger' and i['can']}
        for item in out:
            if item['on'] == rules.triggers.SIGNAL and not item['can'] and not item.get('cannot'):
                if any(sender['trigger'] in able for sender in item['senders']):
                    item['can'], changed = True, True
    for item in out:
        if not item['can'] and not item.get('cannot'):
            item['cannot'] = 'unsent'
    return out


def rehearse_steps(program, case):
    """A rehearsal case stepped through (doc 85, 4.5; Phase 4): story_program.rehearse_steps, each step with listeners()
    on the rehearsal's state, as the Campaigns console explains a running story ("Waiting for"), so an author sees
    which triggers are waiting and why before anything is live."""
    def look(state):
        return listeners(program, state, per_wolf=True)
    return rules.rehearse_steps(program, case, look=look)


def _stall(conn, world, program, state, limits):
    """Doc 85, 5.6's "cannot move on": an ongoing phase none of whose triggers and exits can still fire by itself (each
    off, used up, waiting for a wolf who has been gone `goneDays`, or a signal nothing left can send). None when it can
    move."""
    phase = rules.phase_of(program, state)
    if not (phase['exits'] or phase.get('triggers')) or state.get('waiting'):
        return None   # (Version 2: a trigger waiting part-way through its actions goes on by itself.)
    judged = listeners(program, state, per_wolf=False)
    if any(i['can'] for i in judged if not i['needsWolves']):
        return None
    recent = _recent_wolves(conn, world, state.get('cohort', []), limits['goneDays']) if any(i['needsWolves'] for i in judged) else set()
    judged = listeners(program, state, per_wolf=False, recent=recent)
    if any(i['can'] for i in judged):
        return None
    why = Counter(i['cannot'] for i in judged)
    return {'phase': state['phase'], 'reasons': dict(why), 'goneDays': limits['goneDays'],
            'what': [{'source': i['source'], 'id': i.get('id'), 'index': i.get('index'), 'on': i['on'], 'cannot': i['cannot']} for i in judged][:24]}


def _timers_ahead(state, day, speed):
    """What will happen next by itself: the run's timers, soonest first, with the day and an estimate in real time."""
    out = []
    published = state.get('publishedAt')
    elapsed = (time.time() - published) / 86400 if published else None
    for key, timer in state.get('timers', {}).items():
        clock = timer.get('clock', 'game')
        now = elapsed if clock == 'real' else day
        left = None if now is None else timer['at'] - now
        out.append({'name': key, 'signal': timer.get('signal', key), 'at': timer['at'], 'clock': clock, 'actor': timer.get('actor', ''),
                    'repeat': timer.get('interval', 0), 'scope': timer.get('scope', 'visit'), 'left': left,
                    'seconds': None if left is None else _estimate(max(0, left), clock, speed)})
    out.sort(key=lambda t: (t['seconds'] is None, t['seconds'] if t['seconds'] is not None else t['at']))
    return out


def _load(conn, world, run):
    row = conn.execute(
        """SELECT program,state,status,cursor,receipts,digest,approved_by,program->>'title' FROM dm.story_releases
           WHERE world_id=%s AND id=%s""", (world, run)).fetchone()
    rules.require(row, 'Unknown run.')
    rules.require(row[1] and row[2] in (*STEERED, 'stopping'), 'This run is not running: there is nothing to explain.')
    return row


def _event_view(r):
    return dict(zip(('id', 'kind', 'day', 'actor', 'target', 'cell', 'item', 'quantity', 'coins', 'detail', 'x', 'y', 'at'),
                    (*r[:12], _iso(r[12]))))


def _step(phase, state, run, t, fact, fired_events):
    """Where a fact stopped on its way through one trigger, as the runner and the rules take it (doc 85, 5.5's "why has
    this one not fired?"): {verdict, ...}. Once-rules and conditions are judged with the values as they are now."""
    if fact.get('id') in fired_events:
        return {'verdict': 'fired'}
    if fact['kind'] in rules.SCENE_EVENTS and (fact.get('target') != run or fact.get('quantity') != state['visits']):
        return {'verdict': 'other', 'text': 'a scene of another story or another visit'}
    if fact['kind'] in rules.REGION_EVENTS and fact.get('detail') != run:
        return {'verdict': 'other', 'text': "another story's region"}
    if fact['kind'] == 'story phase' and fact.get('actor') == run:
        return {'verdict': 'other', 'text': "the story's own change of phase"}
    if _before_visit(state, fact):
        return {'verdict': 'before', 'text': 'before this visit of the phase opened'}
    miss = _filter_miss(t, fact)
    if miss:
        return {'verdict': 'filter', **miss}
    actor = fact.get('actor', '')
    repeat, used_by = _once_marks(phase, state, t)
    if (t.get('audience') == 'participant' or repeat in ('player', 'member')) and actor not in state['cohort']:
        return {'verdict': 'outsider', 'wolf': actor}
    if rules.triggers.trigger_key(phase, t['id']) in set(state.get('disabled', ())):
        return {'verdict': 'off'}
    owner = actor if repeat in ('player', 'member') else '*'
    if repeat != 'each' and owner in used_by:
        return {'verdict': 'used', 'once': repeat, **({'wolf': actor} if owner != '*' else {})}
    if 'if' in t:
        tree = rules.triggers.judge(t['if'], state, fact, _ready(phase, state), phase.get('_regions'), None, phase, fact.get('day'))
        if tree['verdict'] is not True:
            return {'verdict': 'conditions', 'conditions': tree}
    return {'verdict': 'would', 'text': 'it would fire on this event now'}


def _before_visit(state, fact):
    """Whether a logged event came before the run's current visit: by the cursor the visit opened at (state.visitCursor,
    since Phase 3: the event that moved it, already the last phase's), or for an older run by the game day."""
    if isinstance(fact.get('id'), int) and isinstance(state.get('visitCursor'), int):
        return fact['id'] <= state['visitCursor']
    return fact.get('day') is not None and fact['day'] <= state['entered'] and fact.get('id') is not None


def _exit_step(phase, state, edge, fact):
    actor = fact.get('actor', '')
    if _before_visit(state, fact):
        return {'verdict': 'before', 'text': 'before this visit of the phase opened'}
    if (edge.get('participant') or 'players' in edge or 'percent' in edge) and actor not in state['cohort']:
        return {'verdict': 'outsider', 'wolf': actor}
    miss = _filter_miss(edge, fact)
    if miss:
        return {'verdict': 'filter', **miss}
    if fact.get('day') is not None and fact['day'] < state['entered'] + edge.get('afterDays', 0):
        return {'verdict': 'early', 'due': state['entered'] + edge.get('afterDays', 0)}
    return {'verdict': 'would', 'text': 'it matches: as a vote or a step towards it'}


def why_not(conn, world, run, which):
    """Why has this one not fired? (doc 85, 5.5): a trigger (by id) or an exit (by index) of the run's phase, judged now,
    and its last events (up to 10) with where each stopped."""
    program, state, status, cursor, receipts, digest, who, title = _load(conn, world, run)
    phase = rules.phase_of(program, state)
    judged = listeners(program, state)
    if isinstance(which, dict) and 'exit' in which:
        rules.require(rules.integer(which['exit'], 0, 15) and which['exit'] < len(phase['exits']), 'No such exit in this phase.')
        rule, head, kinds = phase['exits'][which['exit']], next(i for i in judged if i['source'] == 'exit' and i['index'] == which['exit']), None
    else:
        tid = which.get('trigger') if isinstance(which, dict) else which
        rule = next((t for t in phase.get('triggers', []) if t['id'] == tid), None)
        rules.require(rule, f'Trigger {tid} is not in phase {state["phase"]}.')
        head = next(i for i in judged if i['source'] == 'trigger' and i['id'] == tid)
    out = {'run': run, 'phase': state['phase'], 'visit': state['visits'], 'listener': head, 'events': [], 'unread': 0}
    on = rule['on']
    if on in ('clock', 'dm signal', rules.triggers.SIGNAL, rules.MISSION_GATE):
        # (Not in the world's log: a delay, a direction, a signal, the journal's missions. Its head says what it waits for.)
        out['notLogged'] = on
        if on == rules.triggers.SIGNAL:
            out['recent'] = [dict(zip(('id', 'visit', 'trigger', 'day', 'at'), (r[0], r[1], r[2], r[3], _iso(r[4])))) for r in conn.execute(
                """SELECT id,visit,entry->>'trigger',(entry->>'day')::float8,recorded_at FROM dm.story_trigger_history
                   WHERE run=%s AND entry->>'signal'=%s ORDER BY id DESC LIMIT 5""", (run, rule['signal']))]
        return out
    kinds = [on, 'gift again'] if on == 'gift' else [on]
    rows = conn.execute(
        """SELECT id,kind,game_day,actor,target,cell,item,quantity,coins,detail,x,y,recorded_at FROM game.events
           WHERE world_id=%s AND kind=ANY(%s) AND id<=%s ORDER BY id DESC LIMIT %s""",
        (world, kinds, cursor, EXPLAIN_EVENTS)).fetchall()
    out['unread'] = conn.execute('SELECT count(*) FROM game.events WHERE world_id=%s AND kind=ANY(%s) AND id>%s',
                                 (world, kinds, cursor)).fetchone()[0]
    fired = set()
    if 'id' in rule:
        fired = {int(r[0]) for r in conn.execute(
            """SELECT entry->>'event' FROM dm.story_trigger_history WHERE run=%s AND entry->>'trigger'=%s
               AND entry->>'event' ~ '^[0-9]+$' ORDER BY id DESC LIMIT 200""", (run, rule['id']))}
    for r in rows:
        fact = dict(zip(EVENT_COLUMNS, r[:12]))
        step = _step(phase, state, run, rule, fact, fired) if 'id' in rule else _exit_step(phase, state, rule, fact)
        out['events'].append({'event': _event_view(r), **step})
    return out


def _validate_fact(fact):
    rules.require(isinstance(fact, dict), 'Describe the event.')
    known = {'kind', 'signal', 'actor', 'target', 'cell', 'item', 'detail', 'quantity', 'coins', 'x', 'y'}
    rules.require(set(fact) <= known, 'Unknown field in the event: ' + ', '.join(sorted(set(fact) - known)))
    kind = fact.get('kind')
    rules.require(kind == rules.triggers.SIGNAL or (kind in rules.EVENTS and kind not in ('clock', 'dm signal', 'story phase')),
                  'Choose a world event, or a story signal.')
    rules.require(kind != rules.triggers.SIGNAL or rules.identifier(fact.get('signal')), 'Name the signal.')
    for k in rules.FIELDS:
        rules.require(k not in fact or (isinstance(fact[k], str) and len(fact[k]) <= 400), f'Invalid {k}.')
    for k in ('quantity', 'coins'):
        rules.require(k not in fact or rules.integer(fact[k], -(10**12), 10**12), f'Invalid {k}.')
    rules.require(('x' not in fact and 'y' not in fact) or (rules.identifier(fact.get('cell')) and rules.integer(fact.get('x'), 0, 4095)
                  and rules.integer(fact.get('y'), 0, 4095)), 'A place needs a cell, x and y.')
    return {'id': None, 'detail': '', 'target': '', 'item': '', 'cell': '', 'actor': '', 'quantity': 0, 'coins': 0, **fact}


def _missions_moved(phase, before, after):
    """Mission progress a dry run made: [{mission, title, who, before, after, of}]."""
    out = []
    for m in phase.get('missions', []):
        a = before.get(m['id'], {}).get('tracks', {})
        b = after.get(m['id'], {}).get('tracks', {})
        for who in sorted(set(a) | set(b)):
            if a.get(who) != b.get(who):
                out.append({'mission': m['id'], 'title': m['title'], 'who': who, 'before': a.get(who, [0, 0]), 'after': b.get(who, [0, 0]),
                            'of': len(m['objectives'])})
    return out


def _outcome(program, before, after, trace, moved, actor):
    """What a dry run would do, from the state before and after: the triggers fired, the values changed, mission progress,
    the effects it would send, and the phase it would be in (with that phase's entry effects to follow)."""
    phase = rules.phase_of(program, before)
    out = {'fired': trace, 'changes': _diff(_values(before, actor), _values(after, actor)),
           'missions': _missions_moved(phase, before.get('missions', {}), after.get('missions', {}) if not moved else
                                       {m['id']: {'tracks': m['tracks']} for m in after.get('previousMissions', [])}),
           'effects': list(after.get('triggerEffects', [])), 'moved': bool(moved), 'phase': after['phase'], 'visit': after['visits']}
    if moved:
        landing = rules.phase_of(program, after)
        out['entryEffects'] = entry_effects(landing, after)
        out['ending'] = not (landing['exits'] or landing.get('triggers'))
        out['journal'] = landing['journal']
    return out


def what_if(conn, world, run, fact):
    """What if this happened? (doc 85, 5.5): a made-up event (or story signal) through the rules on a copy of the run's
    state, now. Nothing is saved, and no order is sent. With, for each trigger of the phase that listens for its kind and
    did not fire, where it stopped."""
    program, state, status, cursor, receipts, digest, who, title = _load(conn, world, run)
    fact = _validate_fact(fact)
    if fact['kind'] in rules.SCENE_EVENTS:
        fact.update(target=run, quantity=state['visits'])     # (A made-up scene event is this story's, this visit's.)
    if fact['kind'] in rules.REGION_EVENTS:
        fact['detail'] = run
    rules.require(not fact['actor'] or fact['actor'] in state['cohort'] or fact['kind'] != rules.triggers.SIGNAL,
                  'That wolf is not in this story.')
    phase = rules.phase_of(program, state)
    day = max(game_day(conn, world), state['entered'])
    published = state.get('publishedAt', 0)
    elapsed = max(0, (time.time() - published) / 86400) if published else 0
    world_load(conn, world, program, state)
    copy = json.loads(json.dumps(state))
    trace = []
    if fact['kind'] == rules.triggers.SIGNAL:
        moved = rules.dispatch(program, phase, copy, fact, day, trace, elapsed)
    else:
        moved = rules.advance(program, copy, {**fact, 'day': day}, day, trace, elapsed)
    out = {'run': run, 'fact': fact, 'day': day, **_outcome(program, state, copy, trace, moved, fact['actor'])}
    fired = {f['trigger'] for f in trace}
    out['notFired'] = [{'trigger': t['id'], **_step(phase, state, run, t, {**fact, 'day': day}, set())}
                       for t in phase.get('triggers', []) if t['id'] not in fired and
                       (t['on'] == fact['kind'] or (t['on'] == 'gift' and fact['kind'] == 'gift again'))][:16]
    return out


def preview(conn, world, run, who, kind, payload, may=None):
    """What a console command would do (doc 85, 5.5: "this will do", shown before every [Fire…] and [Go to phase]): the
    command taken by the runner's own code (Steer) on the run as it stands, in a transaction that is then rolled back.
    Its verdict (done, refused with why, or waiting for the game), its words, the values before and after, the triggers it
    fires, the orders it would send at once, and what the run would send next (the new phase's entry effects, the
    triggers' effects). Nothing is saved, queued or audited."""
    import psycopg

    rules.require(kind in COMMAND_KINDS, 'Unknown command.')
    payload = payload or {}
    row = conn.execute('SELECT program,state,status,digest FROM dm.story_releases WHERE world_id=%s AND id=%s', (world, run)).fetchone()
    rules.require(row, 'Unknown run.')
    program, state, status, digest = row
    rules.require(status in STEERED and state, f'This run is {status}: there is nothing to steer.')
    _check_payload(program, kind, payload)
    _may_release(program, kind, payload, may)
    day = None

    def clock():
        nonlocal day
        if day is None:
            day = game_day(conn, world)
        return day

    recorded = []
    out = {'run': run, 'kind': kind, 'payload': payload}
    token = _recording.set(recorded)
    try:
        with conn.transaction():
            steer = Steer(conn, world, run, clock, (0, 'preview', state['visits'], digest, kind, payload, 'a dry run', str(who)[:80], 0),
                          lock=False)
            before = json.loads(json.dumps(steer.state))
            try:
                result = steer.take()
                out.update(outcome='done', text=result['text'], before=result['before'], after=result['after'])
            except Wait:
                out.update(outcome='waits', text='It would wait: the story is still sending the orders of this visit to the game. '
                           f'It is taken once the game has answered them (refused after {FLOW_WAIT_SECONDS} s).')
            except (Refused, ValueError, KeyError, TypeError, IndexError, StopIteration, AttributeError, psycopg.Error) as why:
                # (As take_commands records them: the runner would refuse it, or could not carry it out.)
                out.update(outcome='refused', text=str(why) if isinstance(why, (Refused, ValueError)) else
                           f'Could not carry it out: {type(why).__name__} {why}'[:300])
            after = steer.state
            program = steer.program   # (A move onto a new version changes it, doc 85 Phase 5b.)
            if steer.report is not None:
                out['migration'] = steer.report
            out.update(orders=list(recorded), fired=steer.trace, moved=bool(steer.moved),
                       changes=_diff(_values(before, steer.payload.get('actor') or steer.payload.get('player') or ''),
                                     _values(after, steer.payload.get('actor') or steer.payload.get('player') or '')),
                       phase=after['phase'], visit=after['visits'], status=steer.status)
            if out['outcome'] == 'done':
                # (What the run sends next, after the command: entry effects of a new visit, the triggers' effects.)
                landing = rules.phase_of(program, after)
                out['next'] = [*(after.get('arrivalEffects', []) if steer.moved else []), *entry_effects(landing, after),
                               *after.get('triggerEffects', [])] if steer.moved else list(after.get('triggerEffects', []))
                if steer.moved:
                    out['journal'] = landing['journal']
                    out['ending'] = not (landing['exits'] or landing.get('triggers'))
                out['missions'] = _missions_moved(rules.phase_of(program, before), before.get('missions', {}), after.get('missions', {})) \
                    if not steer.moved and kind != 'migrate' else []
            raise psycopg.Rollback()
    finally:
        _recording.reset(token)
    return out


def explain(conn, world, run, data=None, who='', may=None):
    """The op `explain` (doc 85, 5.5): with nothing more, what the run waits for ({listeners, timers, stall}); with
    `trigger` or `exit`, why it has not fired; with `fact`, what if it happened; with `command`, what that console command
    would do."""
    data = data or {}
    if 'command' in data:
        c = data['command'] if isinstance(data['command'], dict) else {}
        return preview(conn, world, run, who, c.get('kind'), c.get('payload'), may)
    if 'fact' in data:
        return what_if(conn, world, run, data['fact'])
    if 'trigger' in data or 'exit' in data:
        return why_not(conn, world, run, {k: data[k] for k in ('trigger', 'exit') if k in data})
    program, state, status, cursor, receipts, digest, approver, title = _load(conn, world, run)
    limits = alert_rules(conn, world)
    speed = world_speed(conn, world)
    now = _now(conn, world)
    recent = sorted(_recent_wolves(conn, world, state.get('cohort', []), limits['goneDays']))
    world_load(conn, world, program, state)
    judged = listeners(program, state, recent=recent, day=now['day'])
    phase = rules.phase_of(program, state)
    stall = None
    if status == 'running' and (phase['exits'] or phase.get('triggers')) and not any(i['can'] for i in judged):
        stall = _stall(conn, world, program, state, limits)
    return {'run': run, 'phase': state['phase'], 'visit': state['visits'], 'status': status, 'day': now['day'],
            'listeners': judged, 'timers': _timers_ahead(state, now['day'], speed), 'speed': speed, 'recent': recent,
            'goneDays': limits['goneDays'], 'stall': stall, 'ending': not (phase['exits'] or phase.get('triggers')),
            # (The events its missions' objectives take: a "what if" may try those too.)
            'missionEvents': sorted({o['on'] for m in phase.get('missions', []) for o in m['objectives']})}


# ----- the alerts outside the Campaigns tab (doc 85, 5.6: a count on the tab bar, a line on LIVE, the Dev Console)

def _alert_words(title, alert):
    """One line for a "needs you" alert, for the game's Dev Console (plain words: the game relays them as they are)."""
    if alert['kind'] == 'paused':
        return f'Story "{title}" is paused: {(alert.get("problem") or "the game refused an order").rstrip(". ")}. Retry or skip it in Campaigns.'
    if alert['kind'] == 'stuck':
        return f'Story "{title}" cannot move on from {alert.get("phase")}: nothing in it can still fire by itself. See Campaigns.'
    return f'Story "{title}" needs you ({alert["kind"]}). See Campaigns.'


def alert_summary(conn, world, view=None):
    """The op `alerts`: what the tab bar and LIVE show (doc 85, 5.6): whether the runner is down while stories should be
    running, how many runs need the admin, and those runs with their "needs you" alerts."""
    view = view or overview(conn, world)
    runs, down = [], False
    for r in view['runs']:
        needs = [a for a in r.get('alerts', []) if a.get('needsYou')]
        down = down or any(a['kind'] == 'runner' for a in needs)
        others = [a for a in needs if a['kind'] != 'runner']
        if others or r['status'] == 'approved':
            runs.append({'run': r['run'], 'title': r['title'], 'status': r['status'], 'kinds': [a['kind'] for a in others] or ['publish'],
                         'text': _alert_words(r['title'], others[0]) if others else f'Story "{r["title"]}" is approved, waiting to be published.'})
    beat = view.get('runner')
    live = sum(r['status'] in ('running', 'stopping') for r in view['runs'])
    return {'runner': beat, 'runnerDown': down, 'live': live, 'needsYou': len(runs) + (1 if down else 0), 'runs': runs,
            'at': view['now']['at']}


_GAME_ALERTS_EVERY = 10.0
_game_alerts_at = {}


def _game_alerts(conn, world):
    """The "needs you" alerts the runner leaves for the game's Dev Console (dm.story_runner.alerts, 0072): each with a key
    (run and kind), so the game says each once."""
    view = overview(conn, world)
    out = []
    for r in view['runs']:
        for a in r.get('alerts', []):
            if a.get('needsYou') and a['kind'] != 'runner':
                out.append({'key': f'{r["run"]}:{a["kind"]}:{r.get("visit")}', 'run': r['run'], 'kind': a['kind'], 'text': _alert_words(r['title'], a)})
    return out[:20], sum(r['status'] in ('running', 'stopping') for r in view['runs'])


def _beat_alerts(conn, world):
    """Every ten seconds, with the heartbeat: the alerts for the game (when migration 0072 is there)."""
    if time.monotonic() - _game_alerts_at.get(world, -1e9) < _GAME_ALERTS_EVERY:
        return
    import psycopg

    if not conn.execute("SELECT to_regprocedure('dm.story_alerts_for_game(text)') IS NOT NULL").fetchone()[0]:
        return
    _game_alerts_at[world] = time.monotonic()
    try:
        with conn.transaction():
            alerts, live = _game_alerts(conn, world)
            conn.execute('UPDATE dm.story_runner SET alerts=%s::jsonb,alerts_at=statement_timestamp(),live=%s WHERE world_id=%s',
                         (json.dumps(alerts), live, world))
    except (ValueError, KeyError, TypeError, IndexError, StopIteration, AttributeError, psycopg.Error):
        pass   # (The alerts are a courtesy: the heartbeat and the stories go on without them. A lost connection fails the
               # tick's next statement, as before.)


# ----- every problem of a draft, with the world's (doc 85, Phase 3; known problem 4)

def _effect_sites(program):
    """Every effect of a program with where it is: entry effects, and the effects of every trigger's actions."""
    for pi, ph in enumerate(program['phases']):
        for ei, e in enumerate(ph['effects']):
            yield f'phases[{pi}].effects[{ei}]', {'phase': pi, 'phaseId': ph['id'], 'kind': 'effect', 'index': ei}, e
    lists = [(None, None, 'triggers', program.get('triggers', []))] + [
        (i, ph['id'], f'phases[{i}].triggers', ph.get('triggers', [])) for i, ph in enumerate(program['phases'])]
    for pi, pid, path, rules_ in lists:
        for ti, t in enumerate(rules_):
            for address, a in rules.triggers.walk(t['actions']):
                if a['do'] == 'effect':
                    part = rules.triggers.part_of(address)
                    yield (f'{path}[{ti}].{part}', {'phase': pi, 'phaseId': pid, 'kind': 'trigger', 'index': ti, 'id': t['id'],
                           'part': part}, a['effect'])


def _filter_sites(program):
    """Every rule with filters, with where it is and what it is: exits and their prerequisites, triggers, objectives."""
    lists = [(None, None, 'triggers', program.get('triggers', []))] + [
        (i, ph['id'], f'phases[{i}].triggers', ph.get('triggers', [])) for i, ph in enumerate(program['phases'])]
    for pi, pid, path, rules_ in lists:
        for ti, t in enumerate(rules_):
            yield f'{path}[{ti}]', {'phase': pi, 'phaseId': pid, 'kind': 'trigger', 'index': ti, 'id': t['id']}, 'trigger', t
    for pi, ph in enumerate(program['phases']):
        for xi, e in enumerate(ph['exits']):
            at = {'phase': pi, 'phaseId': ph['id'], 'kind': 'exit', 'index': xi}
            yield f'phases[{pi}].exits[{xi}]', at, 'exit', e
            for si, step in enumerate(e.get('prerequisites', [])):
                yield f'phases[{pi}].exits[{xi}].prerequisites[{si}]', {**at, 'part': f'prerequisites[{si}]'}, 'prerequisite', step
        for mi, m in enumerate(ph.get('missions', [])):
            for oi, o in enumerate(m['objectives']):
                yield (f'phases[{pi}].missions[{mi}].objectives[{oi}]', {'phase': pi, 'phaseId': ph['id'], 'kind': 'mission', 'index': mi,
                       'id': m['id'], 'part': f'objectives[{oi}]'}, 'objective', o)


def _tile_words(conn, world, cell, x, y):
    dimensions = conn.execute(
        'SELECT width,height FROM world.cells WHERE world_id=%s AND id=%s '
        'UNION ALL SELECT width,height FROM world.interiors WHERE world_id=%s AND id=%s',
        (world, cell, world, cell),
    ).fetchone()
    return None if dimensions and x < dimensions[0] and y < dimensions[1] else f'Tile ({x},{y}) lies outside {cell}.'


def reference_problems(conn, world, program):
    """Every reference of a program checked against this world, each with where it is (doc 85, Phase 3): what approval
    checks (`blocks`: approval refuses it, as `references` does), and what it does not (known problem 4: ships, areas,
    towns and markets, the tiles of npc.move and project.post, exits' filters and prerequisites, the wolves and items in
    filters), which the game would refuse when the order comes, pausing the run, or which would make a trigger never fire."""
    out = []

    def add(path, at, text, blocks):
        out.append({'level': 'warning', 'world': True, 'blocks': blocks, 'text': text, 'path': path, 'at': at})

    def exists(table, column, value):
        return conn.execute(f'SELECT 1 FROM {table} WHERE world_id=%s AND {column}=%s', (world, value)).fetchone() is not None

    for i, r in enumerate(program.get('regions', [])):
        words = _tile_words(conn, world, r['cell'], r['x2'], r['y2'])
        if words:
            add(f'regions[{i}]', {'kind': 'region', 'index': i, 'id': r['id']}, words, True)
    tables = {'npc': ('dm.story_residents', 'id'), 'resident': ('dm.story_residents', 'id'), 'player': ('game.characters', 'key'),
              'faction': ('live.factions', 'id'), 'other': ('live.factions', 'id'), 'place': ('world.areas', 'id'), 'cell': ('world.areas', 'id')}
    society = conn.execute("SELECT payload::jsonb->'society' FROM game.checkpoints WHERE world_id=%s", (world,)).fetchone()
    society = society[0] if society and isinstance(society[0], dict) else {}
    report = society.get('report') if isinstance(society.get('report'), dict) else {}
    board = society.get('board') if isinstance(society.get('board'), dict) else {}
    markets = {m.get('id') for m in report.get('markets', []) if isinstance(m, dict)} or set(board.get('cell') or {})
    areas = {a.get('id') for a in report.get('areas', []) if isinstance(a, dict)}
    ships = {s.get('id') for s in board.get('ships', []) or [] if isinstance(s, dict)}
    towns = {r[0] for r in conn.execute(
        """SELECT region FROM world.cells WHERE world_id=%s AND region<>'unassigned' UNION SELECT region FROM world.interiors
           WHERE world_id=%s AND region<>'unassigned'""", (world, world))}
    templates = {t['id'] for t in json.loads((DATA / 'Storylines/templates.json').read_text())['templates'] if t.get('source') == 'dm'}
    items = {item['id'] for item in json.loads((DATA / 'Items/items.json').read_text())['items']}
    projects = {k['id'] for k in json.loads((DATA / 'Town/projects.json').read_text())['kinds']}
    for path, at, e in _effect_sites(program):
        for key, value in e.items():
            if key in tables and value not in ('$actor', '$each') and not (e['kind'] == 'scene.play' and key == 'player' and value == 'world'):
                if not exists(*tables[key], value):
                    add(path, at, f'Unknown {key} in this world: {value}', True)
        kind = e['kind']
        if kind == 'storyline.give' and e['template'] not in templates:
            add(path, at, 'Unknown DM quest template.', True)
        if kind in ('npc.walk', 'npc.move', 'project.post'):
            words = _tile_words(conn, world, e['cell'], e['x'], e['y'])
            if words:
                add(path, at, words, kind == 'npc.walk')
        if kind == 'story.area.item' and e['item'] not in items:
            add(path, at, 'Unknown item for a region grant.', True)
        if kind == 'project.post' and e['project'] not in projects:
            add(path, at, 'Unknown town project.', True)
        if kind == 'times.nudge' and markets and e['market'] not in markets:
            add(path, at, f'No market "{e["market"]}" in this world (as last saved): the game would refuse it.', False)
        if kind == 'harvest.override' and areas and e['area'] not in areas:
            add(path, at, f'No harvest area "{e["area"]}" in this world (as last saved): the game would refuse it.', False)
        if kind in ('festival.call', 'project.post') and towns and e['town'] not in towns:
            add(path, at, f'No town "{e["town"]}" in this world: the game would refuse it.', False)
        if kind == 'ship.storm' and e['ship'] not in ships:
            add(path, at, f'Ship "{e["ship"]}" is not due in this world now (as last saved): the game would refuse it unless it is '
                'due when the order comes.', False)
    for si, scene in enumerate(program.get('scenes', [])):
        at = {'kind': 'scene', 'index': si, 'id': scene['id']}
        for name in ('area', 'camera'):
            if name in scene:
                place = scene[name]
                words = _tile_words(conn, world, place['cell'], place.get('x2', place.get('x')), place.get('y2', place.get('y')))
                if words:
                    add(f'scenes[{si}].{name}', at, words, True)
        for role in scene['cast']:
            if 'npc' in role and not exists('dm.story_residents', 'id', role['npc']):
                add(f'scenes[{si}].cast', at, f'Unknown npc in this world: {role["npc"]}', True)
            elif 'npc' not in role and 'cell' in role.get('selector', {}) and not exists('world.areas', 'id', role['selector']['cell']):
                add(f'scenes[{si}].cast', at, f'Unknown cell in this world: {role["selector"]["cell"]}', True)
        for beat in scene['beats']:
            if beat['type'] == 'move':
                words = _tile_words(conn, world, beat['cell'], beat['x'], beat['y'])
                if words:
                    add(f'scenes[{si}].beats', at, words, True)
    for pi, ph in enumerate(program['phases']):
        for npc in ph.get('guestVoices', []):
            if not exists('dm.story_residents', 'id', npc):
                add(f'phases[{pi}].guestVoices', {'phase': pi, 'phaseId': ph['id'], 'kind': 'phase'}, f'Unknown npc in this world: {npc}', True)
    for player in program['audience'].get('players', []):
        if not exists('game.characters', 'key', player):
            add('audience', {'kind': 'settings'}, f'Unknown player in this world: {player}', True)

    def character(value):
        return exists('dm.story_residents', 'id', value) or exists('game.characters', 'key', value)

    for path, at, what, rule in _filter_sites(program):
        filters = rule.get('match', {})
        # (Approval checks these of objectives and triggers; an exit's and a prerequisite's are new: known problem 4.)
        blocks = what in ('trigger', 'objective')
        if filters.get('cell') and not exists('world.areas', 'id', filters['cell']):
            add(path, at, f'Unknown cell in this world: {filters["cell"]}', blocks)
        if rule['on'] == 'enter place' and filters.get('target') and not exists('world.areas', 'id', filters['target']):
            add(path, at, f'Unknown place in this world: {filters["target"]}', blocks)
        if rule['on'] in ('conversation', 'gift', 'gift again') and filters.get('target') and not character(filters['target']):
            add(path, at, f'Unknown mission character in this world: {filters["target"]}', blocks)
        if filters.get('actor') and not character(filters['actor']):
            add(path, at, f'No wolf or resident "{filters["actor"]}" in this world: this never matches.', False)
        if filters.get('item') and __import__('re').split('[~@]', filters['item'], maxsplit=1)[0] not in items:
            add(path, at, f'No item "{filters["item"]}" in the item catalogue: this never matches.', False)
    return out


def program_problems(conn, world, program, with_world=True):
    """The op `problems` (doc 85, Phase 3; 4.2's Problems list): every error of the draft at once, the warnings the program
    alone can tell, and (for a Dungeon Master, once the draft has no error) the world's."""
    out = rules.problems(program)
    errors = sum(p['level'] == 'error' for p in out)
    if with_world and not errors and conn is not None:
        out += reference_problems(conn, world, program)
    return {'problems': out, 'errors': errors, 'warnings': len(out) - errors, 'world': bool(with_world and not errors and conn is not None),
            'digest': rules.digest(program) if not errors else None}


def atlas(conn, world, query):
    rules.require(isinstance(query, str) and len(query) <= 100, 'Search using a short name or ID.')
    rows = conn.execute(
        """SELECT kind,id,name,width,height FROM (SELECT kind,id,name,world_id,width,height FROM world.cells UNION ALL
            SELECT kind,id,name,world_id,width,height FROM world.interiors UNION ALL
            SELECT 'npc',id,name,world_id,NULL,NULL FROM dm.story_residents UNION ALL
            SELECT 'faction',id,name,world_id,NULL,NULL FROM live.factions) atlas
            WHERE world_id=%s AND (strpos(lower(id),lower(%s))>0 OR strpos(lower(name),lower(%s))>0)
            ORDER BY name,id LIMIT 100""",
        (world, query, query),
    )
    return {'matches': [
        {**dict(zip(('kind', 'id', 'name'), r)), **({'width': r[3], 'height': r[4]} if r[3] is not None else {})}
        for r in rows
    ]}


REFS_LIMIT = 5000


def static_refs():
    """What a picker may offer that is the same in every world (doc 85, 4.3): items with their categories, the DM's
    personal-quest templates, the kinds of town project, injuries. A viewer gets these; the world's are a DM's."""
    items = json.loads((DATA / 'Items/items.json').read_text())['items']
    templates = json.loads((DATA / 'Storylines/templates.json').read_text())['templates']
    projects = json.loads((DATA / 'Town/projects.json').read_text())['kinds']
    return {'items': [{'id': i['id'], 'name': i.get('name') or i['id'], 'category': i.get('category', '')} for i in items],
            'templates': [{'id': t['id'], 'name': t.get('title') or t['id']} for t in templates if t.get('source') == 'dm'],
            'projects': [{'id': k['id'], 'name': k.get('title') or k.get('name') or k['id']} for k in projects],
            'injuries': list(rules.INJURY_TYPES)}


def refs(conn, world):
    """Everything a parameter of a trigger may name in this world, by kind, for the Story Creator's pickers (doc 85, 4.3
    and Phase 4, op `refs`; the atlas search grown): residents, wolves, places, factions, towns, markets, harvest areas and
    ships, beside static_refs(). Nobody types an id."""
    out = static_refs()
    out['residents'] = [{'id': r[0], 'name': r[1] or r[0], 'home': r[2] or '', 'work': r[3] or ''} for r in conn.execute(
        'SELECT id,name,home_area,work_area FROM dm.story_residents WHERE world_id=%s ORDER BY name,id LIMIT %s', (world, REFS_LIMIT))]
    out['wolves'] = [{'id': r[0], 'name': r[1] or r[0]} for r in conn.execute(
        "SELECT key,data->>'name' FROM game.characters WHERE world_id=%s ORDER BY data->>'name',key LIMIT %s", (world, REFS_LIMIT))]
    out['places'] = [{'id': r[1], 'kind': r[0], 'name': r[2] or r[1], 'width': r[3], 'height': r[4], 'town': r[5] or ''} for r in conn.execute(
        """SELECT 'cell',id,name,width,height,region FROM world.cells WHERE world_id=%s UNION ALL
           SELECT 'interior',id,name,width,height,region FROM world.interiors WHERE world_id=%s ORDER BY 3,2""", (world, world))]
    out['factions'] = [{'id': r[0], 'name': r[1] or r[0]} for r in conn.execute(
        'SELECT id,name FROM live.factions WHERE world_id=%s ORDER BY name,id', (world,))]
    out['towns'] = [{'id': t, 'name': t.replace('_', ' ').replace('-', ' ')} for t in sorted({p['town'] for p in out['places']} - {'', 'unassigned'})]
    society = conn.execute("SELECT payload::jsonb->'society' FROM game.checkpoints WHERE world_id=%s", (world,)).fetchone()
    society = society[0] if society and isinstance(society[0], dict) else {}
    report = society.get('report') if isinstance(society.get('report'), dict) else {}
    board = society.get('board') if isinstance(society.get('board'), dict) else {}
    named = lambda rows: [{'id': r['id'], 'name': r.get('name') or r['id']} for r in rows if isinstance(r, dict) and isinstance(r.get('id'), str)]
    out['markets'] = named(report.get('markets', [])) or [{'id': m, 'name': m} for m in sorted(board.get('cell') or {})]
    out['areas'] = named(report.get('areas', []))
    out['ships'] = named(board.get('ships', []) or [])
    return out


def context(conn, world, npc):
    row = conn.execute(
        'SELECT id,name,description,greeting,personality,backstory,home_area,work_area FROM dm.story_residents WHERE world_id=%s AND id=%s',
        (world, npc),
    ).fetchone()
    rules.require(row, 'Unknown resident.')
    args = (world, npc)
    mantle = conn.execute(
        """SELECT m.id,m.dialect,m.persona FROM live.npc_voice v JOIN live.mantles m
                            ON m.world_id=v.world_id AND m.id=v.mantle_id WHERE v.world_id=%s AND v.npc_id=%s
                            AND v.released_day IS NULL""",
        args,
    ).fetchone()
    # A memory the Mind never summarised is "Conversation record. ...": the players' typed words. Authors get
    # summaries only (the same rule as the hosted chronicle, R1 security).
    memories = conn.execute(
        "SELECT data->>'subject',left(data->>'text',700) FROM game.npc_memories WHERE world_id=%s AND npc=%s "
        "AND coalesce(data->>'text','') !~* '^\\s*conversation\\s+record' ORDER BY updated_at DESC,key LIMIT 20",
        args,
    )
    beliefs = conn.execute(
        "SELECT subject,left(data->>'claim',400) FROM game.beliefs WHERE world_id=%s AND holder=%s ORDER BY updated_at DESC,key LIMIT 20",
        args,
    )
    events = conn.execute(
        """SELECT game_day,kind,actor,target,cell,item FROM game.events WHERE world_id=%s AND (actor=%s OR target=%s)
                            AND kind=ANY(%s) ORDER BY id DESC LIMIT 40""",
        (world, npc, npc, list(rules.EVENTS)),
    )
    return {
        'resident': dict(
            zip(('id', 'name', 'description', 'greeting', 'personality', 'backstory', 'home', 'work'), row)
        ),
        'voice': dict(zip(('mantle', 'dialect', 'persona'), mantle)) if mantle else None,
        'memories': [dict(zip(('subject', 'summary'), r)) for r in memories],
        'beliefs': [dict(zip(('subject', 'claim'), r)) for r in beliefs],
        'history': [dict(zip(('day', 'kind', 'actor', 'target', 'cell', 'item'), r)) for r in events],
    }


def main():
    import world_db

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--world', required=True, help='Registered world key')
    parser.add_argument('--once', action='store_true')
    args = parser.parse_args()
    if not rules.triggers.story_native.interpreter():
        parser.error('Build ratw_server and its ratw_story library, or set RATW_STORY_LIBRARY to that library.')
    with world_db.connect(args.world, 'dm') as conn:
        world = world_id(conn)
        while True:
            tick(conn, world)
            if args.once:
                return
            time.sleep(2)


if __name__ == '__main__':
    main()
