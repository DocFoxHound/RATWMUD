import {useEffect, useRef, useState} from 'react';
import {checkStory, exportStory, importStory, inspectRun, newStory, storyId, triggers, type Rehearsal, type StoryDraft, type StoryStep, type Trigger} from '../lib/storyRehearsal';
import {dmApi, worldName, type Me, type Npcs, type Target} from './api';
import {useWorld} from './world';
import {download} from '../lib/api';
import {StoryPicker} from './StoryPicker';
import {StoryProgramPanel} from './StoryProgramPanel';

interface StoryFile {draft: StoryDraft; run: Rehearsal | null}
interface Library {files: StoryFile[]; selected: string}
function blank(): Library {
    const draft = newStory();
    return {files: [{draft, run: null}], selected: draft.id};
}
function read(key: string) {
    try {
        const raw = localStorage.getItem(key);
        if (raw === null) return {book: blank(), raw, error: ''};
        const book: Library = JSON.parse(raw);
        if (!book.files?.some(f => f.draft.id === book.selected)) throw Error();
        for (const {draft, run} of book.files) { checkStory(draft); if (run) inspectRun(run); }
        return {book, raw, error: ''};
    } catch { return {book: blank(), raw: null, error: 'The saved story library could not be read. Browser storage may be unavailable.'}; }
}

