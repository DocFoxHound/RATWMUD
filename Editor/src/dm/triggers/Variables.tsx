// The Variables panel of the workbench (Docs/Design/85-story-triggers-and-campaign-control.md, 4.2's right column and
// Phase 5): a version 2 story's declared variables, each a name, a type, a scope, a first value, a label to show it
// under and a note, with every use and rename-everywhere. A version 1 story lists its counters as it uses them, and
// offers "Make it version 2" (the op `upgrade`: the same story, its counters declared as numbers).
import {useState} from 'react';
import {isV2, LIST_TYPES, renameCounter, renameVariable, usesOf, variablesOf, type Variable} from '../../lib/workbench';
import {storyProgram, type Target} from '../api';
import {useBench} from './context';

const ID = /^[A-Za-z0-9_][A-Za-z0-9_.:-]{0,79}$/;
const SCOPE_WORDS: Record<string, string> = {story: "the story's", player: "each wolf's", party: "each party's", world: "the world's"};

/** A first value typed in, as its type wants it (a list: values separated by commas). Undefined: none (the type's own). */
function initialOf(type: string, text: string): unknown {
    if (text.trim() === '') return undefined;
    if (LIST_TYPES[type]) return [...new Set(text.split(',').map(x => x.trim()).filter(Boolean))].map(x => LIST_TYPES[type] === 'number' ? Number(x) : x);
    if (type === 'number') return Number(text);
    return text;
}
const initialText = (v: Variable) => v.initial === undefined ? '' : Array.isArray(v.initial) ? v.initial.join(', ') : String(v.initial);

function Row({v, use, readOnly}: {v: Variable; use: (id: string) => void; readOnly: boolean}) {
    const {program, edit, cat} = useBench();
    const types = cat.variables?.types ?? {};
    const put = (next: Variable, mark?: string) => edit({...program, variables: (program.variables ?? []).map(x => x.id === v.id ? next : x)}, mark);
    const without = (key: keyof Variable) => { const next = {...v}; delete next[key]; return next; };
    const used = usesOf(program, {type: 'variable', id: v.id}).length;
    const [open, setOpen] = useState(false);
    return <li className="wb-var">
        <div className="wb-var-head">
            {!readOnly && <button type="button" className="wb-fold" onClick={() => setOpen(!open)} aria-expanded={open} title="First value, label, note">{open ? '▾' : '▸'}</button>}
            <button type="button" className="link-button" onClick={() => use(v.id)} title="Where it is used">{v.id}</button>
            <small>{types[v.type] ?? v.type} · {SCOPE_WORDS[v.scope] ?? v.scope}{v.initial !== undefined ? ` · starts ${Array.isArray(v.initial) ? v.initial.join(', ') : String(v.initial)}` : ''}
                {v.show ? ` · shown as “${v.show}”` : ''}{used ? ` · used ${used}×` : ' · unused'}</small>
            {!readOnly && <span className="wb-tools">
                <button type="button" className="wb-mini" title="Rename everywhere" onClick={() => {
                    const to = window.prompt(`Rename the variable "${v.id}" everywhere`, v.id)?.trim();
                    if (to && to !== v.id && ID.test(to) && !(program.variables ?? []).some(x => x.id === to)) edit(renameVariable(program, v.id, to));
                }}>✎</button>
                <button type="button" className="wb-x" title="Remove the declaration" onClick={() => {
                    if (used && !window.confirm(`"${v.id}" is used ${used} times: remove its declaration anyway? (Problems will list what names it.)`)) return;
                    edit({...program, variables: (program.variables ?? []).filter(x => x.id !== v.id)});
                }}>×</button></span>}
        </div>
        {!readOnly && open && <div className="wb-var-fields">
            {v.type === 'switch'
                ? <label><input type="checkbox" checked={v.initial === true} onChange={e => put(e.target.checked ? {...v, initial: true} : without('initial'))}/>on at the start</label>
                : v.type === 'timer'
                    ? <label>Clock<select value={v.clock ?? 'game'} onChange={e => put(e.target.value === 'game' ? without('clock') : {...v, clock: e.target.value})}>
                        <option value="game">the world's</option><option value="real">real</option></select></label>
                    : <label>First value<input value={initialText(v)} placeholder={LIST_TYPES[v.type] ? 'values, separated by commas' : v.type === 'number' ? '0' : '(none)'}
                        onChange={e => { const value = initialOf(v.type, e.target.value); put(value === undefined ? without('initial') : {...v, initial: value}, 'v:' + v.id + ':initial'); }}/></label>}
            {['number', 'switch', 'timer'].includes(v.type) && <label>Shown as<input value={v.show ?? ''} maxLength={60} placeholder="(not shown)"
                onChange={e => put(e.target.value ? {...v, show: e.target.value} : without('show'), 'v:' + v.id + ':show')}/></label>}
            <label>Note<input value={v.note ?? ''} maxLength={300} placeholder="(for the author)"
                onChange={e => put(e.target.value ? {...v, note: e.target.value} : without('note'), 'v:' + v.id + ':note')}/></label>
        </div>}
    </li>;
}

