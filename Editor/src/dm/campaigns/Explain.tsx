// Explaining a running campaign (Docs/Design/85-story-triggers-and-campaign-control.md, 5.5; Phase 3, "Explain it"): the
// Overview's "Waiting for" (every trigger and exit of the phase, each condition true, false or unknown with the values
// behind it, and for which wolves here now it would fire), [Why not?] (its last events and where each stopped), "What
// if…" (a made-up event through the rules on a copy of the run: nothing saved) and what will happen next by itself. All
// reads of the op `explain` (tools/story_runner.explain).
import {createContext, useContext, useEffect, useState} from 'react';
import {storyProgram, type Target} from '../api';
import {condLines, dueText, eventText, judgedState, nameOf, outcomeLines, secondsText, stepText, wouldFireFor, type Explained,
    type Judged, type WhatIf, type WhyNot, type Wording} from '../../lib/campaigns';
import type {SteerFn} from './Steer';

/** The trigger catalogue's sentences, once the page has the catalogue (doc 85, Phase 4). */
export const WordingContext = createContext<Wording | undefined>(undefined);

/** The conditions of a trigger as an indented list, each line marked ✔ ✘ or ?. */
export function Conditions({tree, names}: {tree: Judged['conditions']; names: Record<string, string>}) {
    const lines = condLines(tree, names, 0, useContext(WordingContext));
    if (!lines.length) return null;
    return <ul className="camp-conds">{lines.map((l, i) => <li key={i} className={l.verdict === true ? 'yes' : l.verdict === false ? 'no' : 'unknown'}
        style={{paddingLeft: 8 + l.depth * 16}}>◆ {l.text}</li>)}</ul>;
}

/** Why has this one not fired? Its last events of its kind, and where each stopped (the op `explain` with `trigger` or `exit`). */
export function WhyNotPanel({target, run, which, names, close}: {target: Target; run: string; which: {trigger: string} | {exit: number};
    names: Record<string, string>; close: () => void}) {
    const [data, setData] = useState<WhyNot | null>(null), [problem, setProblem] = useState('');
    const wording = useContext(WordingContext);
    useEffect(() => {
        let live = true;
        storyProgram<WhyNot>(target, 'explain', {run, ...which}).then(d => { if (live) setData(d); }, e => { if (live) setProblem((e as Error).message); });
        return () => { live = false; };
    }, [target, run, JSON.stringify(which)]); // eslint-disable-line react-hooks/exhaustive-deps
    const who = 'trigger' in which ? which.trigger : `the exit #${which.exit + 1}`;
    return <div className="camp-whynot" role="region" aria-label={`Why not ${who}`}>
        <div className="camp-whynot-head"><b>Why has {who} not fired?</b><button className="link-button" onClick={close}>close</button></div>
        {problem ? <p className="error-text">{problem}</p> : !data ? <p className="hint">Asking the story runner…</p> : <>
            <p className="hint">{eventText(data.listener, names, wording)} · {judgedState(data.listener, names, null).text}</p>
            <Conditions tree={data.listener.conditions} names={names} />
            {data.notLogged ? <p className="hint">{data.notLogged === 'story signal'
                ? `Signals are not in the world's log. ${(data.listener.timers ?? []).length ? `A timer sends it: ${(data.listener.timers ?? []).map(t => t.timer).join(', ')}.`
                    : (data.listener.senders ?? []).length ? `Sent by ${(data.listener.senders ?? []).map(s => s.trigger + (s.off ? ' (off)' : '')).join(', ')}.`
                    : 'Nothing in this phase sends it now: only the console can (Send signal).'}`
                : data.notLogged === 'clock' ? 'A delay: it fires when its time comes.' : data.notLogged === 'dm signal' ? 'A direction: it waits for a Dungeon Master.'
                : 'It waits for its missions in the journal (Missions tab).'}</p>
                : data.events.length === 0 ? <p className="hint">No event of its kind ({data.listener.on}) in the world's log yet{data.unread ? `; ${data.unread} not read yet` : ''}.</p>
                : <ol className="camp-steps">{data.events.map(e => <li key={e.event.id} className={e.verdict}>
                    <span className="camp-when">#{e.event.id} · day {Number(e.event.day).toFixed(1)}</span>
                    <span>{nameOf(names, e.event.actor) || 'someone'} · {e.event.kind}{e.event.target ? ' → ' + nameOf(names, e.event.target) : ''}{e.event.item ? ' · ' + e.event.item : ''}</span>
                    <b>{stepText(e, names)}</b></li>)}</ol>}
            {data.unread > 0 && !data.notLogged && <p className="hint">{data.unread} more of its kind not read yet (the story is behind or held).</p>}
        </>}
    </div>;
}

