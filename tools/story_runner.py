"""World-scoped approval, delivery and deterministic event consumption for DM story programs."""

import argparse
import datetime
import hashlib
import json
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
        if value == '$actor':
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


def approve(conn, world, who, program, digest):
    report = rules.review(program)
    rules.require(
        report['passed'] and digest == report['digest'], 'Rehearse this exact version successfully before approval.'
    )
    rules.require(not report['uncovered'], 'Rehearse the remaining phases: ' + ', '.join(report['uncovered']))
    references(conn, world, program)
    run, ticket = 'world-' + uuid.uuid4().hex, secrets.token_urlsafe(32)
    conn.execute(
        """INSERT INTO dm.story_releases(id,world_id,digest,program,approved_by,ticket_hash,status)
                    VALUES (%s,%s,%s,%s::jsonb,%s,%s,'approved')""",
        (run, world, digest, json.dumps(program), who, ticket_hash(ticket)),
    )
    return {'run': run, 'ticket': ticket, 'digest': digest}


def queue(conn, who, kind, target, payload):
    receipt = conn.execute(
        """INSERT INTO dm.actions(kind,target_id,payload,requested_by)
                           VALUES (%s,%s,%s::jsonb,%s) RETURNING id""",
        (kind, target, json.dumps(payload), who),
    ).fetchone()[0]
    conn.execute("SELECT pg_notify('ratw_dm',%s)", (json.dumps({'action': receipt}),))
    return receipt


