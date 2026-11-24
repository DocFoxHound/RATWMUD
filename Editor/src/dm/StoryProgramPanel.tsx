import {useEffect, useMemo, useRef, useState} from 'react';
import {storyProgram, worldName, type Target} from './api';
import type {Project} from '../model/model.mjs';
import {download} from '../lib/api';
import {commit, editable, historyOf, namesOf, pretty, redo, refOfProblem, undo, wordingOf, type History, type Program, type Ref, type Refs} from '../lib/workbench';
import {BenchContext} from './triggers/context';
import {WordingContext} from './campaigns/Explain';
import {diffLines, migrationGroups, type MigrationReport, type Problems} from '../lib/campaigns';
import {Workbench, type FullCatalogue} from './triggers/Workbench';
import {ProblemsList} from './triggers/Problems';
import {RehearsalStrip, type Report, type Steps} from './triggers/Rehearsal';
import type {LibraryEntry} from './triggers/Library';
import {SmallQuestFlow} from './triggers/SmallQuest';

export {ProblemsList};

/** What the `catalog` op gives (tools/story_runner.catalogue). */
interface Catalog extends FullCatalogue {blank: Program; examples: Record<string, Program>; eventTypes: LibraryEntry[]}
/** A draft kept on the server (tools/story_drafts.py, migration 0073). */
interface DraftRow {id: number; name: string; owner: string; version: number; savedBy: string; savedAt: string | null; title: string; phases: number; triggers: number}
/** Which draft the text in this browser is: its id and version on the server, its name, and the text as last saved. */
interface DraftMeta {id?: number; version?: number; name: string; saved?: string; savedAt?: string | null; owner?: string}

function readMeta(key: string): DraftMeta {
    try { const v = JSON.parse(localStorage.getItem(key) ?? 'null'); return v && typeof v.name === 'string' ? v : {name: 'Untitled story'}; } catch { return {name: 'Untitled story'}; }
}
const clock = (iso: string | null | undefined) => iso ? new Date(iso).toLocaleTimeString([], {hour: '2-digit', minute: '2-digit'}) : '';

