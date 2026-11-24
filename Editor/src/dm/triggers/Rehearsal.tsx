// Rehearsal along the bottom of the workbench (Docs/Design/85, 4.5): each case as pass or fail with its path; choosing one
// shows its trace in the tree (each trigger's count, the others dimmed); "Step through" plays it one fact at a time with
// the phase, the journal, counters, timers and what fired, and what the phase then waits for, judged as the Campaigns
// console judges a running story (the op `rehearse-step`: story_runner.listeners on the rehearsal's state).
import {useContext, useState} from 'react';
import {storyProgram, type Target} from '../api';
import {eventText, judgedState, type Judged} from '../../lib/campaigns';
import {eventParts, plain, tokenWords, type Fact, type Program} from '../../lib/workbench';
import {Conditions, WordingContext} from '../campaigns/Explain';
import {useBench} from './context';

export interface TraceItem {phase: string; trigger: string; scope?: string; event?: string; on: string; signal?: string; cell?: string; x?: number; y?: number}
export interface CaseReport {name: string; passed: boolean; path: string[]; expected: string[]; triggers?: TraceItem[]; counters?: Record<string, unknown>;
    effects: Record<string, unknown>[]; missionVisits?: {phase: string; visit: number; credits: Record<string, string[]>}[]}
export interface Report {uncoveredTriggers?: string[]; uncovered: string[]; voices: {phase: string; npc: string}[]; digest: string; passed: boolean;
    tests: CaseReport[]; effects: Record<string, unknown>[]}
export interface Step {fact: Fact | null; phase: string; visit: number; path: string[]; journal: string; counters: Record<string, unknown>;
    timers: Record<string, {at: number; signal?: string; clock?: string}>; fired: TraceItem[]; effects: Record<string, unknown>[];
    missions: {id: string; title?: string; ready: boolean}[]; ending: boolean; listening?: Judged[]}
export interface Steps {case: string; passed: boolean; expected: string[]; steps: Step[]; cohort: string[]}

function FactLine({fact}: {fact: Fact}) {
    const {cat, names} = useBench();
    const ev = plain(eventParts(cat, {on: fact.kind, match: Object.fromEntries((['actor', 'target', 'cell', 'item', 'detail'] as const)
        .filter(k => fact[k]).map(k => [k, String(fact[k])]))}, names));
    return <span>day {fact.day} · <b>{fact.kind}</b>: {ev}{fact.quantity ? ` · ${fact.quantity}` : ''}{fact.coins ? ` · ${fact.coins} coins` : ''}
        {fact.cell && fact.x !== undefined ? ` · at ${names.place?.[fact.cell] ?? fact.cell} (${fact.x}, ${fact.y})` : ''}</span>;
}

export function RehearsalStrip({report, program, target, busy, rehearse, chosen, choose, stepping, setStepping}: {report?: Report; program: Program | null; target: Target;
    busy: boolean; rehearse: () => void; chosen: number | null; choose: (i: number | null) => void;
    stepping: {steps: Steps; at: number} | null; setStepping: (s: {steps: Steps; at: number} | null) => void}) {
    const [problem, setProblem] = useState('');
    const [asking, setAsking] = useState(false);
    async function step(i: number) {
        if (!program) return;
        setAsking(true); setProblem('');
        try { setStepping({steps: await storyProgram<Steps>(target, 'rehearse-step', {program, case: i}), at: 0}); choose(i); }
        catch (e) { setProblem((e as Error).message); } finally { setAsking(false); }
    }
    return <section className="wb-rehearsal" aria-label="Rehearsal">
        <header><button type="button" disabled={busy || !program} onClick={rehearse}>Rehearse ▶</button>
            {report ? <span className={report.passed ? 'ok' : 'bad'}>{report.passed ? '✔ every case passes' : '✖ a case fails'}
                {report.uncovered.length ? ` · phases no case reaches: ${report.uncovered.join(', ')}` : ''}
                {report.uncoveredTriggers?.length ? ` · ${report.uncoveredTriggers.length} trigger${report.uncoveredTriggers.length === 1 ? '' : 's'} no case reaches` : ''}</span>
                : <span className="hint">Rehearse every case, then choose one to see its trace in the tree, or step through it.</span>}
            {problem && <span role="alert"> {problem}</span>}</header>
        {report && <ol className="wb-cases">{report.tests.map((t, i) => <li key={i} className={(t.passed ? 'pass' : 'fail') + (chosen === i ? ' chosen' : '')}>
            <button type="button" onClick={() => { choose(chosen === i ? null : i); setStepping(null); }} aria-pressed={chosen === i}>
                {t.passed ? '✔' : '✖'} {t.name}</button>
            <small>{t.path.join(' → ')}{!t.passed && <> · expected {t.expected.join(' → ')}</>}{t.triggers ? ` · ${t.triggers.length} firing${t.triggers.length === 1 ? '' : 's'}` : ''}</small>
            <button type="button" disabled={asking} onClick={() => void step(i)}>Step through</button></li>)}</ol>}
        {stepping && <Stepper data={stepping.steps} at={stepping.at} go={at => setStepping({...stepping, at})} close={() => setStepping(null)}/>}
    </section>;
}

