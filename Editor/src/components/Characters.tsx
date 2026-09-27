import {useEffect, useMemo, useRef, useState} from 'react';
import {COATS, PATTERNS, SEXES, SPECIES, STATURES} from '../model/model.mjs';
import {api} from '../lib/api';
import {formatHour, VOICE_COLORS} from '../lib/glyphs';
import {BEHAVIOR_LABELS, newCharacter, PREFERENCE_LABELS, type Behavior, type Character, type Profession, type Roster} from '../lib/roster';
import {editRoster, getState, isRosterDirty, setState, toast, useStore} from '../lib/store';
import {Hint, NumberField, Row, Section, SelectField, TextField, Toggle} from './fields';
import {Portrait} from './Portrait';

type Filter = 'all' | 'free' | 'employed' | 'gone';

export async function loadRoster() {
    try {
        const roster = await api.roster();
        setState({roster, savedRoster: roster});
    } catch (error) { toast(`Could not load the character roster: ${(error as Error).message}`, 'error'); }
}

export async function saveRoster() {
    const roster = getState().roster;
    if (!roster) return;
    try {
        const saved = await api.saveRoster(roster);
        setState({roster: saved, savedRoster: saved});
        toast('Saved the character roster.', 'success');
    } catch (error) { toast((error as Error).message, 'error'); }
}

export function CharactersWorkspace() {
    const roster = useStore(s => s.roster);
    const selected = useStore(s => s.character);
    const dirty = useStore(isRosterDirty);
    const [filter, setFilter] = useState<Filter>('all');
    const [query, setQuery] = useState('');
    const [profession, setProfession] = useState('');
    const list = useMemo(() => {
        if (!roster) return [];
        const q = query.trim().toLowerCase();
        return roster.characters.filter(c =>
            (filter === 'all' || (filter === 'free' && c.status === 'active' && !c.assignment)
                || (filter === 'employed' && !!c.assignment) || (filter === 'gone' && c.status !== 'active'))
            && (!profession || c.profession === profession || (!c.profession && (c.preferences[profession] ?? 0) > 0))
            && (!q || [c.name, c.id, c.personality, ...c.traits].some(t => t.toLowerCase().includes(q))));
    }, [roster, filter, query, profession]);
    if (!roster) return <div className="characters loading">Loading the character roster…</div>;
    const current = roster.characters.find(c => c.id === selected) ?? null;
    return (
        <div className="characters">
            <aside className="roster-list">
                <div className="roster-head">
                    <div><b>Character roster</b><small>{roster.characters.length} characters · shared by every world</small></div>
                    <button className={dirty ? 'primary' : ''} disabled={!dirty} onClick={saveRoster}>{dirty ? 'Save roster' : 'Saved'}</button>
                </div>
                <div className="roster-tools">
                    <input value={query} onChange={e => setQuery(e.target.value)} placeholder="Search names, traits…" aria-label="Search characters" />
                    <div className="segmented small">{(['all', 'free', 'employed', 'gone'] as Filter[]).map(f =>
                        <button key={f} className={filter === f ? 'on' : ''} onClick={() => setFilter(f)}>{{all: 'All', free: 'Free', employed: 'Employed', gone: 'Dead/removed'}[f]}</button>)}</div>
                    <select value={profession} onChange={e => setProfession(e.target.value)} aria-label="Suited to profession">
                        <option value="">Any profession</option>
                        {roster.professions.map(p => <option key={p.id} value={p.id}>Suits: {p.name}</option>)}
                    </select>
                    <div className="button-grid two">
                        <button onClick={() => { const c = newCharacter(roster); editRoster(r => { r.characters.push(c); }); setState({character: c.id}); }}>+ New</button>
                        <button className="accent" onClick={() => setState({dialog: 'generate'})}>✦ Generate</button>
                    </div>
                </div>
                <div className="roster-items">
                    {list.map(c => <button key={c.id} className={c.id === selected ? 'roster-item on' : 'roster-item'} onClick={() => setState({character: c.id})}>
                        <Portrait appearance={c.appearance} age={c.age} size={42} />
                        <span><b>{c.name}</b><small>{employment(roster, c)}</small></span>
                        {c.status !== 'active' && <em className={c.status}>{c.status}</em>}
                        {c.origin === 'llm' && <i title="Generated with AI">✦</i>}
                    </button>)}
                    {!list.length && <p className="hint pad">No characters match. Create one, or generate a batch with AI.</p>}
                </div>
            </aside>
            <main className="character-sheet">
                {current ? <CharacterSheet roster={roster} c={current} /> : <div className="empty-sheet">
                    <h2>Characters live here, not in a map</h2>
                    <p>Design a pool of wolves with personalities, backstories and job preferences. Place <b>profession slots</b> in your worlds (the ◇ tool); each export fills empty slots with the free character who most wants that job, and they keep it for life: no one ever appears in two places.</p>
                    <p>Named NPCs you design for one specific place are still placed directly with the W tool.</p>
                </div>}
            </main>
            <ProfessionCatalog roster={roster} />
        </div>
    );
}

