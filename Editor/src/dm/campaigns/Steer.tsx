// Steering a running campaign (Docs/Design/85-story-triggers-and-campaign-control.md, 5.4 and 5.8; Phase 2): every control
// sends one command to the story runner's inbox (op `command`), never a change of its own. Each asks first: what it will do,
// a reason of a few words (shown in the timeline and the audit), and for stop, going back and skipping an order the story's
// title typed. The reply is the command's id; the console shows it "sent", then "done" or "refused: why" once the runner has
// taken it (within a tick).
import {useEffect, useState} from 'react';
import {storyProgram, type Target} from '../api';
import {commandLine, nameOf, previewLines, typedTitle, wayOf, type CommandKind, type Controls, type Preview, type RunDetail, type RunRow} from '../../lib/campaigns';
import {WhyNotPanel} from './Explain';

/** A command waiting for the admin's second click. */
export interface Ask { kind: CommandKind; payload: Record<string, unknown>; title: string; does: string[]; warn?: string; way?: 'on' | 'back' | 'again' }
export type SteerFn = (ask: Ask) => void;

/** Since Phase 3 the dialog's "This will do" is a dry run (doc 85, 5.5): the command taken by the runner's own code on the run
 *  as it stands, rolled back (the op `explain` with `command`); what the program says it does stays while it is asked, and
 *  if it cannot be. */
export function AskDialog({ask, row, names, busy, send, cancel, target, dryRun}: {ask: Ask; row: RunRow; names: Record<string, string>; busy: boolean;
    send: (reason: string, force: boolean) => void; cancel: () => void; target?: Target; dryRun?: Preview}) {
    const [reason, setReason] = useState(''), [typed, setTyped] = useState(''), [force, setForce] = useState(false);
    const [preview, setPreview] = useState<Preview | null>(dryRun ?? null), [asked, setAsked] = useState(false);
    useEffect(() => {
        if (!target || dryRun) return;
        let live = true;
        setAsked(true);
        storyProgram<Preview>(target, 'explain', {run: row.run, command: {kind: ask.kind, payload: ask.payload}})
            .then(p => { if (live) setPreview(p); }, () => { if (live) setPreview(null); })
            .finally(() => { if (live) setAsked(false); });
        return () => { live = false; };
    }, [target, row.run, ask.kind, JSON.stringify(ask.payload)]); // eslint-disable-line react-hooks/exhaustive-deps
    const does = preview ? previewLines(preview, names) : ask.does;
    const needsTitle = typedTitle(ask.kind, ask.payload, ask.way);
    const ready = reason.trim().length >= 6 && reason.trim().split(/\s+/).length >= 2 && (!needsTitle || typed.trim() === row.title);
    return <div className="camp-ask" role="dialog" aria-label={ask.title}>
        <h3>{ask.title}</h3>
        <p className="camp-ask-line">You: {commandLine(ask.kind, ask.payload, names)}</p>
        {does.length > 0 && <><b>This will do{preview ? '' : asked ? ' (asking the runner for a dry run…)' : ''}:</b>
            <ul className={preview?.outcome === 'refused' ? 'camp-ask-refused' : ''}>{does.map((d, i) => <li key={i}>{d}</li>)}</ul>
            {preview && <p className="hint">A dry run on the story as it stands now; it is taken as it stands when the runner gets it.</p>}</>}
        {ask.warn && <p className="camp-ask-warn">{ask.warn}</p>}
        <label className="field"><span>Why (shown in the timeline and the audit)</span>
            <input autoFocus value={reason} maxLength={300} placeholder="e.g. two coils were handed over before the story saw them"
                onChange={e => setReason(e.target.value)} /></label>
        {needsTitle && <label className="field"><span>Type the story's title to confirm: “{row.title}”</span>
            <input value={typed} onChange={e => setTyped(e.target.value)} /></label>}
        {!['hold', 'resume', 'stop'].includes(ask.kind) && <label className="camp-check"><input type="checkbox" checked={force} onChange={e => setForce(e.target.checked)} />
            even if it has moved on (made for visit {row.visit})</label>}
        <div className="camp-controls">
            <button className="primary" disabled={busy || !ready} onClick={() => send(reason.trim(), force)}>{busy ? 'Sending…' : 'Send it'}</button>
            <button onClick={cancel}>Cancel</button>
        </div>
    </div>;
}

