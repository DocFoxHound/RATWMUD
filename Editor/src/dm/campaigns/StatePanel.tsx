// Variables & timers (doc 85, 5.3): the story's counters (shared, and each wolf's), its timers, the exits counting
// wolves, and which once-only triggers have fired. Read-only in Phase 1; the edits come in Phase 2.
import {dueText, nameOf, type Now, type RunDetail, type TimelineItem} from '../../lib/campaigns';

export function StatePanel({detail, names, now, items}: {detail: RunDetail; names: Record<string, string>; now: Now; items: TimelineItem[]}) {
    const state = detail.state;
    if (!state) return <p className="hint">{detail.status === 'approved' ? 'Not published yet: it has no state.' : 'No state.'}</p>;
    const story = state.counters?.story ?? {};
    const player = state.counters?.player ?? {};
    const keys = [...new Set(Object.values(player).flatMap(Object.keys))].sort();
    // When each counter last changed: the newest firing whose actions set or add (the history keeps no values, Phase 2 adds them).
    const lastSet = items.find(i => i.type === 'trigger' && i.actions.some(a => a === 'set' || a === 'add'));
    const timers = Object.entries(state.timers ?? {}).sort((a, b) => (a[1].clock ?? 'game').localeCompare(b[1].clock ?? 'game') || a[1].at - b[1].at);
    const exits = detail.listening.filter(l => l.source === 'exit' && (l.players !== undefined || l.percent !== undefined));
    const fired = [...Object.entries(state.fired ?? {}).map(([k, v]) => ({k, v, scope: 'this visit'})),
        ...Object.entries(state.campaignFired ?? {}).map(([k, v]) => ({k, v, scope: 'this run'}))];
    return <div className="camp-state">
        <h3>Variables</h3>
        {Object.keys(story).length === 0 && keys.length === 0 ? <p className="hint">No counter has been set yet (every counter reads 0 until one is).</p>
            : <table className="dm-table"><thead><tr><th>Variable</th><th>Scope</th><th>Value</th></tr></thead><tbody>
                {Object.entries(story).sort().map(([k, v]) => <tr key={'s' + k}><td>{k}</td><td>story</td><td className="num">{v}</td></tr>)}
                {keys.map(k => {
                    const by = Object.entries(player).filter(([, c]) => k in c).sort((a, b) => b[1][k] - a[1][k]);
                    const rest = state.cohort.length - by.length;
                    return <tr key={'p' + k}><td>{k}</td><td>each wolf</td><td>{by.slice(0, 6).map(([id, c]) => `${nameOf(names, id)} ${c[k]}`).join(' · ')}
                        {by.length > 6 ? ` · ${by.length - 6} more` : ''}{rest > 0 ? ` · (${rest} at 0)` : ''}</td></tr>;
                })}</tbody></table>}
        {lastSet && lastSet.type === 'trigger' && <p className="hint">Last changed by {lastSet.trigger}{lastSet.actor ? ` (${nameOf(names, lastSet.actor)})` : ''}, day {lastSet.day.toFixed(1)}.</p>}
        <h3>Timers</h3>
        {timers.length === 0 ? <p className="hint">No timer is running.</p>
            : <table className="dm-table"><thead><tr><th>Timer</th><th>Owner</th><th>Due</th><th>Repeats</th><th>Lives</th></tr></thead><tbody>
                {timers.map(([key, t]) => <tr key={key}><td>{t.signal ?? key}</td><td>{key.includes('/') ? nameOf(names, t.actor) : 'story'}</td>
                    <td>{dueText(t.at, t.clock, now.day, state.publishedAt, now.at)}</td><td>{t.interval ? `every ${t.interval} ${t.clock === 'real' ? 'real ' : ''}days` : '-'}</td>
                    <td>{t.scope === 'campaign' ? 'the run' : 'this visit'}</td></tr>)}</tbody></table>}
        {exits.length > 0 && <><h3>Exits counting wolves</h3><ul>{exits.map(e => {
            const votes = state.votes[String(e.index)] ?? [];
            const needed = e.players ?? Math.max(1, Math.ceil(state.cohort.length * (e.percent ?? 100) / 100));
            return <li key={e.index}>to {e.to} when {e.on}: {votes.length} of {needed} ({votes.map(v => nameOf(names, v)).join(', ') || 'nobody yet'})</li>;
        })}</ul></>}
        {fired.length > 0 && <><h3>Fired once already</h3><ul>{fired.map(f => <li key={f.scope + f.k}>{f.k} ({f.scope}): {f.v.map(w => (w === '*' ? 'for everyone' : nameOf(names, w))).join(', ')}</li>)}</ul></>}
        <p className="hint">The audience: {state.cohort.length} wolves, fixed when it was published.</p>
    </div>;
}
