// The world's variables (Docs/Design/85-story-triggers-and-campaign-control.md, 3.2 and Phase 10): kept for the world after a
// story ends and shared by every story that declares one of that name and type (the game cache of a map editor). Declared for
// the world here, with a type, a first value and a note; set and removed here too, each with a reason, audited. A live story
// reads a new value at its next turn. On a hosted world: the owner, or "may steer campaigns".
import {useCallback, useEffect, useState} from 'react';
import {storyProgram, type Target} from '../api';
import {parseWorldValue, valueText, worldVarText, type RealmVars, type WorldVar, type WorldVars} from '../../lib/campaigns';

const TYPES = ['number', 'switch', 'text', 'wolf', 'resident', 'place', 'item', 'faction', 'numbers', 'texts', 'wolves', 'residents'];
type Editing = {do: 'declare'; name: string; type: string; value: string; note: string} | {do: 'set'; name: string; type: string; value: string}
    | {do: 'remove'; name: string; type: string};

export function WorldVarsPanel({target, names, mayChange, initial, realm}: {target: Target; names: Record<string, string>; mayChange: boolean; initial?: WorldVars;
    /** Doc 86, J2: the "per wolf" tab's first values (tests and the render smoke). */ realm?: RealmVars}) {
    const [tab, setTab] = useState<'world' | 'realm'>(realm && !initial ? 'realm' : 'world');
    const tabs = <div className="camp-filters" role="tablist">
        <button role="tab" className={tab === 'world' ? 'on' : ''} aria-selected={tab === 'world'} onClick={() => setTab('world')}>The world's</button>
        <button role="tab" className={tab === 'realm' ? 'on' : ''} aria-selected={tab === 'realm'} onClick={() => setTab('realm')}>Per wolf</button></div>;
    return <>{tabs}{tab === 'world' ? <WorldOwn target={target} names={names} mayChange={mayChange} initial={initial}/>
        : <RealmVarsPanel target={target} names={names} mayChange={mayChange} initial={realm}/>}</>;
}

/** Doc 86, J2: each wolf's realm values, kept after a story ends and shared by every story that declares the name: filtered
 *  by name and wolf, set for one wolf with a reason (audited story.realm-var). */
