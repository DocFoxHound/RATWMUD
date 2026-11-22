// One campaign (doc 85, 5.3): where it is and how it got there, what it waits for, and its tabs. Read-only in Phase 1,
// except the controls that existed before (stop, retry a refused or expired order, the one-use direction and guest-line
// tickets), moved here from Story Creator's Releases panel.
import {useState} from 'react';
import {atLeast, storyProgram, type Live, type Me, type Target} from '../api';
import {ago, alertText, nameOf, phaseStrip, waitingLines, type Now, type RunDetail, type RunRow, type TimelineItem, type WorldEvent} from '../../lib/campaigns';
import {Timeline, TimelineRow} from './Timeline';
import {StatePanel} from './StatePanel';
import {MissionGrid} from './MissionGrid';
import {WolvesMap} from './WolvesMap';
import {Orders} from './Orders';

export type ViewTab = 'overview' | 'timeline' | 'state' | 'missions' | 'wolves' | 'orders';
const TABS: {id: ViewTab; label: string}[] = [
    {id: 'overview', label: 'Overview'}, {id: 'timeline', label: 'Timeline'}, {id: 'state', label: 'Variables & timers'},
    {id: 'missions', label: 'Missions'}, {id: 'wolves', label: 'Wolves & map'}, {id: 'orders', label: 'Orders'},
];

