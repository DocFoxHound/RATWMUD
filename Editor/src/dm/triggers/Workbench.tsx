// The Story Creator's trigger workbench, in the Warcraft III manner (Docs/Design/85-story-triggers-and-campaign-control.md,
// section 4, Phase 4). The tree on the left (campaign triggers, each phase with its folders, Exits and missions), the
// selected thing in the middle as sentences with clickable typed parameters, and on the right what they refer to:
// the Problems list, search, the story's signals and counters with every use, and its regions. Rehearsal is along the
// bottom (Rehearsal.tsx, held by StoryProgramPanel with undo and redo, the drafts and approval).
import {useEffect, useMemo, useRef, useState} from 'react';
import type {Project} from '../../model/model.mjs';
import type {Target} from '../api';
import type {Problem, Problems} from '../../lib/campaigns';
import {
    fragmentOf, insertTriggers, namesOf, pasteFragment, readFragment, refOfProblem, removeTriggers, renameSignal,
    newQuest, renameTrigger, sameRef, search, signalsOf, takenIds, traceKey, triggersOf, unique, usesOf, withQuests, type Catalogue, type Fragment, type Mission,
    type Program, type Ref, type Refs,
} from '../../lib/workbench';
import {QuestEditor, QuestList} from './QuestEditor';
import {StoryMissions} from '../StoryMissions';
import {StoryRegions, type Region} from '../StoryRegions';
import {EmbeddedScenes} from '../EmbeddedScenes';
import {StoryRehearsalEditor} from '../StoryRehearsalEditor';
import type {Scene} from '../scenes';
import {BenchContext, type Bench} from './context';
import {ExitView, PhaseView, SettingsView} from './PhaseViews';
import {ProblemsList} from './Problems';
import {TriggerTree, type TreeActions} from './TriggerTree';
import {TriggerView} from './TriggerView';
import {LibraryPanel, loadTemplates, saveTemplates, type LibraryEntry} from './Library';
import {VariablesPanel} from './Variables';
import type {Report, Steps} from './Rehearsal';

export interface FullCatalogue {
    catalogue: Catalogue; eventTypes: LibraryEntry[]; missionEvents: string[]; missionMeasures: Record<string, string[]>; events: string[];
    sceneExamples?: Record<string, Scene>;
}

const refKey = (r: Ref) => JSON.stringify(r);

/** Region ids are named by conditions, effects, region events' filters and rehearsal facts: rewritten when a region is renamed. */
function renameRegion(p: Program, from: string, to: string): Program {
    const copy = structuredClone(p);
    const visit = (v: unknown) => {
        if (!v || typeof v !== 'object') return;
        if (Array.isArray(v)) { v.forEach(visit); return; }
        const o = v as Record<string, unknown>;
        if (o.region === from) o.region = to;
        if ((o.on === 'region entered' || o.on === 'region left' || o.kind === 'region entered' || o.kind === 'region left')) {
            if (o.target === from) o.target = to;
            const m = o.match as Record<string, unknown> | undefined;
            if (m?.target === from) m.target = to;
        }
        Object.values(o).forEach(visit);
    };
    visit(copy);
    return copy;
}