export function RealmVarsPanel({target, names, mayChange, initial}: {target: Target; names: Record<string, string>; mayChange: boolean; initial?: RealmVars}) {
    const [data, setData] = useState<RealmVars | null>(initial ?? null);
    const [name, setName] = useState(''), [wolf, setWolf] = useState('');
    const [editing, setEditing] = useState<{name: string; wolf: string; type: string; value: string} | null>(null);
    const [reason, setReason] = useState(''), [problem, setProblem] = useState(''), [said, setSaid] = useState(''), [busy, setBusy] = useState(false);
    const read = useCallback(() => storyProgram<RealmVars>(target, 'realm-vars', {name: name.trim() || undefined, wolf: wolf.trim() || undefined})
        .then(setData, e => setProblem((e as Error).message)), [target, name, wolf]);
    useEffect(() => { if (!initial) void read(); }, []); // eslint-disable-line react-hooks/exhaustive-deps
    const send = async () => {
        if (!editing) return;
        const parsed = parseWorldValue(editing.type, editing.value);
        if ('problem' in parsed) return setProblem(parsed.problem);
        setBusy(true); setProblem('');
        try {
            const r = await storyProgram<{text: string}>(target, 'realm-var', {name: editing.name, wolf: editing.wolf, value: parsed.value, reason: reason.trim()});
            setSaid(r.text); setEditing(null); setReason(''); await read();
        } catch (e) { setProblem((e as Error).message); } finally { setBusy(false); }
    };
    if (!data) return <p className="hint camp-pad">{problem || "Reading the wolves' realm values…"}</p>;
    if (!data.realm) return <p className="hint camp-pad">This world needs migration 0081 to keep realm variables.</p>;
    return <div className="camp-world-vars" aria-label="Realm variables">
        <p className="hint">Each wolf's own values, kept after a story ends and read by every story that declares a realm variable of the same name and
            type: what a wolf did in one act, known in the next. A live story reads a changed value at its next turn. {mayChange ? '' : 'Setting one needs the owner, or "may steer campaigns".'}</p>
        <form className="wb-head" onSubmit={e => { e.preventDefault(); void read(); }}>
            <label>Name<input value={name} onChange={e => setName(e.target.value)} placeholder="any" list="realm-names"/></label>
            <datalist id="realm-names">{data.names.map(n => <option key={n.name} value={n.name}/>)}</datalist>
            <label>Wolf<input value={wolf} onChange={e => setWolf(e.target.value)} placeholder="any (an id)"/></label>
            <button type="submit">Show</button></form>
        <table className="dm-table"><thead><tr><th>Variable</th><th>Wolf</th><th>Value</th><th>Kept by</th><th /></tr></thead>
            <tbody>{data.values.map(v => <tr key={v.name + '|' + v.wolf}>
                <td><code>{v.name}</code><small>{v.type}</small></td><td>{names[v.wolf] ?? v.wolf}</td><td>{valueText(v.value, v.type, names)}</td>
                <td><small>{v.run ? 'a story' : 'set by hand'}{v.updatedAt ? ` · ${new Date(v.updatedAt).toLocaleString()}` : ''}</small></td>
                <td>{mayChange && <button disabled={busy} onClick={() => setEditing({name: v.name, wolf: v.wolf, type: v.type, value: valueAsText({value: v.value} as WorldVar)})}>✎ Set…</button>}</td>
            </tr>)}
                {!data.values.length && <tr><td colSpan={5} className="hint">No wolf keeps one{name || wolf ? ' of these' : ' yet'}.</td></tr>}</tbody></table>
        {data.names.length > 0 && <><h4>Declared by the world's stories</h4><ul className="camp-audit-list">{data.names.map(n =>
            <li key={n.name}><code>{n.name}</code> ({n.type ?? '?'}): {n.wolves} wolf{n.wolves === 1 ? '' : 'ves'} keep it · {n.stories.map(s => `${s.title} (${s.status}${n.type && s.type !== n.type ? `, as ${s.type}` : ''})`).join(', ') || 'no story now'}
                {mayChange && wolf.trim() && <button disabled={busy} onClick={() => setEditing({name: n.name, wolf: wolf.trim(), type: n.type ?? 'number', value: ''})}>Set for {names[wolf.trim()] ?? wolf.trim()}…</button>}</li>)}</ul></>}
        {editing && <div className="camp-ask" role="dialog" aria-label="Set a wolf's realm value">
            <div className="wb-head"><span><code>{editing.name}</code> for {names[editing.wolf] ?? editing.wolf}</span>
                <label className="wb-grow">Value<input value={editing.value} placeholder={hintOf(editing.type)} onChange={e => setEditing({...editing, value: e.target.value})} /></label></div>
            <label className="field"><span>Why (a few words, for the audit)</span><input value={reason} onChange={e => setReason(e.target.value)} /></label>
            <div className="camp-controls"><button className="primary" disabled={busy || reason.trim().split(/\s+/).length < 2} onClick={() => void send()}>Set it</button>
                <button onClick={() => setEditing(null)}>Cancel</button></div>
        </div>}
        {said && <p className="hint dm-note">{said}</p>}
        {problem && <p className="error-text">{problem}</p>}
        {(data.audit ?? []).length > 0 && <><h4>Changed by hand</h4><ul className="camp-audit-list">{data.audit!.slice(0, 20).map((a, i) =>
            <li key={i}><small>{new Date(a.at * 1000).toLocaleString()}</small> <b>{a.who}</b> · {a.detail}</li>)}</ul></>}
    </div>;
}