export function CampaignView({me, target, row, detail, items, events, names, now, live, tab, setTab, earlier, refresh}: {
    me: Me; target: Target; row: RunRow; detail: RunDetail | null; items: TimelineItem[]; events: Record<string, WorldEvent>;
    names: Record<string, string>; now: Now; live: Live | null; tab: ViewTab; setTab: (t: ViewTab) => void; earlier?: () => void;
    refresh: () => Promise<void> | void}) {
    const [ticket, setTicket] = useState(''), [said, setSaid] = useState(''), [busy, setBusy] = useState(false);
    const [mapFocus, setMapFocus] = useState<{cell: string; x: number; y: number} | null>(null);
    const canDirect = atLeast(me, 'dm');
    const state = detail?.state ?? null;
    const strip = detail ? phaseStrip(detail.history, detail.phases, row.phase) : {path: [], next: []};
    const lines = waitingLines(row.waiting, names, now.day, state?.publishedAt, now.at);
    const alerts = row.alerts ?? [];
    const act = async (work: () => Promise<unknown>, done: string) => {
        setBusy(true); setSaid('');
        try { await work(); setSaid(done); await refresh(); } catch (e) { setSaid((e as Error).message); } finally { setBusy(false); }
    };
    const stuck = (row.waiting ?? []).find(w => w.on === 'admin');
    const retryable = row.status === 'paused' && stuck?.on === 'admin' && (!stuck.order || ['refused', 'expired'].includes(stuck.order.status));
    const live_ = ['approved', 'running', 'paused'].includes(row.status);
    const showOnMap = (place: {cell: string; x: number; y: number}) => { setMapFocus(place); setTab('wolves'); };
    return <div className="camp-view">
        <header className="camp-head">
            <div><small>{row.run} · version {row.digest.slice(0, 6)} · approved by {row.approver}{row.approvedAt ? ' ' + ago(row.approvedAt, now.at) : ''}</small>
                <h2>{row.title} <span className={`camp-status ${row.status}`}>{row.status}</span>{row.visit ? <small> visit {row.visit}</small> : null}</h2></div>
            {canDirect && <div className="camp-controls">
                {retryable && <button disabled={busy} onClick={() => void act(() => storyProgram(target, 'retry', {run: row.run}),
                    'Sent again: the story carries on when the game applies it.')}>Retry the {stuck?.on === 'admin' && stuck.order ? stuck.order.status : 'refused'} order</button>}
                {live_ && <button disabled={busy} onClick={() => {
                    if (window.confirm(`Stop "${row.title}" and cancel its future phases? What it already gave the world stays.`))
                        void act(() => storyProgram(target, 'stop', {run: row.run}), 'Stopped.');
                }}>⏹ Stop…</button>}
            </div>}
        </header>
        <div className="camp-strip" aria-label="Phases">
            {strip.path.map((p, i) => <span key={i} className={i === strip.path.length - 1 && row.phase === p ? 'now' : ''}>{i ? '▸ ' : ''}{i === strip.path.length - 1 && row.phase === p ? `[${p}]` : p}</span>)}
            {strip.next.length > 0 && <span className="next">▸ next: {strip.next.join(' or ')}</span>}
            <span className="hint">{now.day !== null ? `Now: day ${now.day.toFixed(1)}` : ''}{row.entered !== undefined && now.day !== null ? ` · in this phase ${(now.day - row.entered).toFixed(1)} days` : ''}</span>
        </div>
        <nav className="camp-tabs" role="tablist">{TABS.map(t => <button key={t.id} role="tab" aria-selected={tab === t.id} className={tab === t.id ? 'on' : ''}
            onClick={() => setTab(t.id)}>{t.label}</button>)}</nav>
        {said && <p className="hint dm-note">{said}</p>}
        <section className="camp-body">
            {!detail ? <p className="hint">Reading this story…</p>
                : tab === 'overview' ? <div className="camp-overview">
                    <h3>Needs you</h3>
                    {alerts.length ? <ul className="camp-alerts">{alerts.map(a => <li key={a.kind} className={a.needsYou ? 'needs' : ''}>{alertText(a, names)}</li>)}</ul>
                        : <p className="hint">(nothing)</p>}
                    <h3>Waiting for</h3>
                    {lines.length ? <ul className="camp-waiting">{lines.map((l, i) => <li key={i} className={l.tone}><b>{l.who}</b> {l.text}</li>)}</ul>
                        : <p className="hint">{row.status === 'complete' ? 'It has ended.' : row.status === 'stopped' ? 'It was stopped.' : '(nothing)'}</p>}
                    {canDirect && row.status === 'running' && (detail.signals.length > 0 || detail.guestVoices.length > 0) && <>
                        <h3>Directions</h3>
                        <p className="hint">A one-use ticket, good for an hour, redeemed through the author bridge (tools/story_author.py).</p>
                        <div className="camp-controls">
                            {detail.signals.map(s => <button key={s} disabled={busy} onClick={() => void act(async () =>
                                setTicket((await storyProgram<{ticket: string}>(target, 'signal-ticket', {run: row.run, signal: s})).ticket), 'Ticket issued.')}>Allow direction “{s}” once</button>)}
                            {detail.guestVoices.map(npc => <button key={npc} disabled={busy} onClick={() => void act(async () =>
                                setTicket((await storyProgram<{ticket: string}>(target, 'voice-ticket', {run: row.run, npc})).ticket), 'Ticket issued.')}>Allow one guest line for {nameOf(names, npc)}</button>)}
                        </div>
                        {ticket && <label className="field"><span>Direction ticket</span><input readOnly value={ticket} onFocus={e => e.target.select()} /></label>}
                    </>}
                    {detail.directions.length > 0 && <><h3>Guest lines given</h3><ul>{detail.directions.map(d =>
                        <li key={d.receipt}>{nameOf(names, d.npc)} · visit {d.visit} · {d.status}</li>)}</ul></>}
                    <h3>Lately</h3>
                    {items.length ? <ol className="camp-timeline">{items.slice(0, 8).map(i => <TimelineRow key={i.key} item={i} names={names} events={events} now={now.at}
                        onMap={showOnMap} />)}</ol> : <p className="hint">Nothing yet.</p>}
                    <details><summary>The approved program (read only)</summary><pre className="camp-json">{JSON.stringify(detail.program, null, 2)}</pre></details>
                </div>
                : tab === 'timeline' ? <Timeline items={items} events={events} names={names} now={now.at} earlier={earlier} onMap={showOnMap}
                    triggers={[...new Set(items.flatMap(i => (i.type === 'trigger' ? [i.trigger] : [])))]} cohort={state?.cohort ?? []} />
                : tab === 'state' ? <StatePanel detail={detail} names={names} now={now} items={items} />
                : tab === 'missions' ? <MissionGrid detail={detail} names={names} live={live} />
                : tab === 'wolves' ? <WolvesMap target={target} detail={detail} names={names} live={live} items={items} focus={mapFocus} />
                : <Orders detail={detail} row={row} names={names} now={now.at} canDirect={canDirect} busy={busy}
                    retry={retryable ? () => void act(() => storyProgram(target, 'retry', {run: row.run}), 'Sent again.') : undefined} />}
        </section>
    </div>;
}
