// The other things the workbench's tree selects (Docs/Design/85, 4.1 decision 4 and 4.2): a phase's exit, shown as the
// trigger it is ("when … → go to …"), the phase itself (its name, journal, guest voices), its entry effects, and the
// campaign's settings.
import {useState} from 'react';
import {eventOf, exitParts, FIELDS, GATE, isV2, type Exit, type Phase, type Program, type Ref} from '../../lib/workbench';
import {useBench} from './context';
import {EventPicker, useOptions, type Values} from './pickers';
import {Sentence} from './Sentence';
import {cuesFor, EffectList} from './TriggerView';

const PHASE_ID = /^[A-Za-z0-9_][A-Za-z0-9_.:-]{0,79}$/;

function newExit(on: string, x: Exit): Exit {
    const base: Exit = {on, to: x.to};
    if (on === GATE) return {...base, missions: []};
    if (on === 'clock') return {...base, afterDays: 1};
    if (on === 'dm signal') return {...base, match: {detail: 'direction'}};
    return base;
}

export function ExitView({at}: {at: Ref & {kind: 'exit'}}) {
    const {cat, names, program, edit, problems} = useBench();
    const [choosing, setChoosing] = useState(false);
    const phase = program.phases[at.phase];
    const x = phase?.exits[at.index];
    if (!x) return <p className="hint">This exit is gone.</p>;
    const save = (next: Exit, mark?: string) => edit({...program, phases: program.phases.map((p, i) => i === at.phase ? {...p, exits: p.exits.map((e, n) => n === at.index ? next : e)} : p)}, mark);
    const pick = (slot: {slot: string}, v: Values) => {
        if (slot.slot === 'to') save({...x, to: String(v.to)});
        else if (slot.slot === 'missions') save({...x, missions: v.missions as string[]});
        else if (slot.slot === 'afterDays') save({...x, afterDays: Number(v.afterDays)});
        else {
            const match = {...x.match} as Record<string, string>;
            for (const [k, value] of Object.entries(v)) { if (value === undefined) delete match[k]; else match[k] = String(value); }
            const {match: _m, ...rest} = x; void _m;
            save(Object.keys(match).length ? {...rest, match} : rest);
        }
    };
    const mode = x.percent !== undefined ? 'percent' : x.players !== undefined ? 'players' : x.participant ? 'participant' : 'any';
    const ev = eventOf(cat, x.on);
    const filterable = (ev ? Object.keys(ev.fields) : [...FIELDS]).filter(k => (FIELDS as readonly string[]).includes(k) && !(k in (x.match ?? {})));
    const gated = x.on !== GATE && x.on !== 'clock' && x.on !== 'dm signal';
    return <article className="wb-trigger" aria-label={'Exit ' + (at.index + 1) + ' of ' + phase.id}>
        <p className="hint">An exit of <strong>{phase.id}</strong>: the older kind of trigger, which only moves the story on. The file keeps it as an exit.</p>
        <h5>When</h5>
        <div className="wb-line"><span className="wb-glyph">⚑</span>
            <Sentence parts={exitParts(cat, x, names, phase.missions ?? [])} pick={pick} problems={problems[`x:${at.phase}:${at.index}`]}
                cues={cuesFor(program, x.on)} clearable={s => (FIELDS as readonly string[]).includes(s.slot)}/>
            <button type="button" onClick={() => setChoosing(!choosing)}>Change event…</button></div>
        {ev?.help && <p className="hint wb-event-line">{ev.help}</p>}
        {choosing && <EventPicker exits value={x.on} close={() => setChoosing(false)} pick={on => { save(newExit(on, x)); setChoosing(false); }}/>}
        {gated && <div className="wb-head">
            <label>Who<select value={mode} onChange={e => {
                const {percent, players, participant, ...base} = x; void percent; void players; void participant;
                const m = e.target.value;
                save(m === 'any' ? base : {...base, ...(m === 'percent' ? {percent: 50} : m === 'players' ? {players: 1} : {participant: true})});
            }}><option value="any">anyone (any matching event)</option><option value="participant">a wolf of the story</option>
                <option value="players">a number of different wolves of the story</option><option value="percent">a share of the audience</option></select></label>
            {(mode === 'players' || mode === 'percent') && <label>{mode === 'percent' ? 'Percent' : 'Wolves'}<input type="number" min={1} max={mode === 'percent' ? 100 : 500}
                value={x.percent ?? x.players} onChange={e => save({...x, [mode]: e.target.valueAsNumber || 1}, `x:${at.phase}:${at.index}:n`)}/></label>}
            {filterable.length > 0 && <label>Add a filter<select value="" onChange={e => { if (e.target.value) save({...x, match: {...x.match, [e.target.value]: ''}}); }}>
                <option value="">the event's…</option>{filterable.map(k => <option key={k} value={k}>{ev?.fields[k]?.label ?? k}</option>)}</select></label>}
        </div>}
        <p><button type="button" className="wb-x-text" onClick={() => edit({...program, phases: program.phases.map((p, i) => i === at.phase ? {...p, exits: p.exits.filter((_, n) => n !== at.index)} : p)})}>Remove this exit</button></p>
    </article>;
}

function GuestVoices({phase, change}: {phase: Phase; change: (ids: string[]) => void}) {
    const residents = useOptions('resident');
    const {names} = useBench();
    const [adding, setAdding] = useState('');
    const list = phase.guestVoices ?? [];
    const found = adding ? residents.filter(r => (r.name + ' ' + r.id).toLowerCase().includes(adding.toLowerCase())).slice(0, 8) : [];
    return <div className="wb-voices"><h5>Optional guest voices</h5>
        <p className="hint">Residents a Dungeon Master may give one line during this phase (a ticket from the Campaigns console).</p>
        <div className="wb-chips">{list.map(id => <span key={id} className="wb-chip">{names.resident?.[id] ?? id}<button type="button" onClick={() => change(list.filter(x => x !== id))} aria-label={'Remove ' + id}>×</button></span>)}
            {list.length < 8 && <input type="search" placeholder="Add a resident…" value={adding} onChange={e => setAdding(e.target.value)} aria-label="Add a guest voice"/>}</div>
        {found.length > 0 && <div className="wb-options">{found.map(r => <button type="button" key={r.id} onClick={() => { change([...list, r.id]); setAdding(''); }}>
            <span>{r.name}</span><code>{r.id}</code></button>)}</div>}</div>;
}

export function PhaseView({at, select}: {at: Ref & {kind: 'phase' | 'entry'}; select: (r: Ref) => void}) {
    const {program, edit, problems} = useBench();
    const phase = program.phases[at.phase];
    const [name, setName] = useState(phase?.id ?? '');
    if (!phase) return null;
    const update = (patch: Partial<Phase>, mark?: string) => edit({...program, phases: program.phases.map((p, i) => i === at.phase ? {...phase, ...patch} : p)}, mark);
    const rename = () => {
        const next = name.trim();
        if (next === phase.id || !PHASE_ID.test(next) || program.phases.some(p => p.id === next)) { setName(phase.id); return; }
        edit(renamePhase(program, phase.id, next));
    };
    return <article className="wb-trigger" aria-label={'Phase ' + phase.id}>
        <div className="wb-head"><label>Phase<input value={name} onChange={e => setName(e.target.value)} onBlur={rename} onKeyDown={e => { if (e.key === 'Enter') rename(); }}/></label>
            <label className="wb-check"><input type="checkbox" checked={program.start === phase.id} onChange={() => edit({...program, start: phase.id})}/>the opening phase</label>
            {!!program.triggers?.length && <label className="wb-check"><input type="checkbox" checked={phase.campaignRules ?? !!(phase.triggers?.length || phase.exits.length)}
                onChange={e => update({campaignRules: e.target.checked})}/>campaign triggers run here</label>}</div>
        <label>The wolves' journal while it is open<textarea rows={3} value={phase.journal} onChange={e => update({journal: e.target.value}, `p:${at.phase}:journal`)}/></label>
        <h5>When it opens</h5>
        <EffectList effects={phase.effects} change={effects => update({effects})} problems={i => problems[`e:${at.phase}:${i}`]}/>
        <GuestVoices phase={phase} change={guestVoices => update({guestVoices})}/>
        <p className="hint">{phase.exits.length} exit{phase.exits.length === 1 ? '' : 's'}, {phase.triggers?.length ?? 0} trigger{phase.triggers?.length === 1 ? '' : 's'}, {phase.missions?.length ?? 0} mission{phase.missions?.length === 1 ? '' : 's'}
            {' '}<button type="button" onClick={() => select({kind: 'missions', phase: at.phase})}>Missions…</button>
            {program.phases.length > 1 && <button type="button" className="wb-x-text" onClick={() => {
                if (!window.confirm(`Remove the phase ${phase.id}? Exits and actions that go there will need another phase.`)) return;
                const phases = program.phases.filter((_, i) => i !== at.phase);
                edit({...program, phases, start: program.start === phase.id ? phases[0].id : program.start});
                select({kind: 'settings'});
            }}>Remove this phase</button>}</p>
    </article>;
}

/** A phase renamed everywhere: the opening, exits, "go to" actions, and the rehearsal cases' expectations. */
export function renamePhase(p: Program, from: string, to: string): Program {
    const r = (id: string) => id === from ? to : id;
    const rewrite = <T extends {actions: {do: string; phase?: string}[]}>(t: T): T => ({...t, actions: t.actions.map(a => a.do === 'enter' ? {...a, phase: r(a.phase!)} : a)});
    return {...p, start: r(p.start), ...(p.triggers ? {triggers: p.triggers.map(rewrite)} : {}),
        phases: p.phases.map(ph => ({...ph, id: r(ph.id), ...(ph.triggers ? {triggers: ph.triggers.map(rewrite)} : {}), exits: ph.exits.map(e => ({...e, to: r(e.to)}))})),
        tests: p.tests.map(c => ({...c, expect: c.expect.map(r), ...(c.expectEffects ? {expectEffects: c.expectEffects.map(e => ({...e, phase: r(String(e.phase))}))} : {}),
            ...(c.expectMissionVisits ? {expectMissionVisits: c.expectMissionVisits.map(v => ({...v, phase: r(String(v.phase))}))} : {})}))};
}

export function SettingsView() {
    const {program, edit} = useBench();
    const wolves = useOptions('wolf');
    const [find, setFind] = useState('');
    const players = program.audience.players ?? [];
    const found = find ? wolves.filter(w => (w.name + ' ' + w.id).toLowerCase().includes(find.toLowerCase()) && !players.includes(w.id)).slice(0, 8) : [];
    return <article className="wb-trigger" aria-label="Campaign settings">
        <div className="wb-head"><label>Title<input value={program.title} onChange={e => edit({...program, title: e.target.value}, 's:title')}/></label>
            <label>Story id<input value={program.id} onChange={e => edit({...program, id: e.target.value}, 's:id')} spellCheck={false}/></label>
            <label>Opening phase<select value={program.start} onChange={e => edit({...program, start: e.target.value})}>{program.phases.map(p => <option key={p.id}>{p.id}</option>)}</select></label></div>
        <h5>Who takes part</h5>
        <div className="wb-head"><label>Audience<select value={program.audience.kind} onChange={e => {
            const kind = e.target.value, cap = program.audience.cap;
            const keep = cap !== undefined && (kind === 'open' || kind === 'everyone') ? {cap} : {};
            edit({...program, audience: kind === 'active' ? {kind: 'active', days: 7} : kind === 'open' ? {kind: 'open', ...keep}
                : kind === 'everyone' ? {kind: 'everyone', days: program.audience.days ?? 7, ...keep} : {kind: 'players', players: []}});
        }}>
            <option value="active">every wolf active lately</option><option value="players">the wolves named</option>
            {/* (Phase 6: a living audience. Open: nobody at first, a wolf joins by taking a quest or a trigger's "take into the story";
                everyone: the wolves active lately, and every wolf who signs in after.) */}
            <option value="open" disabled={!isV2(program)}>open: nobody at first, anyone who takes a quest joins</option>
            <option value="everyone" disabled={!isV2(program)}>everyone: active lately, and each newcomer who signs in</option></select></label>
            {(program.audience.kind === 'active' || program.audience.kind === 'everyone') && <label>Active in the last … real days<input type="number" min={1} max={90} value={program.audience.days ?? 7}
                onChange={e => edit({...program, audience: {...program.audience, days: e.target.valueAsNumber || 1}}, 's:days')}/></label>}
            {(program.audience.kind === 'open' || program.audience.kind === 'everyone') && <label>At most … wolves<input type="number" min={1} max={5000}
                value={program.audience.cap ?? ''} placeholder="5000" onChange={e => { const {cap, ...rest} = program.audience; void cap;
                    edit({...program, audience: e.target.value === '' ? rest : {...rest, cap: Math.max(1, Math.min(5000, Math.round(e.target.valueAsNumber)))}}, 's:cap'); }}/></label>}</div>
        {(program.audience.kind === 'open' || program.audience.kind === 'everyone') && <p className="hint">A living audience (version 2): wolves join when they take a quest, when a
            trigger takes them in, {program.audience.kind === 'everyone' ? 'and when they sign in, ' : ''}up to the cap.</p>}
        {program.audience.kind === 'players' && <div className="wb-voices"><div className="wb-chips">{players.map(id => <span key={id} className="wb-chip">
            {wolves.find(w => w.id === id)?.name ?? id}<button type="button" aria-label={'Remove ' + id} onClick={() => edit({...program, audience: {kind: 'players', players: players.filter(x => x !== id)}})}>×</button></span>)}
            <input type="search" placeholder="Add a wolf…" value={find} onChange={e => setFind(e.target.value)} aria-label="Add a wolf"/></div>
            {found.length > 0 && <div className="wb-options">{found.map(w => <button key={w.id} type="button" onClick={() => { edit({...program, audience: {kind: 'players', players: [...players, w.id]}}); setFind(''); }}>
                <span>{w.name}</span><code>{w.id}</code></button>)}</div>}</div>}
        <label>"Quiet" alert after … real hours (the Campaigns console; 0: never, this one is slow on purpose; empty: the world's rule)
            <input type="number" min={0} max={8760} value={program.quietAfterHours ?? ''} onChange={e => { const {quietAfterHours, ...rest} = program; void quietAfterHours;
                edit(e.target.value === '' ? rest : {...rest, quietAfterHours: e.target.valueAsNumber}, 's:quiet'); }}/></label>
    </article>;
}