/** Problems by place, for the red slots and the tree's marks. */
function problemKeys(p: Program, list: Problem[]): {byKey: Record<string, string[]>; refs: Set<string>} {
    const byKey: Record<string, string[]> = {};
    const refs = new Set<string>();
    const add = (k: string, text: string) => { (byKey[k] ??= []).push(text); };
    for (const pr of list) {
        if (pr.level !== 'error' && !pr.world) continue;
        const at = pr.at ?? {kind: 'program'};
        const ref = refOfProblem(p, at);
        if (ref) refs.add(refKey(ref));
        if (ref?.kind === 'trigger') {
            const head = `t:${ref.phase ?? 'c'}:${ref.index}`;
            // (Version 2: a nested action's part is its whole path, "actions[2].then[0]", as the nested lists look it up.)
            const action = /^actions\[\d+\]/.test(at.part ?? '');
            add(action ? `${head}:${at.part}` : at.part?.startsWith('if') ? head + ':if' : head, pr.text);
        } else if (ref?.kind === 'exit') add(`x:${ref.phase}:${ref.index}`, pr.text);
        // (Phase 6: a quest's problems, by its part when the path names one: "q:2:rewards[0]", "q:2:objectives[1]".)
        else if (ref?.kind === 'quest') {
            const part = /^quests\[\d+\]\.(.+)$/.exec(pr.path ?? '')?.[1] ?? at.part;
            add(part ? `q:${ref.index}:${part}` : `q:${ref.index}`, pr.text);
        }
        else if (at.kind === 'effect' && typeof at.phase === 'number') add(`e:${at.phase}:${at.index ?? 0}`, pr.text);
    }
    return {byKey, refs};
}

