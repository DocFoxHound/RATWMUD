// Variables & timers (doc 85, 5.3): the story's counters (shared, and each wolf's), its timers, the exits counting
// wolves, and which once-only triggers have fired; with Phase 2's edits (set a counter; fire, move or cancel a timer;
// start one the program starts), each a command that asks first.
import {useState} from 'react';
import {changeText, dueText, nameOf, type Now, type RunDetail, type TimelineItem} from '../../lib/campaigns';
import type {SteerFn} from './Steer';

export function StatePanel({detail, names, now, items, steer, busy = false}: {detail: RunDetail; names: Record<string, string>; now: Now; items: TimelineItem[];
    steer?: SteerFn; busy?: boolean}) {
    const [editing, setEditing] = useState<string | null>(null), [value, setValue] = useState(''), [wolf, setWolf] = useState('');
    const [shift, setShift] = useState<Record<string, string>>({}), [start, setStart] = useState(''), [startDays, setStartDays] = useState('');
    const state = detail.state;
    if (!state) return <p className="hint">{detail.status === 'approved' ? 'Not published yet: it has no state.' : 'No state.'}</p>;
    const story = state.counters?.story ?? {};
    const player = state.counters?.player ?? {};
    const keys = [...new Set(Object.values(player).flatMap(Object.keys))].sort();
    // When each counter last changed: the newest firing or command that changed it (the history keeps the values since Phase 2).
    const changedBy = (key: string) => {
        for (const i of items) {
            if (i.type === 'trigger' && i.changes?.some(c => c.what === 'counter' && c.key === key))
                return `day ${i.day.toFixed(1)} by ${i.trigger}${i.actor ? ` (${nameOf(names, i.actor)})` : ''}: ${i.changes.filter(c => c.key === key).map(c => changeText(c, names)).join(', ')}`;
            if (i.type === 'command' && i.command === 'set' && i.payload.key === key && i.status === 'done') return `by ${i.by.split('#')[0]}: "${i.reason}"`;
        }
        return '';
    };
    const named = detail.controls?.counters ?? [];
    const storyKeys = [...new Set([...Object.keys(story), ...named.filter(c => c.scope === 'story').map(c => c.key)])].sort();
    const setter = (key: string, scope: 'story' | 'player') => steer && (editing === scope + key
        ? <span className="camp-group">
            {scope === 'player' && <select value={wolf} onChange={e => setWolf(e.target.value)} aria-label="Wolf"><option value="">choose…</option>
                {state.cohort.map(id => <option key={id} value={id}>{nameOf(names, id)}</option>)}</select>}
            <input type="number" value={value} onChange={e => setValue(e.target.value)} style={{width: 80}} aria-label="Value" />
            <button disabled={busy || value === '' || !Number.isInteger(Number(value)) || (scope === 'player' && !wolf)} onClick={() => {
                const was = scope === 'story' ? story[key] ?? 0 : player[wolf]?.[key] ?? 0;
                steer({kind: 'set', payload: {key, scope, value: Number(value), ...(scope === 'player' ? {player: wolf} : {})}, title: `Set ${key}`,
                    does: [`${key}${scope === 'player' ? ' for ' + nameOf(names, wolf) : ''}: ${was} → ${Number(value)}.`, 'Triggers that read it see the new value from the next event on.']});
                setEditing(null);
            }}>Set…</button><button onClick={() => setEditing(null)}>×</button></span>
        : named.some(c => c.key === key && c.scope === scope) && <button className="link-button" disabled={busy} onClick={() => {
            setEditing(scope + key); setValue(String(scope === 'story' ? story[key] ?? 0 : '')); setWolf('');
        }}>✎</button>);
    const timers = Object.entries(state.timers ?? {}).sort((a, b) => (a[1].clock ?? 'game').localeCompare(b[1].clock ?? 'game') || a[1].at - b[1].at);
    const exits = detail.listening.filter(l => l.source === 'exit' && (l.players !== undefined || l.percent !== undefined));
    const fired = [...Object.entries(state.fired ?? {}).map(([k, v]) => ({k, v, scope: 'this visit'})),
        ...Object.entries(state.campaignFired ?? {}).map(([k, v]) => ({k, v, scope: 'this run'}))];
    return <div className="camp-state">
        <h3>Variables</h3>
        {storyKeys.length === 0 && keys.length === 0 && !named.length ? <p className="hint">No counter has been set yet (every counter reads 0 until one is).</p>
            : <table className="dm-table"><thead><tr><th>Variable</th><th>Scope</th><th>Value</th>{steer && <th></th>}<th>Changed</th></tr></thead><tbody>
                {storyKeys.map(k => <tr key={'s' + k}><td>{k}</td><td>story</td><td className="num">{story[k] ?? 0}</td>{steer && <td>{setter(k, 'story')}</td>}
                    <td className="hint">{changedBy(k)}</td></tr>)}
                {[...new Set([...keys, ...named.filter(c => c.scope === 'player').map(c => c.key)])].sort().map(k => {
                    const by = Object.entries(player).filter(([, c]) => k in c).sort((a, b) => b[1][k] - a[1][k]);
                    const rest = state.cohort.length - by.length;
                    return <tr key={'p' + k}><td>{k}</td><td>each wolf</td><td>{by.slice(0, 6).map(([id, c]) => `${nameOf(names, id)} ${c[k]}`).join(' · ')}
                        {by.length > 6 ? ` · ${by.length - 6} more` : ''}{rest > 0 ? ` · (${rest} at 0)` : ''}</td>{steer && <td>{setter(k, 'player')}</td>}
                        <td className="hint">{changedBy(k)}</td></tr>;
                })}</tbody></table>}
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
        <p className="hint">The audience: {state.cohort.length} wolves, fixed when it was published.</p>
    </div>;
}
