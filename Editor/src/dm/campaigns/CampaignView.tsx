// One campaign (doc 85, 5.3): where it is and how it got there, what it waits for, and its tabs; and since Phase 2 its
// steering: every control sends a command to the story runner's inbox (Steer.tsx), asks first, and is audited. Steering
// needs a DM in the user's own studio, and on a hosted world the owner or "may steer campaigns" (5.7). The one-use direction
// and guest-line tickets stay, for the author bridge.
import {useState} from 'react';
import {atLeast, maySteer, storyProgram, type Live, type Me, type Target} from '../api';
import {ago, alertText, commandLine, firingCounts, nameOf, newKey, phaseStrip, steerable, waitingLines, type Explained, type FramePerson, type Now,
    type RunDetail, type RunRow, type TimelineItem, type WorldEvent} from '../../lib/campaigns';
import {AskDialog, fireAsk, SteerBar, TriggersPanel, type Ask} from './Steer';
import {NextByItself, WaitingFor, WhatIfPanel} from './Explain';
import {Audit} from './Audit';
import {Timeline, TimelineRow} from './Timeline';
import {StatePanel} from './StatePanel';
import {MissionGrid} from './MissionGrid';
import {QuestGrid} from './QuestGrid';
import {WolvesMap} from './WolvesMap';
import {Orders} from './Orders';
import {ChapterBar} from './Chapters';
import {Readiness} from './Readiness';

export type ViewTab = 'overview' | 'timeline' | 'triggers' | 'state' | 'missions' | 'quests' | 'wolves' | 'orders' | 'audit';
const TABS: {id: ViewTab; label: string}[] = [
    {id: 'overview', label: 'Overview'}, {id: 'timeline', label: 'Timeline'}, {id: 'triggers', label: 'Triggers'}, {id: 'state', label: 'Variables & timers'},
    {id: 'missions', label: 'Missions'}, {id: 'quests', label: 'Quests'}, {id: 'wolves', label: 'Wolves & map'}, {id: 'orders', label: 'Orders'}, {id: 'audit', label: 'Audit'},
];