export function Workbench({program, edit, catalog, refs, world, target, sceneLibraryKey, templatesKey, problems, checking, report, chosenCase,
    stepping, startFromLibrary, readOnly = false, jumpTo, initial}: {
    program: Program; edit: (p: Program, mark?: string) => void; catalog: FullCatalogue; refs: Refs | null; world: Project | null; target: Target;
    sceneLibraryKey: string; templatesKey: string; problems: Problems | null; checking: boolean; report?: Report; chosenCase: number | null;
    stepping: {steps: Steps; at: number} | null; startFromLibrary: (id: string) => void; readOnly?: boolean; jumpTo?: {seq: number; ref: Ref};
    /** What is selected first (the render smoke selects each trigger and exit in turn). */
    initial?: Ref}) {
    const cat = catalog.catalogue;
    const [selected, setSelected] = useState<Ref | null>(() => initial ?? (program.triggers?.length ? {kind: 'trigger', phase: null, index: 0}
        : program.phases[0]?.triggers?.length ? {kind: 'trigger', phase: 0, index: 0} : {kind: 'phase', phase: 0}));
    const [picked, setPicked] = useState<Ref[]>([]);
    const [clip, setClip] = useState<Fragment | null>(null);
    const [query, setQuery] = useState('');
    const [use, setUse] = useState<{type: string; id: string; scope?: string} | null>(null);
    const [library, setLibrary] = useState(false);
    const [templates, setTemplatesState] = useState<Fragment[]>(() => loadTemplates(templatesKey));
    const [note, setNote] = useState('');
    const find = useRef<HTMLInputElement>(null);
    const names = useMemo(() => namesOf(refs, program), [refs, program]);
    const {byKey, refs: badRefs} = useMemo(() => problemKeys(program, problems?.problems ?? []), [program, problems]);
    const bench: Bench = {cat, refs, names, program, world, edit: readOnly ? () => {} : edit, problems: byKey};
    useEffect(() => { if (jumpTo) { setSelected(jumpTo.ref); setPicked([]); } }, [jumpTo?.seq]); // eslint-disable-line react-hooks/exhaustive-deps

    // A selection that no longer exists (an undo, a deletion) falls back to its phase or the settings.
    const valid = (r: Ref | null): r is Ref => {
        if (!r) return false;
        if (r.kind === 'trigger') return r.index < triggersOf(program, r.phase).length && (r.phase === null || r.phase < program.phases.length);
        if (r.kind === 'exit') return r.phase < program.phases.length && r.index < program.phases[r.phase].exits.length;
        if (r.kind === 'phase' || r.kind === 'missions' || r.kind === 'entry') return r.phase < program.phases.length;
        if (r.kind === 'quest') return r.index < (program.quests?.length ?? 0);
        return true;
    };
    const current: Ref = valid(selected) ? selected : {kind: 'settings'};
    const scopeOf = (r: Ref | {scope: number | null}): number | null => 'scope' in r ? r.scope : r.kind === 'trigger' ? r.phase
        : (r.kind === 'phase' || r.kind === 'exit' || r.kind === 'missions' || r.kind === 'entry') ? r.phase : null;

    function select(r: Ref, how?: 'add' | 'range') {
        if (how === 'add' && r.kind === 'trigger') {
            setPicked(p => p.some(x => sameRef(x, r)) ? p.filter(x => !sameRef(x, r)) : [...p.filter(x => x.kind === 'trigger' && x.phase === r.phase), r]);
        } else if (how === 'range' && r.kind === 'trigger' && current.kind === 'trigger' && current.phase === r.phase) {
            const [a, b] = [Math.min(current.index, r.index), Math.max(current.index, r.index)];
            setPicked(Array.from({length: b - a + 1}, (_, i) => ({kind: 'trigger', phase: r.phase, index: a + i}) as Ref));
            return;
        } else setPicked(r.kind === 'trigger' ? [r] : []);
        setSelected(r);
    }
    const chosen = () => (picked.length ? picked : [current]).filter(r => r.kind === 'trigger');

    const actions: TreeActions = {
        canPaste: !!clip && !readOnly,
        duplicate: refs_ => {
            const list = refs_.filter(r => r.kind === 'trigger') as (Ref & {kind: 'trigger'})[];
            if (!list.length) return;
            const scope = list[0].phase, last = Math.max(...list.map(r => r.index));
            const {program: next, indexes} = insertTriggers(program, scope, last + 1, fragmentOf(program, list).triggers);
            edit(next); select({kind: 'trigger', phase: scope, index: indexes[0]});
        },
        copy: (refs_, text) => {
            const f = fragmentOf(program, refs_);
            if (!f.triggers.length) return;
            setClip(f);
            const words = JSON.stringify(f, null, 2);
            if (text) navigator.clipboard?.writeText(words).then(() => setNote('Copied as text: paste it into another story, a file or a message.'),
                () => setNote('The browser did not allow copying to the clipboard.'));
            else setNote(`Copied ${f.triggers.length} trigger${f.triggers.length === 1 ? '' : 's'}.`);
        },
        cut: refs_ => { actions.copy(refs_); edit(removeTriggers(program, refs_)); setPicked([]); },
        paste: at => {
            if (!clip) return;
            const scope = scopeOf(at);
            const after = 'kind' in at && at.kind === 'trigger' ? at.index + 1 : triggersOf(program, scope).length;
            const folder = 'kind' in at && at.kind === 'trigger' ? triggersOf(program, scope)[at.index]?.folder ?? null : 'folder' in at ? at.folder : undefined;
            const {program: next, indexes, regions} = pasteFragment(program, clip, scope, after, folder);
            edit(next); select({kind: 'trigger', phase: scope, index: indexes[0]});
            setNote(`Pasted ${indexes.length} trigger${indexes.length === 1 ? '' : 's'}${regions.length ? `, with the region${regions.length === 1 ? '' : 's'} ${regions.join(', ')}` : ''}.`);
        },
        remove: refs_ => {
            const list = refs_.filter(r => r.kind === 'trigger');
            if (!list.length || !window.confirm(`Delete ${list.length === 1 ? 'this trigger' : list.length + ' triggers'}? (Undo brings ${list.length === 1 ? 'it' : 'them'} back.)`)) return;
            edit(removeTriggers(program, list)); setPicked([]);
        },
        rename: r => {
            const t = r.kind === 'trigger' ? triggersOf(program, r.phase)[r.index] : null;
            if (!t) return;
            const to = window.prompt('Rename the trigger (rehearsal cases that expect it by name follow)', t.id)?.trim();
            if (!to || to === t.id) return;
            const taken = takenIds(program, r.kind === 'trigger' ? r.phase : null);
            if (taken.has(to)) { setNote(`"${to}" is taken in this phase.`); return; }
            edit(renameTrigger(program, r, to));
        },
        template: refs_ => {
            const title = window.prompt('A name for this template');
            if (!title?.trim()) return;
            const f = fragmentOf(program, refs_, title.trim());
            const next = [...templates, f];
            setTemplatesState(next);
            setNote(saveTemplates(templatesKey, next) ? `Saved the template "${title.trim()}" (the Library lists it).` : 'Browser storage is unavailable: export the template instead.');
        },
        newTrigger: (scope, folder) => {
            const t = {id: unique('trigger', takenIds(program, scope)), on: 'conversation', audience: 'participant', actions: [{do: 'emit' as const, signal: 'next'}],
                ...(folder ? {folder} : {})};
            const at = folder ? triggersOf(program, scope).reduce((end, x, i) => x.folder === folder ? i + 1 : end, triggersOf(program, scope).length) : triggersOf(program, scope).length;
            const {program: next, indexes} = insertTriggers(program, scope, at, [t]);
            edit(next); select({kind: 'trigger', phase: scope, index: indexes[0]});
        },
        newPhase: () => {
            let n = program.phases.length + 1;
            while (program.phases.some(p => p.id === 'phase-' + n)) n++;
            edit({...program, phases: [...program.phases, {id: 'phase-' + n, journal: '', effects: [], exits: []}]});
            select({kind: 'phase', phase: program.phases.length});
        },
        newQuest: () => {
            const quests = program.quests ?? [];
            edit(withQuests(program, [...quests, newQuest(program)])); select({kind: 'quest', index: quests.length});
        },
        newExit: phase => {
            const ph = program.phases[phase];
            const to = program.phases[(phase + 1) % program.phases.length].id;
            edit({...program, phases: program.phases.map((p, i) => i === phase ? {...p, exits: [...p.exits, {on: 'conversation', to}]} : p)});
            select({kind: 'exit', phase, index: ph.exits.length});
        },
    };

    function onKey(e: React.KeyboardEvent) {
        const typing = e.target instanceof HTMLInputElement || e.target instanceof HTMLTextAreaElement || e.target instanceof HTMLSelectElement;
        const mod = e.ctrlKey || e.metaKey;
        if (mod && e.key.toLowerCase() === 'f') { e.preventDefault(); find.current?.focus(); find.current?.select(); return; }
        if (typing || readOnly) return;
        const list = chosen();
        if (mod && e.key.toLowerCase() === 'c' && list.length) { e.preventDefault(); actions.copy(list); }
        else if (mod && e.key.toLowerCase() === 'x' && list.length) { e.preventDefault(); actions.cut(list); }
        else if (mod && e.key.toLowerCase() === 'v' && clip) { e.preventDefault(); actions.paste(current); }
        else if (mod && e.key.toLowerCase() === 'd' && list.length) { e.preventDefault(); actions.duplicate(list); }
        else if ((e.key === 'Delete') && list.length) { e.preventDefault(); actions.remove(list); }
        else if (e.key === 'F2' && current.kind === 'trigger') { e.preventDefault(); actions.rename(current); }
    }

    // The trace in the tree: the chosen case's firings, or while stepping those up to the step shown.
    const counts = useMemo(() => {
        const firings = stepping ? stepping.steps.steps.slice(0, stepping.at + 1).flatMap(s => s.fired) : chosenCase !== null ? report?.tests[chosenCase]?.triggers ?? [] : null;
        if (!firings) return null;
        const out: Record<string, number> = {};
        for (const f of firings) { const k = (f.scope === 'campaign' ? 'campaign' : f.phase) + '/' + f.trigger; out[k] = (out[k] ?? 0) + 1; }
        return out;
    }, [stepping, chosenCase, report]);
    const allCounts = useMemo(() => {
        if (!report) return null;
        const out: Record<string, number> = {};
        for (const t of report.tests) for (const f of t.triggers ?? []) { const k = (f.scope === 'campaign' ? 'campaign' : f.phase) + '/' + f.trigger; out[k] = (out[k] ?? 0) + 1; }
        return out;
    }, [report]);

    const found = query ? search(program, cat, names, query) : [];
    const signals = signalsOf(program);
    const phaseOfSel = scopeOf(current);
    const middle = (() => {
        switch (current.kind) {
            case 'trigger': return <TriggerView key={refKey(current)} at={current} select={select} fired={allCounts ? allCounts[traceKey(program, current)] ?? 0 : undefined}/>;
            case 'exit': return <ExitView key={refKey(current)} at={current}/>;
            case 'phase': case 'entry': return <PhaseView key={refKey(current)} at={current} select={select}/>;
            case 'missions': {
                const ph = program.phases[current.phase];
                return <article className="wb-trigger"><h4>Missions of {ph.id}</h4>
                    <StoryMissions events={catalog.eventTypes} triggers={catalog.missionEvents} measures={catalog.missionMeasures} missions={ph.missions ?? []} start={startFromLibrary}
                        change={missions => edit({...program, phases: program.phases.map((p, i) => i === current.phase ? {...p, missions,
                            exits: p.exits.map(x => x.on === 'missions completed' ? {...x, missions: x.missions?.filter(id => missions.some(m => m.id === id))} : x)} : p)})}/></article>;
            }
            case 'settings': return <SettingsView/>;
            case 'quests': return <QuestList select={select} add={() => actions.newQuest?.()}/>;
            case 'quest': return <QuestEditor key={refKey(current)} index={current.index} select={select}/>;
            case 'regions': return <article className="wb-trigger"><StoryRegions world={world} regions={program.regions ?? []} used={id => usesOf(program, {type: 'region', id}).length > 0}
                change={(next: Region[], rename?: [string, string]) => edit({...(rename ? renameRegion(program, rename[0], rename[1]) : program), regions: next})}/>
                <p className="hint">Regions are rectangles on the map that triggers test ("the event happened inside") and that area effects act on.</p></article>;
            case 'scenes': return <article className="wb-trigger"><EmbeddedScenes target={target} libraryKey={sceneLibraryKey} program={program} examples={catalog.sceneExamples ?? {}} edit={p => edit(p)}/></article>;
            case 'tests': return <article className="wb-trigger"><StoryRehearsalEditor regions={program.regions ?? []} cases={program.tests} events={catalog.events}
                phases={program.phases.map(p => p.id)} change={tests => edit({...program, tests}, 'tests')}/></article>;
        }
    })();
    const goProblem = (p: Problem) => { const r = refOfProblem(program, p.at); if (r) select(r); };
    return <BenchContext.Provider value={bench}>
        <div className={'wb' + (readOnly ? ' read-only' : '')} onKeyDown={onKey}>
            <div className="wb-bar">
                <input ref={find} type="search" placeholder="Find (Ctrl+F): ids, sentences, words, names…" value={query} onChange={e => setQuery(e.target.value)} aria-label="Find in the story"/>
                <button type="button" onClick={() => setLibrary(!library)} aria-pressed={library}>Library…</button>
                {clip && <small className="hint">clipboard: {clip.triggers.length} trigger{clip.triggers.length === 1 ? '' : 's'}</small>}
                <button type="button" onClick={async () => {
                    const text = window.prompt('Paste a trigger fragment (the text "Copy as text" gives)');
                    if (!text) return;
                    try { setClip(readFragment(text)); setNote('Ready to paste: choose where, then Paste.'); } catch (err) { setNote((err as Error).message); }
                }}>Paste text…</button>
                {note && <small className="wb-note-line" role="status">{note}</small>}
            </div>
            {library && <LibraryPanel entries={catalog.eventTypes} templates={templates} canEdit={!readOnly}
                setTemplates={t => { setTemplatesState(t); saveTemplates(templatesKey, t); }} close={() => setLibrary(false)}
                phaseId={phaseOfSel === null ? null : program.phases[phaseOfSel]?.id ?? null} start={startFromLibrary}
                paste={f => {
                    const scope = phaseOfSel;
                    const {program: next, indexes, regions} = pasteFragment(program, f, scope, triggersOf(program, scope).length);
                    edit(next); select({kind: 'trigger', phase: scope, index: indexes[0]});
                    setNote(`Added ${indexes.length} trigger${indexes.length === 1 ? '' : 's'} from "${f.title ?? 'the library'}"${regions.length ? ' and the regions ' + regions.join(', ') : ''}. Library triggers send signals: give them actions of your own.`);
                }}
                addMission={(m: Mission) => {
                    if (phaseOfSel === null) return;
                    const ph = program.phases[phaseOfSel];
                    let id = m.id;
                    for (let n = 2; (ph.missions ?? []).some(x => x.id === id); n++) id = m.id + '-' + n;
                    edit({...program, phases: program.phases.map((p, i) => i === phaseOfSel ? {...p, missions: [...(p.missions ?? []), {...m, id}]} : p)});
                    select({kind: 'missions', phase: phaseOfSel}); setNote(`Added the mission "${m.title}" to ${ph.id}.`);
                }}/>}
            <div className="wb-columns">
                <TriggerTree selected={current} picked={picked} select={select} actions={actions} counts={counts ?? allCounts}
                    trace={counts ? new Set(Object.keys(counts)) : null} problemRefs={badRefs}/>
                <main className="wb-middle">{middle}</main>
                <aside className="wb-side">
                    {query && <section className="wb-found"><h4>Found · {found.length}</h4>{found.length ? <ul>{found.slice(0, 40).map((f, i) => <li key={i}>
                        <button type="button" onClick={() => select(f.ref)}><b>{f.label}</b><small>{f.text}</small></button></li>)}</ul> : <p className="hint">Nothing.</p>}</section>}
                    <ProblemsList found={problems} untried={report?.uncoveredTriggers} phases={program.phases.map(p => p.id)} asking={checking} go={goProblem}/>
                    <section className="wb-names"><h4>Signals · {signals.length}</h4>
                        <ul>{signals.map(s => <li key={s}><button type="button" className="link-button" onClick={() => setUse({type: 'signal', id: s})}>{s}</button>
                            {!readOnly && <button type="button" className="wb-mini" title="Rename everywhere" onClick={() => {
                                const to = window.prompt(`Rename the signal "${s}" everywhere`, s)?.trim();
                                if (to && to !== s && /^[A-Za-z0-9_][A-Za-z0-9_.:-]{0,79}$/.test(to)) edit(renameSignal(program, s, to));
                            }}>✎</button>}</li>)}</ul>
                        <VariablesPanel use={setUse} readOnly={readOnly} target={target}/>
                        <h4>Regions · {program.regions?.length ?? 0}</h4>
                        <ul>{(program.regions ?? []).map(r => <li key={r.id}><button type="button" className="link-button" onClick={() => setUse({type: 'region', id: r.id})}>{r.id}</button>
                            <small> {names.place?.[r.cell] ?? r.cell} ({r.x1},{r.y1})–({r.x2},{r.y2})</small></li>)}</ul>
                        <button type="button" onClick={() => select({kind: 'regions'})}>Draw regions on the map…</button>
                        {use && <div className="wb-uses"><h5>Where {use.type} “{use.id}” is used <button type="button" className="wb-close" onClick={() => setUse(null)} aria-label="Close">×</button></h5>
                            {(() => { const uses = usesOf(program, use); return uses.length ? <ul>{uses.map((u, i) => <li key={i}><button type="button" onClick={() => select(u.ref)}>{u.where}</button></li>)}</ul>
                                : <p className="hint">Nowhere.</p>; })()}</div>}
                    </section>
                </aside>
            </div>
        </div>
    </BenchContext.Provider>;
}