// The Story Creator's "World stories" (Docs/Design/85, section 4, Phase 4): the drafts (kept on the server for a Dungeon
// Master, many per world; a viewer's stay in this browser), undo and redo over the whole draft, the trigger workbench
// or the program as JSON ("Source"), rehearsal along the bottom, then approval and publication. A story approved and
// published here is watched and directed in the Campaigns tab (5.1).
export function StoryProgramPanel({sceneLibraryKey, world, target, storageKey, canDirect, openCampaigns}: {sceneLibraryKey: string; world: Project | null; target: Target;
    storageKey: string; canDirect: boolean; openCampaigns?: (run: string) => void}) {
    const [catalog, setCatalog] = useState<Catalog>();
    const [refs, setRefs] = useState<Refs | null>(null);
    const [hist, setHist] = useState<History>(() => { try { return historyOf(localStorage.getItem(storageKey) ?? ''); } catch { return historyOf(''); } });
    const source = hist.present;
    const [meta, setMeta] = useState<DraftMeta>(() => readMeta(storageKey + '.draft'));
    const [drafts, setDrafts] = useState<DraftRow[] | null>(null);
    const [draftsProblem, setDraftsProblem] = useState('');
    const [view, setView] = useState<'workbench' | 'source'>('workbench');
    const [report, setReport] = useState<Report>();
    const [chosenCase, setChosenCase] = useState<number | null>(null);
    const [stepping, setStepping] = useState<{steps: Steps; at: number} | null>(null);
    const [ticket, setTicket] = useState('');
    const [selected, setSelected] = useState('');
    const [published, setPublished] = useState('');
    // (Doc 86, G3: what approval warns of without refusing: a purse not yet saved, a face that has died, a variable told twice.)
    const [warnings, setWarnings] = useState<string[]>([]);
    // Approve as a new version of a running campaign (doc 85, Phase 5b): the live runs of this story, and what came of it.
    const [liveRuns, setLiveRuns] = useState<{run: string; title: string; status: string; phase: string | null; visit?: number; digest: string}[]>([]);
    const [forRun, setForRun] = useState('');
    const [asVersion, setAsVersion] = useState<{run: string; version: number; report: MigrationReport} | null>(null);
    const [search, setSearch] = useState('');
    const [places, setPlaces] = useState<{kind: string; id: string; name: string}[]>([]);
    const [resident, setResident] = useState<{resident: {id: string; name: string}}>();
    const [feedback, setFeedback] = useState('');
    const [busy, setBusy] = useState(false);
    const [problems, setProblems] = useState<Problems | null>(null);
    const [checking, setChecking] = useState(false);
    const [opening, setOpening] = useState(false);
    const [jump, setJump] = useState<{seq: number; ref: Ref}>();
    const fileInput = useRef<HTMLInputElement>(null);

    // Every problem at once, asked again half a second after the last edit (doc 85, Phase 3).
    useEffect(() => {
        let active = true;
        const timer = window.setTimeout(() => {
            let parsed: unknown;
            try { parsed = JSON.parse(source); } catch (e) {
                setChecking(false);
                setProblems({problems: [{level: 'error', text: 'The program JSON does not parse: ' + (e as Error).message, path: '', at: {kind: 'source'}}], errors: 1, warnings: 0, world: false, digest: null});
                return;
            }
            setChecking(true);
            storyProgram<Problems>(target, 'problems', {program: parsed}).then(p => { if (active) setProblems(p); })
                .catch(() => { if (active) setProblems(null); }).finally(() => { if (active) setChecking(false); });
        }, 500);
        return () => { active = false; window.clearTimeout(timer); };
    }, [source, target]);
    useEffect(() => {
        let active = true;
        storyProgram<Catalog>(target, 'catalog').then(c => {
            if (active) { setCatalog(c); setHist(h => h.present ? h : historyOf(pretty(c.blank))); }
        }).catch(e => { if (active) setFeedback(e.message); });
        storyProgram<Refs>(target, 'refs').then(r => { if (active) setRefs(r); }).catch(() => { if (active) setRefs(null); });
        return () => { active = false; };
    }, [target]);
    const listDrafts = () => {
        if (!canDirect) return;
        storyProgram<{drafts: DraftRow[]}>(target, 'draft.list').then(r => { setDrafts(r.drafts); setDraftsProblem(''); })
            .catch(e => { setDrafts(null); setDraftsProblem((e as Error).message); });
    };
    useEffect(listDrafts, [target, canDirect]); // eslint-disable-line react-hooks/exhaustive-deps
    useEffect(() => { try { localStorage.setItem(storageKey + '.draft', JSON.stringify(meta)); } catch { /* the meta is a convenience */ } }, [meta, storageKey]);

    const program = useMemo(() => { try { return JSON.parse(source) as unknown; } catch { return undefined; } }, [source]);
    const shown = editable(program) ? program : null;
    const dirty = source !== (meta.saved ?? '');
    function setText(text: string, mark?: string) {
        setHist(h => commit(h, text, mark));
        setTicket(''); setReport(undefined); setPublished(''); setStepping(null); setChosenCase(null); setWarnings([]);
        try { localStorage.setItem(storageKey, text); } catch { setFeedback('Browser storage is full or unavailable. Save the draft or export a copy to keep it.'); }
    }
    const edit = (p: Program, mark?: string) => setText(pretty(p), mark);
    function replaceWith(text: string, next: DraftMeta, ask = true) {
        if (ask && source && dirty && !window.confirm('This draft has changes that are not saved. Replace it anyway? (Undo brings it back.)')) return false;
        setText(text); setMeta(next); setFeedback(''); return true;
    }
    async function perform(work: () => Promise<void>) {
        setBusy(true); setFeedback('');
        try { await work(); } catch (e) { setFeedback(e instanceof Error ? e.message : String(e)); } finally { setBusy(false); }
    }
    function step(direction: 'undo' | 'redo') {
        setHist(h => {
            const next = direction === 'undo' ? undo(h) : redo(h);
            try { localStorage.setItem(storageKey, next.present); } catch { /* as setText */ }
            return next;
        });
        setReport(undefined); setTicket(''); setStepping(null);
    }
    useEffect(() => {
        const onKey = (e: KeyboardEvent) => {
            const typing = e.target instanceof HTMLInputElement || e.target instanceof HTMLTextAreaElement;
            if (!(e.ctrlKey || e.metaKey) || typing) return;
            if (e.key.toLowerCase() === 'z' && !e.shiftKey) { e.preventDefault(); step('undo'); }
            else if ((e.key.toLowerCase() === 'z' && e.shiftKey) || e.key.toLowerCase() === 'y') { e.preventDefault(); step('redo'); }
            else if (e.key.toLowerCase() === 's' && canDirect) { e.preventDefault(); void save(false); }
        };
        window.addEventListener('keydown', onKey);
        return () => window.removeEventListener('keydown', onKey);
    });
    async function save(asNew: boolean) {
        if (!canDirect) return;
        let name = meta.name;
        if (asNew || !meta.id) {
            const asked = window.prompt('A name for this draft', asNew ? meta.name + ' (copy)' : (shown?.title || meta.name));
            if (!asked?.trim()) return;
            name = asked.trim();
        }
        await perform(async () => {
            const parsed = JSON.parse(source);
            const r = await storyProgram<{id: number; version: number; savedAt: string; name: string}>(target, 'draft.save',
                {name, program: parsed, ...(asNew || !meta.id ? {} : {id: meta.id, version: meta.version})});
            setMeta({id: r.id, version: r.version, name: r.name, saved: source, savedAt: r.savedAt, owner: meta.owner});
            listDrafts();
        });
    }
    async function openDraft(id: number) {
        await perform(async () => {
            const d = await storyProgram<DraftRow & {program: Program}>(target, 'draft.open', {id});
            const text = pretty(d.program);
            if (replaceWith(text, {id: d.id, version: d.version, name: d.name, saved: text, savedAt: d.savedAt, owner: d.owner})) setView(editable(d.program) ? 'workbench' : 'source');
        });
    }
    async function deleteDraft() {
        if (!meta.id || !window.confirm(`Delete the draft "${meta.name}" from the server, with its saved versions? Your copy in this browser stays.`)) return;
        await perform(async () => { await storyProgram(target, 'draft.delete', {id: meta.id}); setMeta({name: meta.name}); listDrafts(); });
    }
    async function startFromLibrary(id: string) {
        if (!window.confirm('Start a new small story from this library event? It replaces the draft in the workbench (save it first to keep it).')) return;
        await perform(async () => {
            const p = (await storyProgram<{program: Program}>(target, 'draft-event', {eventType: id})).program;
            replaceWith(pretty(p), {name: p.title}, false); setOpening(false);
        });
    }
    const reviewed = report?.passed && !report.uncovered?.length && program;
    const storyId = (program as Program | undefined)?.id;
    useEffect(() => {
        // (The live runs of this story, once it rehearses clean: what "as a new version of" may name.)
        if (!canDirect || !reviewed || !storyId) { setLiveRuns([]); return; }
        let active = true;
        storyProgram<{runs: {run: string; title: string; status: string; phase: string | null; visit?: number; digest: string; storyId?: string}[]}>(target, 'status')
            .then(o => { if (active) setLiveRuns(o.runs.filter(r => r.storyId === storyId && ['running', 'paused', 'held'].includes(r.status))); })
            .catch(() => { if (active) setLiveRuns([]); });
        return () => { active = false; };
    }, [canDirect, reviewed, storyId, target]);
    const status = !canDirect ? 'Drafts stay in this browser' : meta.id ? (dirty ? 'Unsaved changes' : `Saved ${clock(meta.savedAt)} · version ${meta.version}`) : dirty ? 'Not saved on the server yet' : '';
    const phases = Array.isArray((program as Program | undefined)?.phases) ? (program as Program).phases.map(p => String(p?.id)) : [];
    return <div className="story-columns program-workspace"><section className="dm-panel">
        <div className="wb-file">
            <h3>{shown?.title || meta.name || 'World story'}</h3>
            <span className={'wb-saved' + (dirty ? ' dirty' : '')} role="status">{status}</span>
        </div>
        <fieldset disabled={busy} className="wb-filebar">
            {canDirect && <label>Drafts<select value={meta.id ?? ''} onChange={e => { if (e.target.value) void openDraft(Number(e.target.value)); }} aria-label="Open a draft">
                <option value="">{drafts ? (drafts.length ? 'Open a draft…' : 'No drafts on the server yet') : 'Drafts…'}</option>
                {(drafts ?? []).map(d => <option key={d.id} value={d.id}>{d.name} · {d.owner.split('#')[0]} · {clock(d.savedAt)} · v{d.version}</option>)}</select></label>}
            {canDirect && <button type="button" disabled={!program} onClick={() => void save(false)} title="Ctrl+S">{meta.id ? 'Save' : 'Save on the server…'}</button>}
            {canDirect && meta.id && <button type="button" disabled={!program} onClick={() => void save(true)}>Save as new…</button>}
            {canDirect && meta.id && <button type="button" onClick={() => void deleteDraft()}>Delete draft…</button>}
            <button type="button" onClick={() => setOpening(!opening)} aria-pressed={opening}>New…</button>
            <button type="button" onClick={() => fileInput.current?.click()}>Import…</button>
            <button type="button" disabled={!source} onClick={() => download((shown?.id || 'world-story') + '.json', source)}>Export</button>
            <span className="wb-sep"/>
            <button type="button" disabled={!hist.past.length} onClick={() => step('undo')} title="Ctrl+Z" aria-label="Undo">↶ Undo</button>
            <button type="button" disabled={!hist.future.length} onClick={() => step('redo')} title="Ctrl+Shift+Z" aria-label="Redo">↷ Redo</button>
            <span className="wb-sep"/>
            <span className="wb-views" role="group" aria-label="View"><button type="button" aria-pressed={view === 'workbench'} onClick={() => setView('workbench')}>Workbench</button>
                <button type="button" aria-pressed={view === 'source'} onClick={() => setView('source')}>Source</button></span>
            <input ref={fileInput} type="file" hidden accept=".json,application/json" onChange={e => {
                const f = e.target.files?.[0]; e.target.value = '';
                if (f) void perform(async () => {
                    if (f.size > 1024 * 1024) throw Error('Choose a program under 1 MiB.');
                    const text = await f.text();
                    let parsed: unknown;
                    try { parsed = JSON.parse(text); } catch { parsed = undefined; }
                    if (replaceWith(text, {name: (parsed as Program | undefined)?.title || f.name.replace(/\.json$/, '')})) setView(editable(parsed) ? 'workbench' : 'source');
                });
            }}/>
        </fieldset>
        {draftsProblem && canDirect && <p className="hint">{draftsProblem}</p>}
        {opening && catalog && <NewStory catalog={catalog} refs={refs} close={() => setOpening(false)} start={id => void startFromLibrary(id)}
            pick={p => { if (replaceWith(pretty(p), {name: p.title})) { setOpening(false); setView('workbench'); } }}/>}
        {!catalog && <p className="hint">Loading the trigger catalogue…</p>}
        {catalog && view === 'workbench' && shown && <Workbench key={meta.id ?? 'local'} program={shown} edit={edit} catalog={catalog} refs={refs} world={world} target={target}
            sceneLibraryKey={sceneLibraryKey} templatesKey={storageKey + '.templates'} problems={problems} checking={checking} report={report}
            chosenCase={chosenCase} stepping={stepping} startFromLibrary={id => void startFromLibrary(id)} jumpTo={jump}/>}
        {catalog && (view === 'source' || !shown) && <section className="wb-source">
            {!shown && program !== undefined && <p className="hint" role="status">The workbench cannot show this draft as it is: fix it here (the problems below say where), and it comes back. Nothing is lost.</p>}
            {program === undefined && source && <p className="hint" role="status">This is not JSON yet: the workbench comes back once it parses.</p>}
            {view === 'source' && shown && <p className="hint">The whole program as JSON: for what the workbench cannot yet do. Edits here are undone with the rest.</p>}
            {!shown && <ProblemsList found={problems} phases={phases} asking={checking}
                go={editable(program) ? pr => { const r = refOfProblem(program, pr.at); if (r) { setView('workbench'); setJump(j => ({seq: (j?.seq ?? 0) + 1, ref: r})); } } : undefined}/>}
            <label>Program JSON<textarea rows={24} spellCheck={false} value={source} onChange={e => setText(e.target.value, 'source')}/></label>
        </section>}
        {(() => {
            const strip = <RehearsalStrip report={report} program={shown} target={target} busy={busy} chosen={chosenCase} choose={setChosenCase}
                stepping={catalog && shown ? stepping : null} setStepping={setStepping}
                rehearse={() => void perform(async () => { setReport(await storyProgram<Report>(target, 'rehearse', {program: JSON.parse(source)})); setChosenCase(null); setStepping(null); })}/>;
            if (!catalog || !shown) return strip;
            const names = namesOf(refs, shown);
            return <BenchContext.Provider value={{cat: catalog.catalogue, refs, names, program: shown, world, edit, problems: {}}}>
                <WordingContext.Provider value={wordingOf(catalog.catalogue, Object.assign({}, ...Object.values(names)))}>{strip}</WordingContext.Provider></BenchContext.Provider>;
        })()}
        {report && <section className="wb-approval"><h4>Effects for approval</h4>{report.effects.length ? <ul>{report.effects.map((e, i) => <li key={i}><code>{JSON.stringify(e)}</code></li>)}</ul> : <p>Journal entries only.</p>}
            {report.voices?.map(v => <p key={v.phase + v.npc}>Optional guest voice · {v.npc} during {v.phase}</p>)}
            {shown && <p>Audience · {shown.audience.kind === 'active' ? 'Players active in the last ' + shown.audience.days + ' days'
                : shown.audience.kind === 'open' ? `Open: nobody at first; a wolf joins by taking a quest or being taken in (at most ${shown.audience.cap ?? 5000})`
                : shown.audience.kind === 'everyone' ? `Everyone: players active in the last ${shown.audience.days} days, and each who signs in after (at most ${shown.audience.cap ?? 5000})`
                : shown.audience.players?.join(', ')}</p>}
            {shown?.quests?.length ? <p>Quests · {shown.quests.map(q => q.title).join(', ')}</p> : null}
            <details><summary>Version fingerprint</summary><code>{report.digest}</code></details>
            <button disabled={!canDirect || !reviewed || busy || !!ticket} onClick={() => void perform(async () => {
                const r = await storyProgram<{ticket: string; run: string; warnings?: string[]}>(target, 'approve', {program, digest: report.digest});
                setTicket(r.ticket); setSelected(r.run); setWarnings(r.warnings ?? []);
            })}>Approve this version for {worldName(target)}</button>
            {liveRuns.length > 0 && <div className="wb-for-run">
                <span>or as a new version of a running campaign:</span>
                <select value={forRun} onChange={e => setForRun(e.target.value)} aria-label="Running campaign">
                    <option value="">choose a run…</option>
                    {liveRuns.map(r => <option key={r.run} value={r.run} disabled={r.digest === report.digest}>{r.title} · {r.status} · {r.phase}{r.visit ? ` (visit ${r.visit})` : ''}
                        {r.digest === report.digest ? ' · on this version' : ''}</option>)}</select>
                <button disabled={!canDirect || !reviewed || busy || !forRun} onClick={() => void perform(async () => {
                    const r = await storyProgram<{run: string; version: number; report: MigrationReport; warnings?: string[]}>(target, 'approve', {program, digest: report.digest, forRun});
                    setAsVersion(r); setWarnings(r.warnings ?? []);
                })}>Approve as its new version</button></div>}
            {asVersion && <div className="wb-as-version" role="status">
                <p>Approved as a new version of <code>{asVersion.run}</code>: nothing has moved yet. Move the run onto it from Campaigns (it asks first, with a reason).
                    {openCampaigns && <> <button onClick={() => openCampaigns(asVersion.run)}>Open in Campaigns</button></>}</p>
                {asVersion.report.blocked && <p className="hint">{asVersion.report.blocked}</p>}
                <ul>{diffLines(asVersion.report.diff).map((l, i) => <li key={'d' + i}>{l}</li>)}</ul>
                {!asVersion.report.blocked && <p className="hint">For the run as it stands: {migrationGroups(asVersion.report).map(g => `${g.lines.length} ${g.group}`).join(', ')}.</p>}
            </div>}
            {warnings.length > 0 && <div className="wb-warnings" role="status"><p>Approved, with warnings:</p><ul>{warnings.map((w, i) => <li key={i}>{w}</li>)}</ul></div>}</section>}
        {ticket && <fieldset disabled={busy}><label>Publication ticket<input readOnly value={ticket} onFocus={e => e.target.select()}/></label>
            <button onClick={() => void perform(async () => { const r = await storyProgram<{run: string}>(target, 'publish', {ticket}); setTicket(''); setPublished(r.run); })}>Publish approved story</button>
            {openCampaigns && selected && <button onClick={() => openCampaigns(selected)}>Open in Campaigns</button>}</fieldset>}
        {published && <p className="hint">Published. {openCampaigns && <button onClick={() => openCampaigns(published)}>Open in Campaigns</button>}</p>}
        {feedback && <p role="alert">{feedback}</p>}
    </section>{canDirect && <aside className="dm-panel"><h3>For an author</h3>
        <p className="hint">The stories running in {worldName(target)} are watched in the Campaigns tab.
            {openCampaigns && <> <button onClick={() => openCampaigns('')}>Open Campaigns</button></>}</p>
        <details><summary>World references for an author</summary>
            <label>Search Atlas names and IDs<input value={search} onChange={e => setSearch(e.target.value)}/></label>
            <button disabled={busy} onClick={() => void perform(async () => { const r = await storyProgram<{matches: typeof places}>(target, 'atlas.search', {query: search}); setPlaces(r.matches); setResident(undefined); })}>Find references</button>
            <ul>{places.map(p => <li key={p.kind + p.id}>{p.name} · <code>{p.id}</code> · {p.kind}
                {p.kind === 'npc' && <button disabled={busy} onClick={() => void perform(async () => setResident(await storyProgram(target, 'npc.context', {npc: p.id})))}>Read resident history</button>}</li>)}</ul>
            {resident && <pre>{pretty(resident)}</pre>}
            {!!places.length && <button onClick={() => download('story-context.json', pretty({matches: places, residents: resident ? [resident] : []}))}>Export author context</button>}
        </details>
    </aside>}</div>;
}

