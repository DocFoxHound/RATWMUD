// Variables & timers (doc 85, 5.3; Phase 5: typed variables of the story, the world, each wolf and each party, waits and the
// story's ending): the story's variables, its timers, the exits counting
// wolves, and which once-only triggers have fired; with Phase 2's edits (set a counter; fire, move or cancel a timer;
// start one the program starts), each a command that asks first.
import {useState} from 'react';
import {changeText, dueText, memoryOwnerText, nameOf, valueText, type ControlVariable, type Now, type RunDetail, type TimelineItem, type Value} from '../../lib/campaigns';
import type {SteerFn} from './Steer';

export function StatePanel({detail, names, now, items, steer, busy = false}: {detail: RunDetail; names: Record<string, string>; now: Now; items: TimelineItem[];
    steer?: SteerFn; busy?: boolean}) {
    const [editing, setEditing] = useState<string | null>(null), [value, setValue] = useState(''), [wolf, setWolf] = useState('');
    const [shift, setShift] = useState<Record<string, string>>({}), [start, setStart] = useState(''), [startDays, setStartDays] = useState('');
    const state = detail.state;
    if (!state) return <p className="hint">{detail.status === 'approved' ? 'Not published yet: it has no state.' : 'No state.'}</p>;
    const stores = state.counters ?? {};
    const named = detail.controls?.counters ?? [];
    // Every variable: those the program declares (or, version 1, reads and writes), then any the state holds that it does not name.
    const rows: ControlVariable[] = [...named];
    for (const scope of ['story', 'world'] as const) for (const k of Object.keys(stores[scope] ?? {}))
        if (!rows.some(r => r.key === k && r.scope === scope)) rows.push({key: k, scope, type: 'number', initial: 0});
    for (const scope of ['player', 'party'] as const) for (const k of new Set(Object.values(stores[scope] ?? {}).flatMap(Object.keys)))
        if (!rows.some(r => r.key === k && r.scope === scope)) rows.push({key: k, scope, type: 'number', initial: 0});
    // When each variable last changed: the newest firing or command that changed it (the history keeps the values since Phase 2).
    const changedBy = (key: string) => {
        for (const i of items) {
            if (i.type === 'trigger' && i.changes?.some(c => c.what === 'counter' && c.key === key))
                return `day ${i.day.toFixed(1)} by ${i.trigger}${i.actor ? ` (${nameOf(names, i.actor)})` : ''}: ${i.changes.filter(c => c.key === key).map(c => changeText(c, names)).join(', ')}`;
            if (i.type === 'command' && i.command === 'set' && i.payload.key === key && i.status === 'done') return `by ${i.by.split('#')[0]}: "${i.reason}"`;
        }
        return '';
    };
    const first = (r: ControlVariable): Value => r.initial ?? (r.type === 'switch' ? false : r.type === 'timer' ? null : r.type && r.type !== 'number' ? (['numbers', 'texts', 'wolves', 'residents'].includes(r.type) ? [] : '') : 0);
    const personal = (r: ControlVariable) => r.scope === 'player' || r.scope === 'party';
    const ownerOf = (r: ControlVariable, w: string) => r.scope === 'party' ? state.parties?.[w] ?? w : w;
    const valueOf = (r: ControlVariable, w = ''): Value => {
        const store = personal(r) ? (stores[r.scope] as Record<string, Record<string, Value>> | undefined)?.[ownerOf(r, w)] : (stores[r.scope] as Record<string, Value> | undefined);
        return store && r.key in store ? store[r.key] : first(r);
    };
    // A value typed in, as its type wants it; undefined when it is not one.
    const parsed = (r: ControlVariable, text: string): Value | undefined => {
        const type = r.type ?? 'number';
        if (type === 'number') return text !== '' && Number.isFinite(Number(text)) && Number.isInteger(Number(text)) ? Number(text) : undefined;
        if (type === 'switch') return text === 'on' ? true : text === 'off' ? false : undefined;
        if (['numbers', 'texts', 'wolves', 'residents'].includes(type))
            return [...new Set(text.split(',').map(x => x.trim()).filter(Boolean))].map(x => type === 'numbers' ? Number(x) : x);
        return text.length <= 200 ? text : undefined;
    };
    const setter = (r: ControlVariable) => steer && r.type !== 'timer' && (editing === r.scope + r.key
        ? <span className="camp-group">
            {personal(r) && <select value={wolf} onChange={e => setWolf(e.target.value)} aria-label="Wolf"><option value="">choose…</option>
                {state.cohort.map(id => <option key={id} value={id}>{nameOf(names, id)}</option>)}</select>}
            {r.type === 'switch' ? <select value={value} onChange={e => setValue(e.target.value)} aria-label="Value"><option value="">…</option><option value="on">on</option><option value="off">off</option></select>
                : <input type={(r.type ?? 'number') === 'number' ? 'number' : 'text'} value={value} onChange={e => setValue(e.target.value)} style={{width: (r.type ?? 'number') === 'number' ? 80 : 160}}
                    aria-label="Value" placeholder={['numbers', 'texts', 'wolves', 'residents'].includes(r.type ?? '') ? 'a, b, c' : ''} />}
            <button disabled={busy || parsed(r, value) === undefined || (personal(r) && !wolf)} onClick={() => {
                const was = valueOf(r, wolf), next = parsed(r, value)!;
                steer({kind: 'set', payload: {key: r.key, scope: r.scope, value: next, ...(personal(r) ? {player: wolf} : {})}, title: `Set ${r.key}`,
                    does: [`${r.key}${personal(r) ? ' for ' + nameOf(names, wolf) + (r.scope === 'party' ? "'s party" : '') : ''}: ${valueText(was, r.type, names)} → ${valueText(next, r.type, names)}.`,
                        r.scope === 'world' ? 'It is the world\'s: every story declaring it sees the new value.' : 'Triggers that read it see the new value from the next event on.']});
                setEditing(null);
            }}>Set…</button><button onClick={() => setEditing(null)}>×</button></span>
        : <button className="link-button" disabled={busy} onClick={() => {
            const now = personal(r) ? undefined : valueOf(r);
            setEditing(r.scope + r.key); setWolf('');
            setValue(now === undefined || now === null ? '' : typeof now === 'boolean' ? (now ? 'on' : 'off') : Array.isArray(now) ? now.join(', ') : String(now));
        }}>✎</button>);
    const SCOPE: Record<string, string> = {story: 'story', world: 'the world', player: 'each wolf', party: 'each party'};
    const timers = Object.entries(state.timers ?? {}).sort((a, b) => (a[1].clock ?? 'game').localeCompare(b[1].clock ?? 'game') || a[1].at - b[1].at);
    const exits = detail.listening.filter(l => l.source === 'exit' && (l.players !== undefined || l.percent !== undefined));
    const fired = [...Object.entries(state.fired ?? {}).map(([k, v]) => ({k, v, scope: 'this visit'})),
        ...Object.entries(state.campaignFired ?? {}).map(([k, v]) => ({k, v, scope: 'this run'}))];
    return <div className="camp-state">
        {state.outcome && <p className="camp-outcome"><b>Ended: {state.outcome.outcome}</b> on day {state.outcome.day.toFixed(1)}: “{state.outcome.text}”</p>}
        {state.outcomes && Object.keys(state.outcomes).length > 0 && <p className="hint">Their own endings: {Object.entries(state.outcomes).map(([w, o]) =>
            `${nameOf(names, w)} ${o.outcome} (day ${o.day.toFixed(1)})`).join(' · ')}</p>}
        <h3>Variables</h3>
        {rows.length === 0 ? <p className="hint">No variable has been set yet (a counter reads 0 until one is).</p>
            : <table className="dm-table"><thead><tr><th>Variable</th><th>Type</th><th>Whose</th><th>Value</th>{steer && <th></th>}<th>Changed</th></tr></thead><tbody>
                {rows.map(r => {
                    let shownValue: string;
                    if (personal(r)) {
                        const owners = Object.entries((stores[r.scope] ?? {}) as Record<string, Record<string, Value>>).filter(([, c]) => r.key in c);
                        const rest = r.scope === 'player' ? state.cohort.length - owners.length : 0;
                        shownValue = owners.slice(0, 6).map(([id, c]) => `${nameOf(names, id)} ${valueText(c[r.key], r.type, names)}`).join(' · ')
                            + (owners.length > 6 ? ` · ${owners.length - 6} more` : '') + (rest > 0 ? `${owners.length ? ' · ' : ''}(${rest} at ${valueText(first(r), r.type, names)})` : '');
                    } else shownValue = valueText(valueOf(r), r.type, names);
                    return <tr key={r.scope + r.key} title={r.note}><td>{r.key}{r.show && <small className="hint"> “{r.show}”</small>}</td><td>{r.type ?? 'number'}</td>
                        <td>{SCOPE[r.scope] ?? r.scope}</td><td className={r.type === 'number' && !personal(r) ? 'num' : ''}>{shownValue || '—'}</td>
                        {steer && <td>{setter(r)}</td>}<td className="hint">{changedBy(r.key)}</td></tr>;
                })}</tbody></table>}
        {(state.waiting ?? []).length > 0 && <><h3>Waiting</h3><ul>{state.waiting!.map((w, i) => <li key={i}>{w.trigger}{w.actor ? ` (${nameOf(names, w.actor)})` : ''}:
            the rest of its actions {dueText(w.at, w.clock as 'game' | 'real', now.day, state.publishedAt, now.at)}{w.scope === 'campaign' ? ', whatever the phase' : ', if the phase is still open'}</li>)}</ul></>}
        <h3>Timers</h3>
        {timers.length === 0 ? <p className="hint">No timer is running.</p>
            : <table className="dm-table"><thead><tr><th>Timer</th><th>Owner</th><th>Due</th><th>Repeats</th><th>Lives</th>{steer && <th></th>}</tr></thead><tbody>
                {timers.map(([key, t]) => <tr key={key}><td>{t.signal ?? key}</td><td>{key.includes('/') ? nameOf(names, t.actor) : 'story'}</td>
                    <td>{dueText(t.at, t.clock, now.day, state.publishedAt, now.at)}</td><td>{t.interval ? `every ${t.interval} ${t.clock === 'real' ? 'real ' : ''}days` : '-'}</td>
                    <td>{t.scope === 'campaign' ? 'the run' : 'this visit'}</td>
                    {steer && <td className="camp-group">
                        <button disabled={busy} onClick={() => steer({kind: 'timer', payload: {do: 'fire', name: key}, title: `Fire timer ${key} now`,
                            does: [`Sends its signal "${t.signal ?? key}" now${t.actor && key.includes('/') ? ' for ' + nameOf(names, t.actor) : ''}: whatever listens fires.`,
                                t.interval ? `It goes round again, due in ${t.interval} days.` : 'It is then gone.']})}>Fire now</button>
                        <input type="number" step="0.5" placeholder="± days" value={shift[key] ?? ''} style={{width: 72}} aria-label="Days"
                            onChange={e => setShift(x => ({...x, [key]: e.target.value}))} />
                        <button disabled={busy || !Number(shift[key])} onClick={() => steer({kind: 'timer', payload: {do: 'shift', name: key, days: Number(shift[key])},
                            title: `Move timer ${key}`, does: [`Due ${Number(shift[key]) > 0 ? 'later' : 'sooner'} by ${Math.abs(Number(shift[key]))} days: ${dueText(t.at + Number(shift[key]), t.clock, now.day, state.publishedAt, now.at)}.`]})}>± time</button>
                        <button disabled={busy} onClick={() => steer({kind: 'timer', payload: {do: 'cancel', name: key}, title: `Cancel timer ${key}`,
                            does: ['It never fires (unless a trigger starts it again).']})}>Cancel</button></td>}</tr>)}</tbody></table>}
        {steer && Object.keys(detail.controls?.schedules ?? {}).length > 0 && <p className="camp-group">
            <select value={start} onChange={e => setStart(e.target.value)} aria-label="Timer to start"><option value="">start a timer…</option>
                {Object.entries(detail.controls!.schedules).map(([sig, sch]) => <option key={sig} value={sig}>{sig} (every {sch.days} days{sch.repeat ? ', repeating' : ''})</option>)}</select>
            <input type="number" step="0.5" placeholder="days" value={startDays} style={{width: 72}} onChange={e => setStartDays(e.target.value)} aria-label="In days" />
            <button disabled={busy || !start || (detail.controls!.schedules[start]?.owner === 'player')} onClick={() => steer({kind: 'timer',
                payload: {do: 'start', signal: start, ...(startDays ? {days: Number(startDays)} : {})}, title: `Start timer ${start}`,
                does: [`As the program starts it, due in ${startDays || detail.controls!.schedules[start].days} days.`]})}>+ Start…</button></p>}
        {exits.length > 0 && <><h3>Exits counting wolves</h3><ul>{exits.map(e => {
            const votes = state.votes[String(e.index)] ?? [];
            const needed = e.players ?? Math.max(1, Math.ceil(state.cohort.length * (e.percent ?? 100) / 100));
            return <li key={e.index}>to {e.to} when {e.on}: {votes.length} of {needed} ({votes.map(v => nameOf(names, v)).join(', ') || 'nobody yet'})</li>;
        })}</ul></>}
        {fired.length > 0 && <><h3>Fired once already</h3><ul>{fired.map(f => <li key={f.scope + f.k}>{f.k} ({f.scope}): {f.v.map(w => (w === '*' ? 'for everyone' : nameOf(names, w))).join(', ')}</li>)}</ul></>}
        {(detail.memory?.length ?? 0) > 0 && <><h3>What residents remember</h3><table className="dm-table"><thead><tr><th>Who</th><th>What</th><th>Days left</th><th>After the story</th></tr></thead>
            <tbody>{detail.memory!.map(m => <tr key={m.owner + '#' + m.key}><td>{memoryOwnerText(m.owner, names)}</td><td>{m.key} = {m.value}</td>
                <td>{m.daysLeft == null ? 'for good' : m.daysLeft}</td><td>{m.keep ? 'kept' : 'taken back'}</td></tr>)}</tbody></table>
            <p className="hint">As the game last saved it: residents' lines gated on world.mem, self.mem and pair.mem play while it lasts.</p></>}
        <p className="hint">The audience: {state.cohort.length} wolves, fixed when it was published.</p>
    </div>;
}
