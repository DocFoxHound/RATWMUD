// The Campaigns list (doc 85, 5.2): every run of the world, and for each live one whether it is alive, where it is,
// who is in it, when anything last happened and what it is waiting for.
import {useState} from 'react';
import type {Live} from '../api';
import {ago, alertText, filterRuns, hereCount, lastText, listCounts, questsText, waitingOn, type FramePerson, type ListFilter, type Overview} from '../../lib/campaigns';

const FILTERS: {id: ListFilter; label: string}[] = [
    {id: 'all', label: 'All'}, {id: 'running', label: 'Running'}, {id: 'held', label: 'Held'}, {id: 'paused', label: 'Paused'}, {id: 'approved', label: 'Approved'},
    {id: 'ended', label: 'Ended'},
];

export function CampaignList({overview, live, names, open, holdAll}: {overview: Overview | null; live: Live | null; names: Record<string, string>;
    open: (run: string) => void; holdAll?: () => void}) {
    const [filter, setFilter] = useState<ListFilter>('all');
    if (!overview) return <p className="hint camp-pad">Loading the world's stories…</p>;
    const counts = listCounts(overview.runs);
    const people = live?.frame?.people as FramePerson[] | undefined;
    const runs = filterRuns(overview.runs, filter);
    return <div className="camp-list">
        <div className="camp-filters">
            {counts.needsYou > 0 ? <span className="camp-needs">⚠ {counts.needsYou} need{counts.needsYou === 1 ? 's' : ''} you</span> : <span className="hint">Nothing needs you.</span>}
            {FILTERS.map(f => <button key={f.id} className={filter === f.id ? 'on' : ''} onClick={() => setFilter(f.id)}>{f.label} {counts[f.id]}</button>)}
            {holdAll && counts.running > 0 && <button className="camp-hold-all" onClick={holdAll}>⏸ Hold all…</button>}
            {!live?.frame && <span className="hint">Who is in the world: {live ? 'no positions yet (is the game server running?)' : 'connecting…'}</span>}
        </div>
        {overview.runs.length === 0 ? <p className="hint camp-pad">No world stories in this world yet. Write one in Story Creator, rehearse it, approve and publish it: it shows here.</p>
            : <table className="dm-table camp-table"><thead><tr>
                <th>Story</th><th>Status</th><th>Phase</th><th>Wolves</th><th>Missions</th><th>Last happened</th><th>Waiting on</th>
            </tr></thead><tbody>{runs.map(r => {
                const wait = waitingOn(r.waiting, names, overview.now.day, undefined, overview.now.at);
                const alerts = r.alerts ?? [];
                return <tr key={r.run} onClick={() => open(r.run)} className={alerts.some(a => a.needsYou) ? 'needs' : ''}>
                    <td><b>{r.title}</b><small>{r.approver ? `approved by ${r.approver}` : ''}{r.approvedAt ? ` · ${ago(r.approvedAt, overview.now.at)}` : ''}</small></td>
                    <td><span className={`camp-status ${r.status}`}>{r.status}</span>{r.commandsQueued ? <small className="camp-warn">{r.commandsQueued} command{r.commandsQueued === 1 ? '' : 's'} sent</small> : null}
                        {alerts.map(a => <small key={a.kind} className={a.needsYou ? 'error-text' : 'camp-warn'} title={alertText(a, names)}>⚠ {a.kind}</small>)}</td>
                    <td>{r.phase ?? '-'}{r.visit ? <small>visit {r.visit}{r.entered !== undefined && overview.now.day !== null ? ` · ${(overview.now.day - r.entered).toFixed(1)} days in` : ''}</small> : null}</td>
                    <td>{r.wolves ?? '-'}{r.cohort && <small>{hereCount(r.cohort, people)} here now</small>}</td>
                    <td>{questsText(r.missions)}</td>
                    <td>{lastText(r.last, names)}{r.last?.at && <small>{ago(r.last.at, overview.now.at)}{r.last.day != null ? ` · day ${r.last.day.toFixed(1)}` : ''}</small>}</td>
                    <td className={wait ? `camp-wait ${wait.tone}` : ''}>{wait ? <><b>{wait.who}</b><small>{wait.text}</small></> : '-'}</td>
                </tr>;
            })}</tbody></table>}
        {overview.runs.some(r => (r.alerts ?? []).length) && <ul className="camp-alerts">{overview.runs.flatMap(r => (r.alerts ?? []).map(a =>
            <li key={r.run + a.kind} className={a.needsYou ? 'needs' : ''}><button className="link-button" onClick={() => open(r.run)}>{r.title}</button> · {alertText(a, names)}</li>))}</ul>}
    </div>;
}