/** The header's row (doc 85, 5.3): hold or resume, stop, send a signal, go to a phase, tell the wolves. */
export function SteerBar({row, detail, names, steer, busy}: {row: RunRow; detail: RunDetail; names: Record<string, string>; steer: SteerFn; busy: boolean}) {
    const c = detail.controls;
    const [signal, setSignal] = useState(''), [phase, setPhase] = useState(''), [effects, setEffects] = useState(true), [telling, setTelling] = useState(false);
    const [to, setTo] = useState<'participants' | 'wolf' | 'region'>('participants'), [wolf, setWolf] = useState(''), [region, setRegion] = useState(''), [text, setText] = useState('');
    if (!c) return null;
    const cohort = detail.state?.cohort ?? [];
    const pickedSignal = signal || c.signals[0] || '';
    const pickedPhase = phase || c.phases.find(p => p !== row.phase) || '';
    const way = wayOf(c.phases, row.phase, pickedPhase);
    return <>
        <div className="camp-controls" aria-label="Steer this campaign">
            {row.status === 'held'
                ? <>
                    <button disabled={busy} onClick={() => steer({kind: 'resume', payload: {catchUp: true}, title: 'Resume',
                        does: ['It reads the world events that happened while it was held, in order: they count.']})}>▶ Resume</button>
                    <button disabled={busy} onClick={() => steer({kind: 'resume', payload: {catchUp: false}, title: 'Resume from now',
                        does: ['It carries on from now: what happened while it was held is skipped (its timers still fire).']})}>▶ Resume from now</button>
                </>
                : row.status === 'running' && <button disabled={busy} onClick={() => steer({kind: 'hold', payload: {}, title: 'Hold',
                    does: ['It reads no events, sends no orders and fires no timers until resumed.', 'Orders already sent complete. No event is lost: they wait in the log.']})}>⏸ Hold</button>}
            <button disabled={busy} onClick={() => steer({kind: 'stop', payload: {}, title: 'Stop this campaign',
                does: ['Tells the game to end it (story.stop); it is "stopping" until the game acknowledges.', 'Its future phases are cancelled.'],
                warn: 'What it already gave the world stays. A stopped run cannot be resumed.'})}>⏹ Stop…</button>
            {c.signals.length > 0 && <span className="camp-group">
                <select value={pickedSignal} onChange={e => setSignal(e.target.value)} aria-label="Signal">{c.signals.map(s => <option key={s}>{s}</option>)}</select>
                <button disabled={busy} onClick={() => steer({kind: 'signal', payload: {name: pickedSignal}, title: `Send signal "${pickedSignal}"`,
                    does: c.triggers.filter(t => t.on === 'story signal' && t.signal === pickedSignal).map(t => `${t.off ? '(off) ' : ''}${t.id}: ${t.actions.join(' → ')}`)
                        .concat(detail.listening.filter(l => l.on === 'dm signal' && l.match?.detail === pickedSignal).map(l => `the direction exit to ${l.to}`))})}>Send signal</button></span>}
            <span className="camp-group">
                <select value={pickedPhase} onChange={e => setPhase(e.target.value)} aria-label="Phase">{c.phases.map(p => <option key={p} value={p}>{p}{p === row.phase ? ' (now)' : ''}</option>)}</select>
                <label className="camp-check"><input type="checkbox" checked={effects} onChange={e => setEffects(e.target.checked)} /> with its entry effects</label>
                <button disabled={busy || !pickedPhase} onClick={() => steer({kind: 'enter', payload: {phase: pickedPhase, effects}, way,
                    title: way === 'back' ? `Go back to ${pickedPhase}` : way === 'again' ? `Open ${pickedPhase} again` : `Go to ${pickedPhase}`,
                    does: [`A new visit of ${pickedPhase} (visit ${(row.visit ?? 0) + 1}), its journal sent to the story's wolves.`,
                        effects ? 'Its approved entry effects are sent, one by one, as on any entry.' : 'Its entry effects are not sent.',
                        'This visit\'s timers and once-only marks go; the campaign\'s stay.'],
                    warn: way !== 'on' ? 'Nothing is undone: what the world was given stays given (doc 85, 5.1 rule 5).' : undefined})}>Go to phase…</button>
            </span>
            <button disabled={busy} onClick={() => setTelling(t => !t)}>✉ Tell the wolves…</button>
        </div>
        {telling && <div className="camp-tell">
            <select value={to} onChange={e => setTo(e.target.value as typeof to)} aria-label="To">
                <option value="participants">the story's wolves</option><option value="wolf">one wolf</option>
                {c.regions.length > 0 && <option value="region">the wolves in a region</option>}</select>
            {to === 'wolf' && <select value={wolf} onChange={e => setWolf(e.target.value)} aria-label="Wolf"><option value="">choose…</option>
                {cohort.map(id => <option key={id} value={id}>{nameOf(names, id)}</option>)}</select>}
            {to === 'region' && <select value={region} onChange={e => setRegion(e.target.value)} aria-label="Region"><option value="">choose…</option>
                {c.regions.map(r => <option key={r}>{r}</option>)}</select>}
            <input value={text} maxLength={300} placeholder="One line, as a banner and a line in their log" onChange={e => setText(e.target.value)} />
            <button disabled={busy || !text.trim() || (to === 'wolf' && !wolf) || (to === 'region' && !region)} onClick={() => { setTelling(false); steer({kind: 'tell',
                payload: {to, text: text.trim(), ...(to === 'wolf' ? {player: wolf} : {}), ...(to === 'region' ? {region} : {})}, title: 'Tell the wolves',
                does: ['Those in the world now see it as a banner and a line; those away are told when they are back.', 'The story itself does not move.']}); }}>Send…</button>
            <button onClick={() => setTelling(false)}>×</button>
        </div>}
    </>;
}