export function VariablesPanel({use, readOnly, target}: {use: (what: {type: string; id: string; scope?: string}) => void; readOnly: boolean; target: Target}) {
    const {program, edit, cat} = useBench();
    const [adding, setAdding] = useState(false);
    const [name, setName] = useState('');
    const [type, setType] = useState('number');
    const [scope, setScope] = useState('story');
    const [note, setNote] = useState('');
    const v2 = isV2(program);
    const list = variablesOf(program);
    if (!v2) return <section className="wb-vars"><h4>Counters · {list.length}</h4>
        <ul>{list.map(c => <li key={c.scope + c.id}><button type="button" className="link-button" onClick={() => use({type: 'counter', id: c.id, scope: c.scope})}>{c.id}</button>
            <small> {c.scope === 'player' ? "each wolf's" : "the story's"} number</small>
            {!readOnly && <button type="button" className="wb-mini" title="Rename everywhere" onClick={() => {
                const to = window.prompt(`Rename the counter "${c.id}" everywhere`, c.id)?.trim();
                if (to && to !== c.id && ID.test(to)) edit(renameCounter(program, c.scope, c.id, to));
            }}>✎</button>}</li>)}</ul>
        <p className="hint">A version 1 story's counters are numbers named where they are used. Version 2 declares typed variables (numbers, switches,
            texts, wolves, lists, timers; the story's, each wolf's, each party's or the world's), and adds If, Wait, For each, Random and more.</p>
        {!readOnly && <button type="button" onClick={async () => {
            try {
                const {program: next} = await storyProgram<{program: typeof program}>(target, 'upgrade', {program});
                edit(next); setNote('Made version 2: its counters are declared numbers, and it rehearses as it did.');
            } catch (err) { setNote((err as Error).message); }
        }}>Make it version 2…</button>}
        {note && <p className="hint" role="status">{note}</p>}
    </section>;
    const add = () => {
        if (!ID.test(name) || list.some(v => v.id === name)) return;
        edit({...program, variables: [...(program.variables ?? []), {id: name, type, scope}]});
        setName(''); setAdding(false);
    };
    return <section className="wb-vars"><h4>Variables · {list.length}</h4>
        <ul>{list.map(v => <Row key={v.id} v={v} readOnly={readOnly} use={id => use({type: 'variable', id})}/>)}</ul>
        {!list.length && <p className="hint">{cat.variables?.help ?? 'Declare every variable the story uses.'}</p>}
        {!readOnly && (adding ? <form className="wb-var-new" onSubmit={e => { e.preventDefault(); add(); }}>
            <label>Name<input autoFocus value={name} onChange={e => setName(e.target.value)} placeholder="letters, digits, - _ . :"/></label>
            <label>Type<select value={type} onChange={e => setType(e.target.value)} aria-label="Type">{Object.entries(cat.variables?.types ?? {number: 'a number'}).map(([k, w]) =>
                <option key={k} value={k}>{w}</option>)}</select></label>
            <label>Whose<select value={scope} onChange={e => setScope(e.target.value)} aria-label="Scope">{Object.entries(cat.variables?.scopes ?? SCOPE_WORDS).map(([k, w]) =>
                <option key={k} value={k}>{w}</option>)}</select></label>
            <button type="submit" disabled={!ID.test(name) || list.some(v => v.id === name)}>Declare</button>
            <button type="button" onClick={() => setAdding(false)}>Cancel</button></form>
            : <button type="button" onClick={() => setAdding(true)}>+ New variable</button>)}
    </section>;
}