/** "New…" (doc 85, 4.4): a small quest (Phase 6), a small story from one of the 100 library events, a campaign (blank or one of the 8 examples). */
export function NewStory({catalog, refs, close, start, pick, questing = false}: {catalog: Catalog; refs: Refs | null; close: () => void; start: (id: string) => void;
    pick: (p: Program) => void; questing?: boolean}) {
    const [q, setQ] = useState('');
    const [quest, setQuest] = useState(questing);
    const words = q.toLowerCase().split(/\s+/).filter(Boolean);
    const found = catalog.eventTypes.filter(e => words.every(w => (e.title + ' ' + e.category + ' ' + e.scale).toLowerCase().includes(w)));
    return <div className="wb-new" role="dialog" aria-label="New story">
        <header><strong>New…</strong><button type="button" className="wb-close" onClick={close} aria-label="Close">×</button></header>
        <section className="wb-new-quest"><h5>A small quest</h5>
            <p className="hint">One quest, anyone who asks may take it: who gives it, what to do, the reward.</p>
            {quest ? <SmallQuestFlow cat={catalog.catalogue} refs={refs} pick={pick} close={() => setQuest(false)} library={catalog.eventTypes}/>
                : <button type="button" onClick={() => setQuest(true)}>A small quest…</button>}</section>
        <div className="wb-new-grid">
            <section><h5>A small story</h5><p className="hint">One of the 100 library events, ready to rehearse: its opening, its objectives as a mission, its outcome.</p>
                <input type="search" placeholder="Find an event" value={q} onChange={e => setQ(e.target.value)} aria-label="Find a library event"/>
                <div className="wb-options">{found.slice(0, 40).map(e => <button key={e.id} type="button" onClick={() => start(e.id)}><span>{e.title}</span><small>{e.category} · {e.scale}</small></button>)}</div></section>
            <section><h5>A campaign</h5><p className="hint">Phases, triggers, missions and scenes.</p>
                <button type="button" onClick={() => pick(structuredClone(catalog.blank))}>A blank campaign</button>
                <div className="wb-options">{Object.entries(catalog.examples).map(([id, p]) => <button key={id} type="button" onClick={() => pick(structuredClone(p))}>
                    <span>{p.title}</span><small>{p.phases.length} phases · from the examples</small></button>)}</div></section>
        </div></div>;
}