/** The Triggers tab (doc 85, 5.3): the phase's triggers and the campaign's, each on or off, how often fired, [Fire…]. */
/** Fire a trigger now (5.4), as asked from the Triggers tab and the Overview. */
export function fireAsk(t: {id: string; actions: string[]}, wolf: string, check: boolean, names: Record<string, string>): Ask {
    return {kind: 'fire', payload: {trigger: t.id, check, ...(wolf ? {actor: wolf} : {})},
        title: `Fire ${t.id} now`, does: [`Runs its actions as if its event had happened${wolf ? ' for ' + nameOf(names, wolf) : ''}: ${t.actions.join(' → ')}.`,
            check ? 'Its conditions and once-only rule are checked first; refused if they say no.' : 'Unchecked: its conditions and once-only rule are not asked.',
            'Marked "by hand" in the history.']};
}

export function TriggersPanel({detail, names, canSteer, steer, busy, counts, target}: {detail: RunDetail; names: Record<string, string>; canSteer: boolean;
    steer: SteerFn; busy: boolean; counts: Record<string, {count: number; last: string | null; day: number | null}>; target?: Target}) {
    const c: Controls | null | undefined = detail.controls;
    const [wolves, setWolves] = useState<Record<string, string>>({}), [checks, setChecks] = useState<Record<string, boolean>>({});
    const [why, setWhy] = useState<string | null>(null);
    if (!c) return <p className="hint">This run has ended: it has no triggers to steer.</p>;
    const cohort = detail.state?.cohort ?? [];
    return <div className="camp-triggers">
        <table className="dm-table"><thead><tr><th>Trigger</th><th>When</th><th>Does</th><th>Fired</th><th>On</th>{canSteer && <th>By hand</th>}{target && <th></th>}</tr></thead>
            <tbody>{c.triggers.flatMap(t => {
                const n = counts[t.id];
                const check = checks[t.id] ?? true;
                const wolf = wolves[t.id] ?? '';
                const rows = [<tr key={t.key} className={t.off ? 'off' : ''}>
                    <td><b>{t.id}</b><small>{t.scope === 'campaign' ? 'campaign' : 'this phase'} · {t.once === 'each' ? 'every time' : 'once per ' + t.once}</small></td>
                    <td>{t.on}{t.signal ? ` "${t.signal}"` : ''}{t.hasConditions ? <small>with conditions</small> : null}</td>
                    <td>{t.actions.join(' → ')}</td>
                    <td>{n ? <>{n.count}×<small>last day {n.day?.toFixed(1)}</small></> : <span className="hint">not in the timeline</span>}</td>
                    <td>{canSteer ? <button className={`camp-switch ${t.off ? 'off' : 'on'}`} disabled={busy} title={t.off ? 'Turned off: click to turn it on' : 'On: click to turn it off'}
                        onClick={() => steer({kind: t.off ? 'enable' : 'disable', payload: {trigger: t.id}, title: `Turn ${t.id} ${t.off ? 'on' : 'off'}`,
                            does: [t.off ? 'It fires again when its event comes.' : 'It does not fire, whatever happens, until turned on again.',
                                t.scope === 'phase' ? 'For this phase, on every visit of it.' : 'For the whole campaign.']})}>{t.off ? 'off' : 'on'}</button>
                        : t.off ? 'off' : 'on'}</td>
                    {canSteer && <td className="camp-fire">
                        <select value={wolf} onChange={e => setWolves(w => ({...w, [t.id]: e.target.value}))} aria-label="As wolf">
                            <option value="">{t.needsWolf ? 'choose a wolf…' : 'no wolf'}</option>
                            {cohort.map(id => <option key={id} value={id}>{nameOf(names, id)}</option>)}</select>
                        <label className="camp-check"><input type="checkbox" checked={check} onChange={e => setChecks(x => ({...x, [t.id]: e.target.checked}))} /> check</label>
                        <button disabled={busy || (t.needsWolf && !wolf)} onClick={() => steer(fireAsk(t, wolf, check, names))}>Fire…</button>
                    </td>}
                    {target && <td><button onClick={() => setWhy(why === t.id ? null : t.id)}>{why === t.id ? 'Hide' : 'Why not?'}</button></td>}
                </tr>];
                if (target && why === t.id) rows.push(<tr key={t.key + ':why'} className="camp-why-row"><td colSpan={canSteer ? 7 : 6}>
                    <WhyNotPanel target={target} run={detail.run} which={{trigger: t.id}} names={names} close={() => setWhy(null)} /></td></tr>);
                return rows;
            })}</tbody></table>
        {c.offElsewhere.length > 0 && <p className="hint">Also turned off, in other phases: {c.offElsewhere.join(', ')}.</p>}
    </div>;
}