function employment(roster: Roster, c: Character) {
    const job = roster.professions.find(p => p.id === c.profession)?.name;
    if (c.assignment) return `${job} · ${c.assignment.world}/${c.assignment.slot}`;
    if (job) return `${job} for life · between posts`;
    const best = Object.entries(c.preferences).sort((a, b) => b[1] - a[1])[0];
    return best && best[1] > 0 ? `Free · prefers ${roster.professions.find(p => p.id === best[0])?.name ?? best[0]}` : 'Free · no job preferences yet';
}

function CharacterSheet({roster, c}: {roster: Roster; c: Character}) {
    const set = (fields: Partial<Character>) => editRoster(r => { Object.assign(r.characters.find(x => x.id === c.id)!, fields); });
    const look = c.appearance;
    const setLook = (fields: Partial<Character['appearance']>) => set({appearance: {...look, ...fields}});
    const locked = !!c.profession;
    return (<>
        <header className="sheet-head">
            <Portrait appearance={look} age={c.age} size={150} className="big" />
            <div><small>{c.origin === 'llm' ? 'GENERATED WITH AI · EDITABLE' : 'CHARACTER'}</small><h1>{c.name}</h1>
                <p>{employment(roster, c)} · id <code>{c.id}</code></p></div>
        </header>
        <div className="sheet-columns">
            <div>
                <Section title="Identity">
                    <Row><TextField label="Name" value={c.name} max={120} onCommit={v => set({name: v})} />
                        <NumberField label="Age" value={c.age} min={0} max={200} onCommit={v => set({age: v})} /></Row>
                    <TextField label="Appearance & bearing" multiline value={c.description} max={4000} placeholder="What others see when they inspect this wolf." onCommit={v => set({description: v})} />
                    <TextField label="Greeting" value={c.greeting} max={1024} placeholder="An offline line when nobody is running the live model." onCommit={v => set({greeting: v})} />
                </Section>
                <Section title="Personality & story (sent to the live-dialogue model)">
                    <TextField label="Personality" multiline value={c.personality} max={2000} placeholder="Temperament, values, fears, how they talk." onCommit={v => set({personality: v})} />
                    <TextField label="Traits" value={c.traits.join(', ')} max={500} hint="Comma-separated, up to 12." onCommit={v => set({traits: v.split(',').map(t => t.trim().slice(0, 40)).filter(Boolean).slice(0, 12)})} />
                    <TextField label="Backstory" multiline value={c.backstory} max={6000} placeholder="Where they come from, who they love, what they owe." onCommit={v => set({backstory: v})} />
                    <Hint>The game also keeps each character's own memories of every player they meet, per world save, so their history grows in play.</Hint>
                </Section>
            </div>
            <div>
                <Section title="Job preferences">
                    <Hint>Profession slots draw the free character with the highest preference. “Never” means they are never drawn for it.</Hint>
                    {roster.professions.map(p => <div key={p.id} className="pref-row">
                        <span>{p.name}{locked && c.profession === p.id && <em> · their profession</em>}</span>
                        <div className="segmented small">{PREFERENCE_LABELS.map((label, v) =>
                            <button key={v} className={(c.preferences[p.id] ?? 0) === v ? 'on' : ''} title={label}
                                onClick={() => set({preferences: {...c.preferences, [p.id]: v}})}>{v === 0 ? '–' : '★'.repeat(v)}</button>)}</div>
                    </div>)}
                    {!roster.professions.length && <Hint>Add professions in the catalog first.</Hint>}
                </Section>
                <Section title="Life & employment">
                    {locked ? <Hint>Drawn into <b>{roster.professions.find(p => p.id === c.profession)?.name}</b> by an export. That is their profession for life{c.assignment ? `, working at ${c.assignment.world} / ${c.assignment.slot}` : ', currently between posts'}.</Hint>
                        : <Hint>Not yet employed. The next export that needs someone like them may draw them.</Hint>}
                    <SelectField label="Status" value={c.status} onChange={v => set({status: v})}
                        hint={c.status === 'active' ? 'Alive and available to their profession.' : 'Never drawn again. Their slot is refilled at the next export; their record and history stay.'}
                        options={[{value: 'active', label: 'Active'}, {value: 'dead', label: 'Dead'}, {value: 'removed', label: 'Removed from the world'}]} />
                    {!locked && <button className="danger" onClick={() => { editRoster(r => { r.characters = r.characters.filter(x => x.id !== c.id); }); setState({character: null}); }}>Delete character</button>}
                    {locked && <Hint>Characters who have worked can't be deleted; mark them dead or removed so their history is kept.</Hint>}
                </Section>
                <Section title="Appearance">
                    <Row>
                        <SelectField label="Species" value={look.species} onChange={v => setLook({species: v})} options={SPECIES.map(v => ({value: v, label: v[0].toUpperCase() + v.slice(1)}))} />
                        <SelectField label="Sex" value={look.sex} onChange={v => setLook({sex: v})} options={SEXES.map(v => ({value: v, label: v}))} />
                    </Row>
                    <Row>
                        <SelectField label="Stature" value={look.stature} onChange={v => setLook({stature: v})} options={STATURES.map(v => ({value: v, label: v}))} />
                        <SelectField label="Markings" value={look.pattern} onChange={v => setLook({pattern: v})} options={PATTERNS.map(v => ({value: v, label: v}))} />
                    </Row>
                    {(['baseColor', 'gradientColor', 'markingColor'] as const).map(k => <div className="field" key={k}>
                        <span>{{baseColor: 'Coat', gradientColor: 'Coat gradient', markingColor: 'Markings'}[k]} · {COATS[look[k]][0]}</span>
                        <div className="swatches">{COATS.map(([name, hex], i) => <button key={name} title={name} aria-label={name} className={look[k] === i ? 'on' : ''} style={{background: hex}} onClick={() => setLook({[k]: i})} />)}</div>
                    </div>)}
                    <div className="field"><span>Voice color in chat</span>
                        <div className="swatches small">{VOICE_COLORS.map((hex, i) => <button key={i} aria-label={`Voice color ${i}`} className={c.voice === i ? 'on' : ''} style={{background: hex}} onClick={() => set({voice: i})} />)}</div>
                    </div>
                </Section>
            </div>
        </div>
    </>);
}

function ProfessionCatalog({roster}: {roster: Roster}) {
    const [editing, setEditing] = useState<string | null>(null);
    const blank: Profession = {id: '', name: '', behavior: 'civilian', workLabel: 'working', description: '', hours: {start: 8, end: 17}, paid: true};
    const [draft, setDraft] = useState<Profession>(blank);
    const current = editing ? roster.professions.find(p => p.id === editing) : null;
    const value = current ?? draft;
    const change = (fields: Partial<Profession>) => current
        ? editRoster(r => { Object.assign(r.professions.find(p => p.id === current.id)!, fields); })
        : setDraft({...draft, ...fields});
    const inUse = (id: string) => roster.characters.some(c => c.profession === id);
    return (
        <aside className="catalog">
            <div className="roster-head"><div><b>Professions</b><small>Jobs slots can offer</small></div>
                <button onClick={() => { setEditing(null); setDraft(blank); }}>+ New</button></div>
            <div className="catalog-list">{roster.professions.map(p => <button key={p.id} className={editing === p.id ? 'on' : ''} onClick={() => setEditing(p.id)}>
                <b>{p.name}</b><small>{BEHAVIOR_LABELS[p.behavior]} · {formatHour(p.hours.start)}–{formatHour(p.hours.end)} · {roster.characters.filter(c => c.profession === p.id).length} employed</small></button>)}</div>
            <div className="catalog-form">
                <h3>{current ? `Edit ${current.name}` : 'New profession'}</h3>
                {!current && <TextField label="ID (permanent)" value={draft.id} max={48} hint="e.g. ferryman" onCommit={v => setDraft({...draft, id: v.toLowerCase()})} />}
                <TextField label="Name" value={value.name} max={120} onCommit={v => change({name: v})} />
                <SelectField<Behavior> label="Behaves like" value={value.behavior} onChange={v => change({behavior: v})}
                    hint="The game's routine for this job. New professions reuse the closest one until they get their own AI."
                    options={[{value: 'guard', label: 'Guard: patrol or post'}, {value: 'merchant', label: 'Shopkeeper: counter & trade'}, {value: 'civilian', label: 'Worker: work, evenings, home'}]} />
                <TextField label="Activity label" value={value.workLabel} max={40} onCommit={v => change({workLabel: v})} />
                <Row><NumberField label="Starts" value={value.hours.start} min={0} max={23.75} step={0.25} onCommit={v => change({hours: {...value.hours, start: v}})} />
                    <NumberField label="Ends" value={value.hours.end} min={0} max={23.75} step={0.25} onCommit={v => change({hours: {...value.hours, end: v}})} /></Row>
                <Toggle label="Paid by the treasury" checked={value.paid} onChange={v => change({paid: v})} />
                <TextField label="Description" multiline value={value.description} max={1000} onCommit={v => change({description: v})} />
                {current ? <button className="danger" disabled={inUse(current.id)} title={inUse(current.id) ? 'Characters hold this profession for life' : ''}
                    onClick={() => { editRoster(r => { r.professions = r.professions.filter(p => p.id !== current.id); r.characters.forEach(c => { delete c.preferences[current.id]; }); }); setEditing(null); }}>Delete profession</button>
                    : <button className="primary" onClick={() => {
                        if (!/^[a-z][a-z0-9_-]{0,47}$/.test(draft.id) || roster.professions.some(p => p.id === draft.id)) { toast('Choose a new ID: lowercase letters, digits, _ or -.', 'error'); return; }
                        if (!draft.name.trim()) { toast('Give the profession a name.', 'error'); return; }
                        editRoster(r => { r.professions.push(draft); }); setEditing(draft.id); setDraft(blank);
                    }}>Add profession</button>}
            </div>
        </aside>
    );
}

export function GenerateDialog() {
    const roster = useStore(s => s.roster);
    const ref = useRef<HTMLDialogElement>(null);
    const [ai, setAi] = useState<{available: boolean; model: string; error: string} | null>(null);
    const [count, setCount] = useState(4);
    const [favor, setFavor] = useState<string[]>([]);
    const [theme, setTheme] = useState('');
    const [busy, setBusy] = useState(false);
    const [results, setResults] = useState<Character[]>([]);
    const [keep, setKeep] = useState<Set<string>>(new Set());
    useEffect(() => { ref.current?.showModal(); api.ai().then(setAi).catch(() => setAi({available: false, model: '', error: 'The local host is unreachable.'})); }, []);
    const generate = async () => {
        setBusy(true);
        try {
            const made = await api.generate(count, favor, theme);
            setResults(made); setKeep(new Set(made.map(c => c.id)));
        } catch (error) { toast((error as Error).message, 'error'); }
        finally { setBusy(false); }
    };
    const add = () => {
        const chosen = results.filter(c => keep.has(c.id));
        editRoster(r => { for (const c of chosen) if (!r.characters.some(x => x.id === c.id)) r.characters.push(c); });
        toast(`Added ${chosen.length} character${chosen.length === 1 ? '' : 's'}. Review them, then Save roster.`, 'success');
        if (chosen[0]) setState({character: chosen[0].id});
        ref.current?.close();
    };
    return (
        <dialog ref={ref} className="modal wide" onClose={() => setState({dialog: null})}>
            <div className="modal-head"><small>CHARACTER ROSTER</small><h2>Generate characters with AI</h2>
                <button className="icon" onClick={() => ref.current?.close()} aria-label="Close">×</button></div>
            <div className="modal-body">
                {ai && !ai.available && <p className="hint bad">{ai.error}</p>}
                {ai?.available && <Hint>Uses {ai.model} through the local live-NPC configuration. The key never leaves the local host. Each batch is one paid request; nothing is saved until you add and save.</Hint>}
                <Row><NumberField label="How many (1–8)" value={count} min={1} max={8} onCommit={setCount} />
                    <div className="field"><span>Lean toward professions</span>
                        <div className="chips">{roster?.professions.map(p => <button key={p.id} className={favor.includes(p.id) ? 'on' : ''}
                            onClick={() => setFavor(f => f.includes(p.id) ? f.filter(x => x !== p.id) : [...f, p.id])}>{p.name}</button>)}</div></div></Row>
                <TextField label="Theme or notes" multiline value={theme} max={1000} placeholder="e.g. river-trade folk from the eastern marshes; one should be secretly in debt to bandits." onCommit={setTheme} />
                {results.length > 0 && <div className="generated">{results.map(c => <label key={c.id} className="generated-card">
                    <input type="checkbox" checked={keep.has(c.id)} onChange={e => setKeep(k => { const n = new Set(k); if (e.target.checked) n.add(c.id); else n.delete(c.id); return n; })} />
                    <Portrait appearance={c.appearance} age={c.age} size={72} />
                    <div><b>{c.name}</b><small>{c.age} · {c.appearance.species} · {c.traits.join(', ')}</small><p>{c.personality}</p>
                        <small>Best at: {Object.entries(c.preferences).filter(([, v]) => v >= 2).map(([k]) => roster?.professions.find(p => p.id === k)?.name ?? k).join(', ') || '—'}</small></div>
                </label>)}</div>}
                <div className="modal-actions">
                    <button onClick={() => ref.current?.close()}>Close</button>
                    <button disabled={!ai?.available || busy} onClick={generate}>{busy ? 'Writing characters…' : results.length ? 'Generate another batch' : '✦ Generate'}</button>
                    {results.length > 0 && <button className="primary" disabled={!keep.size} onClick={add}>Add {keep.size} to roster</button>}
                </div>
            </div>
        </dialog>
    );
}