export function CampaignView({me, target, row, detail, items, events, names, now, live, tab, setTab, earlier, refresh, explained, open}: {
    me: Me; target: Target; row: RunRow; detail: RunDetail | null; items: TimelineItem[]; events: Record<string, WorldEvent>;
    names: Record<string, string>; now: Now; live: Live | null; tab: ViewTab; setTab: (t: ViewTab) => void; earlier?: () => void;
    refresh: () => Promise<void> | void; explained?: Explained | null; open?: (run: string) => void}) {
    const [ticket, setTicket] = useState(''), [said, setSaid] = useState(''), [busy, setBusy] = useState(false);
    const [mapFocus, setMapFocus] = useState<{cell: string; x: number; y: number} | null>(null);
    const [ask, setAsk] = useState<Ask | null>(null), [sent, setSent] = useState<number | null>(null);
    const canDirect = atLeast(me, 'dm');
    const canSteer = maySteer(me) && steerable(row.status) && !!detail?.controls;
    const state = detail?.state ?? null;
    const strip = detail ? phaseStrip(detail.history, detail.phases, row.phase) : {path: [], next: []};
    // (The pressing waits stay first: the admin, the game, orders to send. The phase's triggers and exits are explained below.)
    const lines = waitingLines(row.waiting, names, now.day, state?.publishedAt, now.at)
        .filter((_, i) => !explained || ['publish', 'admin', 'game', 'send', 'end'].includes(row.waiting?.[i]?.on ?? ''));
    const here = new Set(((live?.frame?.people ?? []) as FramePerson[]).filter(p => p[2] === 'p').map(p => p[0]));
    const alerts = row.alerts ?? [];
    const act = async (work: () => Promise<unknown>, done: string) => {
        setBusy(true); setSaid('');
        try { await work(); setSaid(done); await refresh(); } catch (e) { setSaid((e as Error).message); } finally { setBusy(false); }
    };
    /** The admin's second click: one command, with its reason, made for this visit and this version. */
    const send = (reason: string, force: boolean) => {
        if (!ask || !detail) return;
        const payload = force ? {...ask.payload, force: true} : ask.payload;
        void act(async () => {
            const c = await storyProgram<{id: number}>(target, 'command', {run: row.run, kind: ask.kind, payload, reason, key: newKey(), visit: row.visit, digest: row.digest});
            setSent(c.id); setAsk(null);
        }, 'Sent: the story runner takes it within a tick.');
    };
    const mine = sent !== null ? detail?.commands?.find(c => c.id === sent) : undefined;
    const showOnMap = (place: {cell: string; x: number; y: number}) => { setMapFocus(place); setTab('wolves'); };
    return <div className="camp-view">
        <header className="camp-head">
            <div><small>{row.run} · version {row.digest.slice(0, 6)} · approved by {row.approver}{row.approvedAt ? ' ' + ago(row.approvedAt, now.at) : ''}</small>
                <h2>{row.title} <span className={`camp-status ${row.status}`}>{row.status}</span>{row.visit ? <small> visit {row.visit}</small> : null}</h2></div>
        </header>
        {row.moving && <p className="hint camp-pad-x camp-moving">⇪ Moving onto a new version: the game is being told its words for this visit. Until it answers,
            the story reads nothing and takes no other command; if the game refuses, it goes back.</p>}
        {canSteer && detail && <div className="camp-steer"><SteerBar row={row} detail={detail} names={names} steer={setAsk} busy={busy} /></div>}
        {!canSteer && canDirect && steerable(row.status) && me.hosted && <p className="hint camp-pad-x">Steering this world's campaigns needs its owner, or the owner's
            "may steer campaigns" for your account (My world, Editor accounts).</p>}
        {ask && <AskDialog key={ask.title + JSON.stringify(ask.payload)} ask={ask} row={row} names={names} busy={busy} send={send} cancel={() => setAsk(null)}
            target={target} />}
        {detail?.campaign && <ChapterBar detail={detail} names={names} open={run => open?.(run)} />}
        <div className="camp-strip" aria-label="Phases">
            {strip.path.map((p, i) => <span key={i} className={i === strip.path.length - 1 && row.phase === p ? 'now' : ''}>{i ? '▸ ' : ''}{i === strip.path.length - 1 && row.phase === p ? `[${p}]` : p}</span>)}
            {strip.next.length > 0 && <span className="next">▸ next: {strip.next.join(' or ')}</span>}
            <span className="hint">{now.day !== null ? `Now: day ${now.day.toFixed(1)}` : ''}{row.entered !== undefined && now.day !== null ? ` · in this phase ${(now.day - row.entered).toFixed(1)} days` : ''}</span>
        </div>
        <nav className="camp-tabs" role="tablist">{TABS.map(t => <button key={t.id} role="tab" aria-selected={tab === t.id} className={tab === t.id ? 'on' : ''}
            onClick={() => setTab(t.id)}>{t.label}</button>)}</nav>
        {said && <p className="hint dm-note">{said}{mine && <> · <b className={`camp-cmd ${mine.status}`}>{mine.status === 'queued' ? 'sent' : mine.status}</b>
            {mine.result?.text ? `: ${mine.result.text}` : ` (${commandLine(mine.kind, mine.payload, names)})`}</>}</p>}
        <section className="camp-body">
            {!detail ? <p className="hint">Reading this story…</p>
                : tab === 'overview' ? <div className="camp-overview">
                    {row.ready && <Readiness row={row} detail={detail} names={names} steer={canSteer ? setAsk : undefined} busy={busy} />}
                    <h3>Needs you</h3>
                    {alerts.length ? <ul className="camp-alerts">{alerts.map(a => <li key={a.kind} className={a.needsYou ? 'needs' : ''}>{alertText(a, names)}</li>)}</ul>
                        : <p className="hint">(nothing)</p>}
                    <h3>Waiting for</h3>
                    {lines.length > 0 && <ul className="camp-waiting">{lines.map((l, i) => <li key={i} className={l.tone}><b>{l.who}</b> {l.text}</li>)}</ul>}
                    {steerable(row.status) ? <WaitingFor explained={explained ?? null} names={names} here={here} busy={busy} target={target} run={row.run}
                        steer={canSteer ? setAsk : undefined} fire={canSteer ? l => setAsk(fireAsk({id: l.id!, actions: l.actions ?? []},
                            l.needsWolves ? (l.forWolves ?? []).find(w => here.has(w)) ?? (l.forWolves ?? [])[0] ?? '' : '', true, names)) : undefined} />
                        : !lines.length && <p className="hint">{row.status === 'complete' ? 'It has ended.' : row.status === 'stopped' ? 'It was stopped.' : '(nothing)'}</p>}
                    {steerable(row.status) && <><h3>Next by itself</h3>
                        <NextByItself explained={explained ?? null} names={names} publishedAt={state?.publishedAt} nowIso={now.at} />
                        <h3>What if… <small>(a dry run: nothing is saved or sent)</small></h3>
                        <WhatIfPanel target={target} run={row.run} names={names} cohort={state?.cohort ?? []} explained={explained ?? null} /></>}
                    {canDirect && row.status === 'running' && (detail.signals.length > 0 || detail.guestVoices.length > 0) && <details>
                        <summary>One-use tickets for the author bridge</summary>
                        <h3>Directions</h3>
                        <p className="hint">A one-use ticket, good for an hour, redeemed through the author bridge (tools/story_author.py).</p>
                        <div className="camp-controls">
                            {detail.signals.map(s => <button key={s} disabled={busy} onClick={() => void act(async () =>
                                setTicket((await storyProgram<{ticket: string}>(target, 'signal-ticket', {run: row.run, signal: s})).ticket), 'Ticket issued.')}>Allow direction “{s}” once</button>)}
                            {detail.guestVoices.map(npc => <button key={npc} disabled={busy} onClick={() => void act(async () =>
                                setTicket((await storyProgram<{ticket: string}>(target, 'voice-ticket', {run: row.run, npc})).ticket), 'Ticket issued.')}>Allow one guest line for {nameOf(names, npc)}</button>)}
                        </div>
                        {ticket && <label className="field"><span>Direction ticket</span><input readOnly value={ticket} onFocus={e => e.target.select()} /></label>}
                    </details>}
                    {detail.directions.length > 0 && <><h3>Guest lines given</h3><ul>{detail.directions.map(d =>
                        <li key={d.receipt}>{nameOf(names, d.npc)} · visit {d.visit} · {d.status}</li>)}</ul></>}
                    <h3>Lately</h3>
                    {items.length ? <ol className="camp-timeline">{items.slice(0, 8).map(i => <TimelineRow key={i.key} item={i} names={names} events={events} now={now.at}
                        onMap={showOnMap} />)}</ol> : <p className="hint">Nothing yet.</p>}
                    <details><summary>The approved program (read only)</summary><pre className="camp-json">{JSON.stringify(detail.program, null, 2)}</pre></details>
                </div>
                : tab === 'timeline' ? <Timeline items={items} events={events} names={names} now={now.at} earlier={earlier} onMap={showOnMap}
                    triggers={[...new Set(items.flatMap(i => (i.type === 'trigger' ? [i.trigger] : [])))]} cohort={state?.cohort ?? []}
                    undoFigure={canDirect ? (history, words) => { if (window.confirm(words + '?')) void act(() => storyProgram(target, 'figure-undo', {history}), 'The figure is put back.'); } : undefined} />
                : tab === 'triggers' ? <TriggersPanel detail={detail} names={names} canSteer={canSteer} steer={setAsk} busy={busy} counts={firingCounts(items)}
                    target={steerable(row.status) ? target : undefined} />
                : tab === 'state' ? <StatePanel detail={detail} names={names} now={now} items={items} steer={canSteer ? setAsk : undefined} busy={busy} />
                : tab === 'missions' ? <MissionGrid detail={detail} names={names} live={live} steer={canSteer ? setAsk : undefined} busy={busy} />
                : tab === 'quests' ? <QuestGrid detail={detail} names={names} live={live} steer={canSteer ? setAsk : undefined} busy={busy} />
                : tab === 'audit' ? <Audit detail={detail} names={names} now={now.at} />
                : tab === 'wolves' ? <WolvesMap target={target} detail={detail} names={names} live={live} items={items} focus={mapFocus} steer={canSteer ? setAsk : undefined} busy={busy} />
                : <Orders detail={detail} row={row} names={names} now={now.at} busy={busy} steer={canSteer ? setAsk : undefined} />}
        </section>
    </div>;
}
