// The world's variables (Docs/Design/85-story-triggers-and-campaign-control.md, 3.2 and Phase 10): kept for the world after a
// story ends and shared by every story that declares one of that name and type (the game cache of a map editor). Declared for
// the world here, with a type, a first value and a note; set and removed here too, each with a reason, audited. A live story
// reads a new value at its next turn. On a hosted world: the owner, or "may steer campaigns".
import {useCallback, useEffect, useState} from 'react';
import {storyProgram, type Target} from '../api';
import {parseWorldValue, worldVarText, type WorldVar, type WorldVars} from '../../lib/campaigns';

const TYPES = ['number', 'switch', 'text', 'wolf', 'resident', 'place', 'item', 'faction', 'numbers', 'texts', 'wolves', 'residents'];
type Editing = {do: 'declare'; name: string; type: string; value: string; note: string} | {do: 'set'; name: string; type: string; value: string}
    | {do: 'remove'; name: string; type: string};

export function WorldVarsPanel({target, names, mayChange, initial}: {target: Target; names: Record<string, string>; mayChange: boolean; initial?: WorldVars}) {
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