/** The Overview's "Waiting for" (doc 85, 5.3): each trigger and exit of the phase, judged, with [Fire…] and [Why not?]. */
export function WaitingFor({explained, names, here, steer, busy, target, run, fire}: {explained: Explained | null; names: Record<string, string>;
    here: Set<string>; steer?: SteerFn; busy: boolean; target: Target; run: string; fire?: (l: Judged) => void}) {
    const [why, setWhy] = useState<string | null>(null);
    const wording = useContext(WordingContext);
    if (!explained) return <p className="hint">Asking the story runner what it waits for…</p>;
    if (explained.ending) return <p className="hint">This phase is an ending: nothing to wait for.</p>;
    const live = explained.listeners.filter(l => !l.off && !l.used), spent = explained.listeners.filter(l => l.off || l.used);
    const row = (l: Judged) => {
        const id = l.source === 'trigger' ? `t:${l.id}` : `x:${l.index}`;
        const state = judgedState(l, names, explained.day);
        const wolves = wouldFireFor(l, here, names);
        return <li key={id} className={`camp-judged ${state.tone}${l.can ? '' : ' cannot'}`}>
            <div className="camp-judged-head">
                <span>⚑ <b>{l.source === 'trigger' ? l.id : `exit to ${l.to}`}</b> {eventText(l, names, wording)}</span>
                <span className="camp-judged-state">{state.text}</span>
                <span className="camp-judged-buttons">
                    {steer && fire && l.source === 'trigger' && <button disabled={busy} onClick={() => fire(l)}>Fire…</button>}
                    <button onClick={() => setWhy(why === id ? null : id)}>{why === id ? 'Hide' : 'Why not?'}</button></span>
            </div>
            <Conditions tree={l.conditions} names={names} />
            {(wolves || l.enters || l.to) && <small className="camp-judged-more">{wolves}{wolves && (l.enters || l.to) ? ' · ' : ''}
                {l.enters ? `then enters ${l.enters}` : l.to && l.source === 'exit' ? `goes to ${l.to}` : ''}</small>}
            {why === id && <WhyNotPanel target={target} run={run} which={l.source === 'trigger' ? {trigger: l.id!} : {exit: l.index!}} names={names}
                close={() => setWhy(null)} />}
        </li>;
    };
    return <>
        {explained.stall && <p className="camp-stall">⚠ Cannot move on: nothing in {explained.phase} can still fire by itself. Fire a trigger, set a variable,
            start a timer or go to a phase.</p>}
        <ul className="camp-judged-list">{live.map(row)}</ul>
        {spent.length > 0 && <details className="camp-spent"><summary>{spent.length} turned off or used up</summary><ul className="camp-judged-list">{spent.map(row)}</ul></details>}
    </>;
}

/** What will happen next by itself: the run's timers, soonest first, with an estimate in real time from the world's speed. */
export function NextByItself({explained, names, publishedAt, nowIso}: {explained: Explained | null; names: Record<string, string>; publishedAt?: number; nowIso: string}) {
    if (!explained?.timers.length) return <p className="hint">Nothing: no timer runs.</p>;
    return <ul className="camp-waiting">{explained.timers.map(t => <li key={t.name}><b>{t.signal}</b> {dueText(t.at, t.clock, explained.day, publishedAt, nowIso)}
        {t.seconds !== null ? ` · ${secondsText(t.seconds)}` : explained.speed === null && t.clock === 'game' ? ' · (the world\'s speed is not known yet)' : ''}
        {t.repeat ? ` · every ${t.repeat} days` : ''}{t.actor && t.name.includes('/') ? ` · ${nameOf(names, t.actor)}'s` : ''}</li>)}</ul>;
}

