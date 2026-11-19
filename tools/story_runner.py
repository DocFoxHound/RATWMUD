"""World-scoped approval, delivery and deterministic event consumption for DM story programs."""

import argparse
import hashlib
import json
import secrets
import time
import uuid
from pathlib import Path

import story_library
import story_program as rules

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
        if key in tables
    }
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
        rules.require(previous and previous[0] == 'refused', 'Only an explicitly refused game action can be retried.')
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


def status(conn, world, run=None, before_visit=None, before_trigger=None):
    if not run:
        rows = conn.execute(
            """SELECT id,program->>'title',status,digest,approved_by,state->>'phase',problem
                               FROM dm.story_releases WHERE world_id=%s ORDER BY approved_at DESC LIMIT 100""",
            (world,),
        )
        return {
            'runs': [dict(zip(('run', 'title', 'status', 'digest', 'approver', 'phase', 'problem'), r)) for r in rows]
        }
    row = conn.execute(
        'SELECT status,state,problem,receipts,program FROM dm.story_releases WHERE world_id=%s AND id=%s',
        (world, run),
    ).fetchone()
    rules.require(row, 'Unknown run.')
    st, state, problem, ids, program = row
    mission_program = any(p.get('missions') for p in rules.phases_of(program))
    if state:
        ids = [*ids, *(state[k] for k in ('missionReceipt', 'triggerReceipt') if k in state)]
    projection = ',missions' if mission_program else ''
    for boundary in (before_visit, before_trigger):
        rules.require(boundary is None or rules.integer(boundary, 1, 2**63 - 1), 'Invalid history cursor.')
    visit_clause = ' AND visit < %s' if before_visit is not None else ''
    history = conn.execute(
        f'SELECT visit,phase,game_day,cause,receipts{projection} FROM dm.story_transitions WHERE run=%s{visit_clause} ORDER BY visit DESC LIMIT 101',
        (run, before_visit) if visit_clause else (run,),
    ).fetchall()
    trace = []
    if any(ph.get('triggers') for ph in rules.phases_of(program)):
        trace_clause = ' AND id < %s' if before_trigger is not None else ''
        trace = conn.execute(
            f'SELECT id,visit,entry FROM dm.story_trigger_history WHERE run=%s{trace_clause} ORDER BY id DESC LIMIT 51',
            (run, before_trigger) if trace_clause else (run,),
        ).fetchall()
    return {
        'run': run,
        'status': st,
        'state': state,
        'problem': problem,
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
        'trace': [{'id': row[0], 'visit': row[1], **row[2]} for row in trace[:50]],
        'beforeTrigger': trace[49][0] if len(trace) > 50 else None,
        'actions': [
            dict(zip(('id', 'kind', 'status', 'result'), r))
            for r in conn.execute('SELECT id,kind,status,result FROM dm.actions WHERE id=ANY(%s) ORDER BY id', (ids,))
        ],
    }


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
    memories = conn.execute(
        "SELECT data->>'subject',left(data->>'text',700) FROM game.npc_memories WHERE world_id=%s AND npc=%s ORDER BY updated_at DESC,key LIMIT 20",
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