function Stepper({data, at, go, close}: {data: Steps; at: number; go: (i: number) => void; close: () => void}) {
    const {names} = useBench();
    const wording = useContext(WordingContext);
    const s = data.steps[Math.min(at, data.steps.length - 1)];
    const flat: Record<string, string> = Object.assign({}, ...Object.values(names));
    const before = at > 0 ? data.steps[at - 1] : null;
    const changed = (k: string, v: unknown) => before && JSON.stringify((before.counters as Record<string, unknown>)?.[k]) !== JSON.stringify(v);
    const story = (s.counters as {story?: Record<string, number>}).story ?? {};
    const player = (s.counters as {player?: Record<string, Record<string, number>>}).player ?? {};
    return <div className="wb-stepper" aria-label={'Stepping through ' + data.case}>
        <header><strong>{data.case}</strong> · step {at} of {data.steps.length - 1}
            <button type="button" disabled={at === 0} onClick={() => go(0)} aria-label="First step">⏮</button>
            <button type="button" disabled={at === 0} onClick={() => go(at - 1)} aria-label="Previous step">◀</button>
            <button type="button" disabled={at >= data.steps.length - 1} onClick={() => go(at + 1)} aria-label="Next step">▶</button>
            <button type="button" disabled={at >= data.steps.length - 1} onClick={() => go(data.steps.length - 1)} aria-label="Last step">⏭</button>
            <button type="button" className="wb-close" onClick={close} aria-label="Stop stepping">×</button></header>
        <div className="wb-step-grid">
            <div><h6>The fact</h6><p>{s.fact ? <FactLine fact={s.fact}/> : 'The story is published: the opening phase opens.'}</p>
                <h6>Phase</h6><p><b>{s.phase}</b> (visit {s.visit}){s.ending ? ' · an ending' : ''} · path {s.path.join(' → ')}</p>
                <p className="wb-journal">“{s.journal}”</p>
                {s.fired.length > 0 && <><h6>Fired</h6><ul>{s.fired.map((f, i) => <li key={i}>{f.scope === 'campaign' ? 'Campaign' : f.phase} / <b>{f.trigger}</b> ← {f.signal ?? f.on}</li>)}</ul></>}
                {s.effects.length > 0 && <><h6>Sent to the game</h6><ul>{s.effects.map((e, i) => <li key={i}><code>{String(e.kind)}</code> {Object.entries(e).filter(([k]) => k !== 'kind' && k !== 'phase')
                    .map(([k, v]) => `${k} ${flat[String(v)] ?? (typeof v === 'string' ? tokenWords(v, names) : String(v))}`).join(', ')}</li>)}</ul></>}</div>
            <div><h6>Counters</h6>{!Object.keys(story).length && !Object.keys(player).length ? <p className="hint">none yet</p> : <dl className="wb-counters">
                {Object.entries(story).map(([k, v]) => <div key={k} className={changed('story', story) && (before?.counters as {story?: Record<string, number>})?.story?.[k] !== v ? 'changed' : ''}><dt>{k}</dt><dd>{v}</dd></div>)}
                {Object.entries(player).flatMap(([w, o]) => Object.entries(o).map(([k, v]) => <div key={w + k}><dt>{k} <small>{flat[w] ?? w}</small></dt><dd>{v}</dd></div>))}</dl>}
                {Object.keys(s.timers).length > 0 && <><h6>Timers</h6><ul>{Object.entries(s.timers).map(([k, t]) => <li key={k}>{k}: due day {Number(t.at).toFixed(2)}{t.clock === 'real' ? ' (real)' : ''}</li>)}</ul></>}
                {s.missions.length > 0 && <><h6>Missions</h6><ul>{s.missions.map(m => <li key={m.id}>{m.title ?? m.id}: {m.ready ? '✔ ready' : 'open'}</li>)}</ul></>}</div>
            <div><h6>Waiting for</h6>{s.ending ? <p className="hint">Nothing: an ending.</p> : <ul className="camp-judged-list">{(s.listening ?? []).map((l, i) => {
                const state = judgedState(l, flat, s.fact?.day ?? 0);
                return <li key={i} className={`camp-judged ${state.tone}${l.can ? '' : ' cannot'}`}><div className="camp-judged-head">
                    <span>⚑ <b>{l.source === 'trigger' ? l.id : `exit to ${l.to}`}</b> {eventText(l, flat, wording)}</span><span className="camp-judged-state">{state.text}</span></div>
                    <Conditions tree={l.conditions} names={flat}/></li>;
            })}</ul>}</div>
        </div></div>;
}