export function StoryCreator({me, target, openPersonal}: {me: Me; target: Target; openPersonal: () => void}) {
    const key = 'ratw.story-creator.v1.' + JSON.stringify([target, me.account || me.username]);
    const [store, setStore] = useState(() => read(key));
    const fileInput = useRef<HTMLInputElement>(null);
    const latestStore = useRef(store);
    latestStore.current = store;
    const [fileError, setFileError] = useState('');
    const [catalogError, setCatalogError] = useState('');
    const [roster, setRoster] = useState<Npcs | null>(null);
    const [mode, setMode] = useState<'write' | 'rehearse' | 'program'>('write');
    const [eventPlace, setEventPlace] = useState('');
    const world = useWorld(target, setCatalogError);
    useEffect(() => {
        let active = true;
        dmApi.npcs(target).then(n => { if (active) setRoster(n); }).catch(e => { if (active) setCatalogError(String(e.message)); });
        return () => { active = false; };
    }, [target]);
    const {book} = store;
    const file = book.files.find(f => f.draft.id === book.selected)!;
    const {draft, run} = file;
    const view = run && inspectRun(run);
    const editable = me.role !== 'viewer';
    const castName = run && [...(roster?.people ?? []), ...(roster?.holders ?? [])].find(p => p.id === run.story.resident)?.name;
    const placeName = run && [...(world?.cells ?? []), ...(world?.rooms ?? [])].find(p => p.id === run.story.place)?.name;
    const issues = checkStory(draft);
    const waitsForArrival = draft.steps.filter(s => !s.ending && s.exits.length > 0 && s.exits.every(x => x.when === 'arrival'));
    if (draft.resident && !roster) issues.push('The resident roster is needed to check the cast.');
    if (draft.place && !world) issues.push('The atlas is needed to check this location.');
    if (draft.resident && roster) {
        const resident = [...roster.people, ...roster.holders].find(p => p.id === draft.resident);
        if (!resident?.home || roster.dead.includes(draft.resident)) issues.push('Choose a living resident with a home placement, or clear the cast.');
    }
    if (draft.place && world && ![...world.cells, ...world.rooms].some(c => c.id === draft.place)) issues.push('Choose a location in this world.');
    function persist(next: Library) {
        try {
            if (localStorage.getItem(key) !== store.raw) {
                setStore({...store, error: 'Another tab changed the library. Reopen its saved version before editing.'}); return;
            }
            const raw = JSON.stringify(next);
            localStorage.setItem(key, raw); setStore({book: next, raw, error: ''});
        } catch { setStore({...store, book: next, error: 'Browser storage is unavailable. Edits last for this session.'}); }
    }
    function update(next: StoryFile) { persist({...book, files: book.files.map(f => f.draft.id === draft.id ? next : f)}); }
    function edit(changes: Partial<StoryDraft>) { update({...file, draft: {...draft, ...changes}}); }
    function stepEdit(id: string, changes: Partial<StoryStep>) {
        edit({steps: draft.steps.map(s => s.id === id ? {...s, ...changes} : s)});
    }
    function saveFile() {
        const name = draft.title.replace(/[^a-zA-Z0-9_-]+/g, '-').slice(0, 80) || 'untitled';
        download(name + '.story.json', exportStory(draft));
    }
    async function openFile(source: File) {
        try {
            const imported = importStory(await source.text());
            if (latestStore.current !== store) throw Error('The library changed while opening the file. Choose the file again.');
            persist({selected:imported.id, files:[...book.files, {draft:imported,run:null}]});
            setMode('write'); setFileError('');
        } catch(e) { setFileError((e as Error).message); }
    }
    function begin() {
        if (issues.length || !editable) return;
        if (run && !window.confirm('Replace this rehearsal and its log with a new run of the draft?')) return;
        update({...file, run: {story: structuredClone(draft), events: []}}); setEventPlace(draft.place); setMode('rehearse');
    }
    function event(when: Trigger) {
        if (run) update({...file, run: {...run, events: [...run.events, {id: run.events.length + 1, when, place: eventPlace || run.story.place}]}});
    }
    return <main className="story-workspace">
        <header><div><h2>Story Creator</h2><p className="hint">Drafts and rehearsals save in this browser for {worldName(target)}.</p></div>
            <label>Story library<select aria-label="Story library" value={book.selected} onChange={e => {persist({...book, selected: e.target.value}); setEventPlace('');}}>
                {book.files.map(f => <option key={f.draft.id} value={f.draft.id}>{f.draft.title || 'Untitled draft'}</option>)}</select></label>
            {editable && <button onClick={() => {const d = newStory(); persist({selected: d.id, files: [...book.files, {draft: d, run: null}]}); setMode('write');}}>New story</button>}
        </header>
        {store.error && <p role="alert">{store.error} <button onClick={() => setStore(read(key))}>Reopen saved version</button></p>}
        <nav aria-label="Story workspace"><button aria-pressed={mode === 'write'} onClick={() => setMode('write')}>Write draft</button>
            <button aria-pressed={mode === 'rehearse'} onClick={() => setMode('rehearse')}>Inspect rehearsal</button>
            <button aria-pressed={mode === 'program'} onClick={() => setMode('program')}>World stories</button>
            <button onClick={openPersonal}>Personal storylines and storytellers</button></nav>
        {mode === 'program' ? <StoryProgramPanel canDirect={editable} key={key} target={target} storageKey={key + '.world-program'}/> : mode === 'write' ? <div className="story-columns"><section className="dm-panel">
            <fieldset disabled={!editable}><label>Story title<input value={draft.title} onChange={e => edit({title:e.target.value})}/></label>
            <StoryPicker world={world} roster={roster} resident={draft.resident} place={draft.place}
                onResident={resident => edit({resident})} onPlace={place => edit({place})} />
            {catalogError && <p role="status">World catalogue unavailable or incomplete. {catalogError}</p>}
            {draft.steps.map((s,i) => <section className="story-step" key={s.id}>
                <h3>{i === 0 ? 'Opening' : 'Step ' + (i + 1)}</h3>
                <label>Step name<input value={s.name} onChange={e => stepEdit(s.id, {name:e.target.value})}/></label>
                <label>Player’s journal entry<textarea value={s.journal} onChange={e => stepEdit(s.id, {journal:e.target.value})}/></label>
                <label><input type="checkbox" checked={s.ending} onChange={e => {
                    if (e.target.checked && s.exits.length && !window.confirm('Remove this step’s exits and make it an ending?')) return;
                    stepEdit(s.id, {ending:e.target.checked, exits:[]});
                }}/> Ending</label>
                {s.exits.map((exit,k) => <div className="story-exit" key={k}>
                    <label>When<select aria-label="When" value={exit.when} onChange={e => stepEdit(s.id, {exits:s.exits.map((x,n) => n === k ? {...x, when:e.target.value as Trigger} : x)})}>
                        {Object.entries(triggers).map(([v,label]) => <option value={v} key={v}>{label}</option>)}</select></label>
                    <label>Go to<select aria-label="Go to" value={exit.to} onChange={e => stepEdit(s.id, {exits:s.exits.map((x,n) => n === k ? {...x, to:e.target.value} : x)})}>
                        <option value="">Choose step</option>{draft.steps.map((t,n) => <option value={t.id} key={t.id}>{n + 1}. {t.name}</option>)}</select></label>
                    <button onClick={() => stepEdit(s.id,{exits:s.exits.filter((_,n) => n !== k)})}>Remove exit</button>
                </div>)}
                {!s.ending && s.exits.length < 3 && <button onClick={() => {
                    const when = (Object.keys(triggers) as Trigger[]).find(k => !s.exits.some(e => e.when === k))!;
                    stepEdit(s.id,{exits:[...s.exits,{when,to:''}]});
                }}>Add exit</button>}
                {i > 0 && <button onClick={() => {if (window.confirm('Remove this step? Incoming exits will need a new destination.'))
                    edit({steps:draft.steps.filter(t => t.id !== s.id).map(t => ({...t, exits:t.exits.map(x => x.to === s.id ? {...x,to:''} : x)}))});}}>Remove step</button>}
            </section>)}
            <button onClick={() => edit({steps:[...draft.steps,{id:storyId(),name:'',journal:'',ending:true,exits:[]}]})}>Add step</button></fieldset>
        </section><aside className="dm-panel"><h3>Draft checks</h3>
            {issues.length ? <ul>{issues.map((m,i) => <li key={i}>{m}</li>)}</ul> : <p>Ready to rehearse.</p>}
            {waitsForArrival.map(s => <p className="hint" key={s.id}>{s.name} waits indefinitely if nobody arrives. Add a deadline or a DM decision to give it another exit.</p>)}
            <button disabled={!editable || issues.length > 0} onClick={begin}>Start local rehearsal</button>
            <h3>Draft file</h3><button onClick={saveFile}>Export JSON</button>
            {editable && <><input type="file" hidden accept=".json,application/json" ref={fileInput} onChange={e => {
                const source = e.target.files?.[0]; e.target.value = ''; if (source) void openFile(source);
            }}/><button onClick={() => fileInput.current?.click()}>Import as new draft</button></>}
            {fileError && <p role="alert">{fileError}</p>}</aside></div>
        : !run || !view ? <section className="dm-panel"><h3>No rehearsal yet</h3><p>Complete the draft checks to start a run.</p></section>
        : <div className="story-columns"><section className="dm-panel"><h3>{run.story.title} · {view.step.name}</h3>
            <dl className="story-context">
                {run.story.resident && <><dt>Cast</dt><dd>{castName || run.story.resident}</dd></>}
                {run.story.place && <><dt>Location</dt><dd>{placeName || run.story.place}</dd></>}
            </dl>
            <p>{view.step.ending ? 'This ending has been reached.' : 'Waiting for ' + view.step.exits.map(x => triggers[x.when]).join(' or ') + '.'}</p>
            <fieldset disabled={!editable}>{!view.step.ending && <>
                {view.step.exits.some(x => x.when === 'arrival') && <label>Arrival location<select aria-label="Arrival location" value={eventPlace || run.story.place} onChange={e => setEventPlace(e.target.value)}>
                    <option value={run.story.place}>{placeName || run.story.place}</option><option value="__other_quay__">Another location</option></select></label>}
                {view.step.exits.map(x => <button key={x.when} onClick={() => event(x.when)}>{triggers[x.when]}</button>)}</>}
                {run.events.length > 0 && <button onClick={() => update({...file,run:{...run,events:[...run.events,run.events[run.events.length-1]]}})}>Replay last event</button>}
            </fieldset><h3>Run log</h3><ol>{view.log.map((m,i) => <li key={i}>{m}</li>)}</ol></section>
            <aside className="dm-panel"><h3>Player’s journal</h3>{view.journal.map((m,i) => <p key={i}>{m}</p>)}</aside></div>}
    </main>;
}