def enter(conn, run, program, who, state, cursor, cause):
    phase = rules.phase_of(program, state)
    state.pop('arrivalEffects', None)
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
            and not phase['effects']
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
                            AND status IN ('running','paused')""",
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
            if e['kind'] == 'scene.play' and e['player'] not in ('$actor', 'world'):
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
        {**rules.begin(program, cohort, day), 'publishedAt': published_at},
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
    if status != 'applied':
        if status != 'queued':
            conn.execute(
                "UPDATE dm.story_releases SET status='paused',problem=%s WHERE id=%s",
                (f'Action {receipts[-1]} {status}: {result}', run),
            )
        return False
    effects = [*state.get('arrivalEffects', []), *(e for e in phase['effects'] if e['kind'] != 'npc.line')]
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


def tick_run(conn, world, run, clock):
    row = conn.execute(
        """SELECT program,state,cursor,approved_by,receipts FROM dm.story_releases
                          WHERE world_id=%s AND id=%s AND status='running' FOR UPDATE SKIP LOCKED""",
        (world, run),
    ).fetchone()
    if not row:
        return
    p, state, cursor, who, receipts = row
    phase = rules.phase_of(p, state)
    receipt_key = next((key for key in ('missionReceipt', 'triggerReceipt') if key in state), None)
    if receipt_key:
        pending = state[receipt_key]
        delivery, message = conn.execute('SELECT status,result FROM dm.actions WHERE id=%s', (pending,)).fetchone()
        if delivery != 'applied':
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
    if not (phase['exits'] or phase.get('triggers')):
        conn.execute("UPDATE dm.story_releases SET status='complete' WHERE id=%s", (run,))
        return
    # The clock read precedes the log read: an older query can never advance past a newly committed backlog.
    has_clock = bool(state.get('timers')) or any(
        e['on'] == 'clock' for e in [*phase['exits'], *phase.get('triggers', [])]
    )
    day = clock() if has_clock else None
    prior_clock = json.dumps([state.get('fired'), state.get('timers')]) if has_clock else None
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
        moved = rules.advance(p, state, fact, day, trace, elapsed)
        if trace:
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
        if state.get('triggerEffects'):
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
        if state.get('triggerEffects'):
            break
    if (
        not state.get('triggerEffects')
        and day is not None
        and len(rows) < 128
        and dispatch({'kind': 'clock'}, day, wall_elapsed)
    ):
        enter(conn, run, p, who, state, cursor, 'clock')
    elif rows or (has_clock and prior_clock != json.dumps([state.get('fired'), state.get('timers')])):
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
    runs = conn.execute(
        "SELECT id,cursor,state->>'visits' FROM dm.story_releases WHERE world_id=%s AND status='running' ORDER BY id",
        (world,),
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

LIVE_STATUSES = ('approved', 'running', 'paused')
# The first values of doc 85, 5.6 (adjustable per world later, Phase 3).
ALERTS = {'gameSeconds': 60, 'behindEvents': 2000, 'behindSeconds': 60, 'quietHours': 24}
RECEIPT_KEYS = ('missionReceipt', 'triggerReceipt')
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
    effects = [*state.get('arrivalEffects', []), *(e for e in phase['effects'] if e['kind'] != 'npc.line')]
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
    if status not in ('running', 'paused') or not state:
        return []
    out = []
    pending = [state[k] for k in RECEIPT_KEYS if k in state]
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
    listening = _listening(phase)
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


def _alerts(conn, world, status, problem, waiting, behind, last, cohort, now):
    """Doc 85, 5.6: the alerts that need nothing new (Phase 1): paused, the game not answering, falling behind, quiet."""
    out = []
    if status == 'paused':
        out.append({'kind': 'paused', 'needsYou': True, 'problem': problem})
    if status not in ('running', 'paused'):
        return out
    game = next((w for w in waiting if w['on'] == 'game'), None)
    if game and game['order']['age'] > ALERTS['gameSeconds']:
        out.append({'kind': 'game', 'needsYou': False, 'order': game['order']})
    elif status == 'running' and (behind['events'] > ALERTS['behindEvents'] or behind['seconds'] > ALERTS['behindSeconds']):
        out.append({'kind': 'behind', 'needsYou': False, **behind})
    if status == 'running' and cohort:
        moments = [m for m in (last.get('trigger', {}).get('at'), last.get('phase', {}).get('at'), last.get('order')) if m]
        latest = max(moments) if moments else None
        if latest and (now - latest).total_seconds() > ALERTS['quietHours'] * 3600:
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
    return out


def _run_receipts(conn, run, receipts, state):
    """Every order a run has sent: each visit's receipts, the current visit's, and one waiting for its answer."""
    by_visit = {}
    for visit, ids in conn.execute('SELECT visit,receipts FROM dm.story_transitions WHERE run=%s', (run,)):
        for i in ids:
            by_visit.setdefault(i, visit)
    extra = [*(receipts or []), *((state[k] for k in RECEIPT_KEYS if k in state) if state else [])]
    for i in extra:
        by_visit.setdefault(i, state.get('visits') if state else None)
    return by_visit


def _row_view(conn, world, row, now, last):
    run, title, st, digest, who, approved_at, state, problem, cursor, receipts, program = row
    view = {'run': run, 'title': title, 'status': st, 'digest': digest, 'approver': who, 'approvedAt': _iso(approved_at),
            'phase': state.get('phase') if state else None, 'problem': problem}
    if st == 'approved':
        view['waiting'] = [{'on': 'publish'}]
    if not state or program is None:
        if st in ('complete', 'stopped') and state:
            view.update(visit=state.get('visits'), wolves=len(state.get('cohort', [])))
        return view
    phase = rules.phase_of(program, state)
    pending = [state[k] for k in RECEIPT_KEYS if k in state]
    orders = _orders(conn, [*pending, *receipts[-1:]])
    waiting = _waiting(program, state, st, problem, receipts, orders)
    behind = _behind(conn, world, cursor, now['newest'], now['at']) if st == 'running' else {'events': 0, 'seconds': 0}
    moment = [m for m in (last.get('trigger'), last.get('phase')) if m and m.get('at')]
    view.update(
        visit=state['visits'], entered=state['entered'], cohort=state.get('cohort', []), wolves=len(state.get('cohort', [])),
        missions=_summary_missions(phase, state), waiting=waiting, behind=behind,
        # (A firing and the visit it opened share a moment: the visit is the newer.)
        last={**max(moment, key=lambda m: (m['at'], m['type'] == 'phase')), 'at': _iso(max(m['at'] for m in moment))}
        if moment else None,
        alerts=_alerts(conn, world, st, problem, waiting, behind, last, state.get('cohort', []), now['at']),
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
    runs = []
    for r in rows:
        view = _row_view(conn, world, r, now, last.get(r[0], {}))
        if r[0] in ended:
            view.update(visit=ended[r[0]][0], wolves=ended[r[0]][1])
        runs.append(view)
    ids = {i for v in runs for i in [*v.get('cohort', []), (v.get('last') or {}).get('actor', '')]}
    return {'runs': runs, 'now': {'day': now['day'], 'newest': now['newest'], 'at': _iso(now['at'])},
            'names': names(conn, world, ids), 'thresholds': ALERTS}


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
    view = _row_view(conn, world, row, now, _last_activity(conn, [run]).get(run, {}) if st in LIVE_STATUSES else {})
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
            a['phase'] for t in [*rules.effective_phase(program, p).get('triggers', [])] for a in t['actions'] if a['do'] == 'enter'}),
            'missions': len(p.get('missions', []))} for p in program['phases']],
        'start': program['start'],
        'listening': _listening(rules.phase_of(program, state)) if state else [],
        'names': names(conn, world, who),
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
    kinds = set(kinds or ('phase', 'trigger', 'order'))
    rules.require(kinds <= {'phase', 'trigger', 'order'}, 'Unknown timeline filter.')
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
    # Newest first. One tick's work shares a moment: a firing (still in the visit it fired in) comes before the visit it
    # opened, and in one visit the visit opened, then its firings, then its orders.
    rank = {'phase': 0, 'trigger': 1, 'order': 2}
    epoch = datetime.datetime.min.replace(tzinfo=datetime.timezone.utc)
    items.sort(key=lambda i: (i['at'] or epoch, i.get('visit') or 0, rank[i['type']], i['seq']), reverse=True)
    more = False
    if paging and len(items) > limit:
        more, items = True, items[:limit]
    cursor = dict(before)
    for name, key in (('phase', 'visit'), ('trigger', 'trigger'), ('order', 'order')):
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
              for name, k in (('phase', 'visit'), ('trigger', 'trigger'), ('order', 'order'))}
    for i in items:
        i['at'] = _iso(i['at'])
        del i['seq']
    who = {*(i.get('actor', '') for i in items), *(i.get('target', '') for i in items),
           *(e[k] for e in events.values() for k in ('actor', 'target', 'cell')), *(i.get('cell', '') for i in items)}
    return {'run': run, 'items': items, 'events': events, 'names': names(conn, world, who),
            'before': cursor if more else None, 'since': {**newest, 'at': _iso(now['at'])}, 'now': _iso(now['at'])}


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