function WorldOwn({target, names, mayChange, initial}: {target: Target; names: Record<string, string>; mayChange: boolean; initial?: WorldVars}) {
    const [data, setData] = useState<WorldVars | null>(initial ?? null);
    const [editing, setEditing] = useState<Editing | null>(null);
    const [reason, setReason] = useState(''), [problem, setProblem] = useState(''), [said, setSaid] = useState(''), [busy, setBusy] = useState(false);
    const read = useCallback(() => storyProgram<WorldVars>(target, 'world-vars').then(setData, e => setProblem((e as Error).message)), [target]);
    useEffect(() => { if (!initial) void read(); }, [read]); // eslint-disable-line react-hooks/exhaustive-deps
    const send = async () => {
        if (!editing) return;
        const body: Record<string, unknown> = {do: editing.do, name: editing.name.trim(), reason: reason.trim()};
        if (editing.do !== 'remove') {
            const parsed = editing.value.trim() === '' && editing.do === 'declare' ? null : parseWorldValue(editing.type, editing.value);
            if (parsed && 'problem' in parsed) return setProblem(parsed.problem);
            if (parsed) body.value = parsed.value;
        }
        if (editing.do === 'declare') Object.assign(body, {type: editing.type, note: editing.note});
        setBusy(true); setProblem('');
        try {
            const r = await storyProgram<{text: string}>(target, 'world-var', body);
            setSaid(r.text); setEditing(null); setReason(''); await read();
        } catch (e) { setProblem((e as Error).message); } finally { setBusy(false); }
    };
    if (!data) return <p className="hint camp-pad">{problem || "Reading the world's variables…"}</p>;
    if (!data.declarations) return <p className="hint camp-pad">This world needs migration 0079 to declare its variables (the values stories keep are listed once it has it).</p>;
    const vars: WorldVar[] = data.variables;
    return <div className="camp-world-vars" aria-label="World variables">
        <p className="hint">Kept for the world after a story ends, and read by every story that declares a world variable of the same name and type (a story declaring it as
            another type is refused at approval). A live story reads a new value at its next turn. {mayChange ? '' : 'Changing them needs the owner, or "may steer campaigns".'}</p>
        <table className="dm-table"><thead><tr><th>Variable</th><th>Type</th><th>Value</th><th>Read by</th><th>Note</th><th /></tr></thead>
            <tbody>{vars.map(v => <tr key={v.name}>
                <td><code>{v.name}</code>{!v.declared && <small className="camp-warn" title="A story keeps it, but the world has not declared it">not declared</small>}</td>
                <td>{v.type}</td><td>{worldVarText(v, names)}{v.updatedBy ? <small>by {v.updatedBy}</small> : v.run ? <small>by a story</small> : null}</td>
                <td>{v.stories.length ? v.stories.map(s => <small key={s.story + (s.run ?? '')}>{s.title} ({s.status}{s.type !== v.type ? `, as ${s.type}` : ''})</small>) : <small className="hint">no story now</small>}</td>
                <td><small>{v.note}</small></td>
                <td>{mayChange && <span className="camp-controls">
                    {v.declared && <button disabled={busy} onClick={() => setEditing({do: 'set', name: v.name, type: v.type, value: valueAsText(v)})}>✎ Set…</button>}
                    <button disabled={busy} onClick={() => setEditing({do: 'declare', name: v.name, type: v.type, value: v.declared ? '' : valueAsText(v), note: v.note ?? ''})}>
                        {v.declared ? 'Declaration…' : 'Declare…'}</button>
                    {!v.unset && <button disabled={busy} onClick={() => setEditing({do: 'remove', name: v.name, type: v.type})}>Remove…</button>}</span>}</td>
            </tr>)}
                {!vars.length && <tr><td colSpan={6} className="hint">None yet.</td></tr>}</tbody></table>
        {mayChange && !editing && <div className="camp-controls"><button onClick={() => setEditing({do: 'declare', name: '', type: 'number', value: '', note: ''})}>+ Declare a world variable…</button></div>}
        {editing && <div className="camp-ask" role="dialog" aria-label="Change a world variable">
            <div className="wb-head">
                <label>Name<input value={editing.name} readOnly={editing.do !== 'declare' || vars.some(v => v.name === editing.name && v.declared)} maxLength={80}
                    onChange={e => setEditing({...editing, name: e.target.value})} /></label>
                {editing.do === 'declare' && <label>Type<select value={editing.type} onChange={e => setEditing({...editing, type: e.target.value})}>
                    {TYPES.map(t => <option key={t}>{t}</option>)}</select></label>}
                {editing.do !== 'remove' && <label className="wb-grow">{editing.do === 'declare' ? 'First value (empty: its type\'s)' : 'Value'}
                    <input value={editing.value} placeholder={hintOf(editing.type)} onChange={e => setEditing({...editing, value: e.target.value})} /></label>}
                {editing.do === 'declare' && <label className="wb-grow">Note<input value={editing.note} maxLength={300} onChange={e => setEditing({...editing, note: e.target.value})} /></label>}
            </div>
            {editing.do === 'remove' && <p>Remove <code>{editing.name}</code> from the world? A story declaring it starts again from its first value. Refused while a running story uses it.</p>}
            <label className="field"><span>Why (a few words, for the audit)</span><input value={reason} onChange={e => setReason(e.target.value)} /></label>
            <div className="camp-controls"><button className="primary" disabled={busy || reason.trim().split(/\s+/).length < 2 || !editing.name.trim()} onClick={() => void send()}>
                {editing.do === 'declare' ? 'Declare it' : editing.do === 'set' ? 'Set it' : 'Remove it'}</button><button onClick={() => setEditing(null)}>Cancel</button></div>
        </div>}
        {said && <p className="hint dm-note">{said}</p>}
        {problem && <p className="error-text">{problem}</p>}
        {(data.audit ?? []).length > 0 && <><h4>Changed by hand</h4><ul className="camp-audit-list">{data.audit!.slice(0, 20).map((a, i) =>
            <li key={i}><small>{new Date(a.at * 1000).toLocaleString()}</small> <b>{a.who}</b> · {a.detail}</li>)}</ul></>}
    </div>;
}

const valueAsText = (v: WorldVar) => v.value === null || v.value === undefined ? '' : Array.isArray(v.value) ? v.value.join(', ')
    : typeof v.value === 'boolean' ? (v.value ? 'on' : 'off') : String(v.value);
const hintOf = (type: string) => type === 'number' ? 'a whole number' : type === 'switch' ? 'on or off' : type === 'numbers' ? 'whole numbers, with commas'
    : type === 'texts' ? 'words, with commas' : type === 'wolves' || type === 'residents' ? 'ids, with commas' : type === 'text' ? 'up to 200 characters' : 'an id';