/** What if this happened? A made-up event (or story signal) through the rules on a copy of the run's state: nothing saved,
 *  no order sent. */
export function WhatIfPanel({target, run, names, cohort, explained}: {target: Target; run: string; names: Record<string, string>; cohort: string[];
    explained: Explained | null}) {
    const kinds = [...new Set([...(explained?.listeners ?? []).map(l => l.on), ...(explained?.missionEvents ?? [])]
        .filter(k => !['clock', 'dm signal', 'missions completed'].includes(k)))];
    const signals = [...new Set((explained?.listeners ?? []).filter(l => l.on === 'story signal').map(l => l.signal!))];
    const [kind, setKind] = useState(''), [signal, setSignal] = useState(''), [actor, setActor] = useState('');
    const [fields, setFields] = useState<Record<string, string>>({});
    const [out, setOut] = useState<WhatIf | null>(null), [problem, setProblem] = useState(''), [asking, setAsking] = useState(false);
    const picked = kind || kinds.find(k => k !== 'story signal') || kinds[0] || '';
    const pickedSignal = signal || signals[0] || '';
    const guess = (explained?.listeners ?? []).find(l => l.on === picked)?.match ?? {};
    const value = (k: string) => fields[k] ?? guess[k] ?? '';
    const ask = async () => {
        setAsking(true); setProblem(''); setOut(null);
        const fact: Record<string, unknown> = {kind: picked};
        if (picked === 'story signal') fact.signal = pickedSignal;
        if (actor) fact.actor = actor;
        for (const k of ['target', 'item', 'cell', 'detail']) if (value(k)) fact[k] = value(k);
        for (const k of ['quantity', 'coins', 'x', 'y']) if (fields[k]) fact[k] = Number(fields[k]);
        try { setOut(await storyProgram<WhatIf>(target, 'explain', {run, fact})); } catch (e) { setProblem((e as Error).message); } finally { setAsking(false); }
    };
    if (!kinds.length) return <p className="hint">This phase listens for nothing a made-up event could try.</p>;
    return <div className="camp-whatif">
        <div className="camp-controls">
            <select aria-label="Event" value={picked} onChange={e => { setKind(e.target.value); setFields({}); setOut(null); }}>{kinds.map(k => <option key={k}>{k}</option>)}</select>
            {picked === 'story signal' && <select aria-label="Signal" value={pickedSignal} onChange={e => setSignal(e.target.value)}>{signals.map(s => <option key={s}>{s}</option>)}</select>}
            <select aria-label="By" value={actor} onChange={e => setActor(e.target.value)}><option value="">by nobody in particular</option>
                {cohort.map(id => <option key={id} value={id}>by {nameOf(names, id)}</option>)}</select>
            {picked !== 'story signal' && ['target', 'item', 'quantity', 'coins', 'cell', 'x', 'y'].map(k => <input key={k} aria-label={k} placeholder={k} value={value(k)}
                size={k === 'x' || k === 'y' ? 4 : 10} className={k === 'x' || k === 'y' ? 'short' : ''} onChange={e => setFields(f => ({...f, [k]: e.target.value}))} />)}
            <button disabled={asking} onClick={() => void ask()}>{asking ? 'Asking…' : 'What would happen?'}</button>
        </div>
        {problem && <p className="error-text">{problem}</p>}
        {out && <div className="camp-whatif-out"><b>It would:</b>
            {outcomeLines(out, names).length ? <ul>{outcomeLines(out, names).map((l, i) => <li key={i}>{l}</li>)}</ul> : <p className="hint">do nothing: no trigger fires.</p>}
            {out.notFired.length > 0 && <><b>Not fired:</b><ul>{out.notFired.map(n => <li key={n.trigger}>{n.trigger}: {stepText(n, names)}</li>)}</ul></>}
            <p className="hint">A dry run on a copy of the story, as it stands now: nothing was saved or sent.</p></div>}
    </div>;
}
