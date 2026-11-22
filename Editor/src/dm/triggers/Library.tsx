// Templates (Docs/Design/85, 4.4): the parts of the 100 library events (an opening, objectives, outcomes) pasted in as
// triggers, a library event's mission added to a phase, a small story started from one; and the Dungeon Master's own
// templates (a `ratw-triggers` fragment: triggers and the regions they use), kept in this browser, exported and imported.
import {useRef, useState} from 'react';
import {download} from '../../lib/api';
import {readFragment, type Component, type Fragment, type Mission, type Trigger} from '../../lib/workbench';

export interface LibraryEntry extends Mission {components: Component[]; category: string; scale: string; opening?: {journal: string}}

export function loadTemplates(key: string): Fragment[] {
    try { const v = JSON.parse(localStorage.getItem(key) ?? '[]'); return Array.isArray(v) ? v.filter(f => f?.format === 'ratw-triggers') : []; } catch { return []; }
}
export function saveTemplates(key: string, list: Fragment[]): boolean {
    try { localStorage.setItem(key, JSON.stringify(list)); return true; } catch { return false; }
}

/** The library entry's triggers as one fragment, in its own folder: the opening (going to the phase after it) and its objectives. */
export function entryFragment(entry: LibraryEntry, roles: Set<string>, next?: string): Fragment {
    const triggers: Trigger[] = [];
    for (const c of entry.components) {
        if (!roles.has(c.role) || !c.trigger) continue;
        const t = structuredClone(c.trigger) as Trigger;
        // (The library's opening goes to its own "work" phase; pasted into another story it sends a signal instead,
        // for the author to give a meaning to: a phase to go to, a journal mission to start.)
        if (c.role === 'opening') t.actions = next ? [{do: 'enter', phase: next}] : [{do: 'emit', signal: entry.id + '-begins'}];
        t.folder = entry.title.slice(0, 80);
        triggers.push(t);
    }
    return {format: 'ratw-triggers', version: 1, title: entry.title, triggers};
}

export function LibraryPanel({entries, templates, setTemplates, paste, addMission, start, close, phaseId, canEdit}: {entries: LibraryEntry[]; templates: Fragment[];
    setTemplates: (f: Fragment[]) => void; paste: (f: Fragment) => void; addMission: (m: Mission) => void; start: (id: string) => void; close: () => void;
    phaseId: string | null; canEdit: boolean}) {
    const [q, setQ] = useState('');
    const [id, setId] = useState('');
    const [error, setError] = useState('');
    const file = useRef<HTMLInputElement>(null);
    const words = q.toLowerCase().split(/\s+/).filter(Boolean);
    const found = entries.filter(e => words.every(w => (e.title + ' ' + e.category + ' ' + e.scale + ' ' + e.objectives.map(o => o.label + ' ' + o.on).join(' ')).toLowerCase().includes(w)));
    const entry = found.find(e => e.id === id) ?? found[0];
    const categories = [...new Set(found.map(e => e.category))];
    return <div className="wb-library" role="dialog" aria-label="Library">
        <header><strong>Library</strong><span className="hint"> · 100 events in 10 categories, and your own templates</span>
            <button type="button" className="wb-close" onClick={close} aria-label="Close">×</button></header>
        <div className="wb-library-body">
            <div className="wb-library-list"><input type="search" autoFocus placeholder="Find an event (a theft, a fair, a lost pup…)" value={q} onChange={e => setQ(e.target.value)} aria-label="Find a library event"/>
                <div className="wb-options">{categories.map(c => <div key={c}><h6>{c}</h6>{found.filter(e => e.category === c).map(e => <button key={e.id} type="button"
                    aria-pressed={entry?.id === e.id} onClick={() => setId(e.id)}><span>{e.title}</span><small>{e.scale} · {e.mode}</small></button>)}</div>)}</div></div>
            {entry && <div className="wb-library-entry"><h4>{entry.title}</h4><p className="hint">{entry.category} · {entry.scale} · {entry.mode}</p>
                {entry.opening && <p>{entry.opening.journal}</p>}
                <ol>{entry.components.map((c, i) => <li key={i}><small className="wb-tag">{c.role}</small> {c.label}{c.trigger ? <code> {c.trigger.on}</code> : null}</li>)}</ol>
                <div className="wb-chips">
                    <button type="button" disabled={!canEdit} onClick={() => paste(entryFragment(entry, new Set(['opening', 'objective'])))}
                        title="Its opening and each objective, as triggers in a folder of their own">Add its triggers here{phaseId ? ' (' + phaseId + ')' : ''}</button>
                    <button type="button" disabled={!canEdit || !phaseId} onClick={() => addMission({id: entry.id, title: entry.title, mode: entry.mode, objectives: structuredClone(entry.objectives)})}
                        title={phaseId ? '' : 'Choose a phase in the tree first'}>Add its mission to {phaseId ?? 'a phase'}</button>
                    <button type="button" disabled={!canEdit} onClick={() => start(entry.id)}>New small story from it…</button></div></div>}
        </div>
        <h5>Your templates <small className="hint">(kept in this browser; right-click triggers → Save as a template)</small></h5>
        {templates.length ? <ul className="wb-templates">{templates.map((t, i) => <li key={i}><strong>{t.title || 'Untitled'}</strong> <small>{t.triggers.length} trigger{t.triggers.length === 1 ? '' : 's'}{t.regions?.length ? `, ${t.regions.length} region${t.regions.length === 1 ? '' : 's'}` : ''}</small>
            <button type="button" disabled={!canEdit} onClick={() => paste(t)}>Add here</button>
            <button type="button" onClick={() => download((t.title || 'triggers').replace(/[^a-zA-Z0-9_-]+/g, '-') + '.triggers.json', JSON.stringify(t, null, 2))}>Export</button>
            <button type="button" className="wb-x-text" onClick={() => setTemplates(templates.filter((_, n) => n !== i))}>Delete</button></li>)}</ul>
            : <p className="hint">None yet.</p>}
        <input ref={file} type="file" hidden accept=".json,application/json" onChange={async e => {
            const f = e.target.files?.[0]; e.target.value = '';
            if (!f) return;
            try { const fragment = readFragment(await f.text()); setTemplates([...templates, {...fragment, title: fragment.title || f.name.replace(/\.json$/, '')}]); setError(''); }
            catch (err) { setError((err as Error).message); }
        }}/>
        <button type="button" onClick={() => file.current?.click()}>Import a template file…</button>
        {error && <p role="alert">{error}</p>}
    </div>;
}
