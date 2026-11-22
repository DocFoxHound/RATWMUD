import {useEffect, useState} from 'react';
import {storyProgram, worldName, type Target} from './api';
import type {Project} from '../model/model.mjs';
import {EmbeddedScenes} from './EmbeddedScenes';
import {editableRegion} from './StoryRegions';
import {editableRule} from './StoryTriggerEditor';
import {download} from '../lib/api';

import {StoryPhaseEditor, type WorldProgram as Program, type ProgramCatalogue as Catalogue} from './StoryPhaseEditor';

interface TraceTile {cell?:string;x?:number;y?:number}
function tracePlace({cell,x,y}:TraceTile) {
    return cell && x!==undefined && y!==undefined?' · '+cell+' ('+x+', '+y+')':'';
}
interface Report {uncoveredTriggers?:string[]; uncovered:string[]; voices:{phase:string;npc:string}[]; digest: string; passed: boolean; tests: {triggers?:(TraceTile & {phase:string;trigger:string;event?:string;on:string;signal?:string})[]; counters?:Record<string,unknown>; missionVisits?:{phase:string;visit:number;credits:Record<string,string[]>}[]; name:string; passed:boolean; path:string[]; expected:string[]; effects:Record<string,unknown>[]}[]; effects: Record<string,unknown>[]}
const pretty = (value: unknown) => JSON.stringify(value,null,2);

// A story approved and published here is watched and directed in the Campaigns tab (Docs/Design/85, 5.1: the Releases
// panel that was beside the editor moved there).
export function StoryProgramPanel({sceneLibraryKey,world,target, storageKey, canDirect, openCampaigns}: {sceneLibraryKey:string;world:Project|null;target:Target; storageKey:string; canDirect:boolean;
    openCampaigns?:(run:string)=>void}) {
    const [catalog,setCatalog] = useState<Catalogue>();
    const [source,setSource] = useState(() => { try { return localStorage.getItem(storageKey) ?? ''; } catch { return ''; } });
    const [report,setReport] = useState<Report>();
    const [ticket,setTicket] = useState('');
    const [selected,setSelected] = useState('');
    const [published,setPublished] = useState('');
    const [search,setSearch] = useState('');
    const [places,setPlaces] = useState<{kind:string;id:string;name:string}[]>([]);
    const [resident,setResident] = useState<{resident:{id:string;name:string}}>();
    const [feedback,setFeedback] = useState('');
    const [busy,setBusy] = useState(false);
    useEffect(() => {
        let active = true;
        storyProgram<Catalogue>(target,'catalog').then(c => {
            if (active) {setCatalog(c); setSource(previous => previous || pretty(c.blank));}
        }).catch(e => {if (active) setFeedback(e.message);});
        return () => {active = false;};
    }, [target]);
    let program: Program | undefined;
    try {program = JSON.parse(source);} catch { /* Raw editing includes incomplete JSON. */ }
    function edit(text:string) {
        setSource(text); setTicket(''); setReport(undefined); setFeedback(''); setPublished('');
        try {localStorage.setItem(storageKey,text);} catch {setFeedback('Browser storage is full or unavailable. Export a copy to keep this draft.');}
    }
    async function perform(work:()=>Promise<void>) {
        setBusy(true); setFeedback('');
        try {await work();} catch (e) {setFeedback(e instanceof Error ? e.message : String(e));} finally {setBusy(false);}
    }
    const editableGraph = program && (program.regions===undefined || (Array.isArray(program.regions) && program.regions.every(editableRegion))) && (program.triggers===undefined || (Array.isArray(program.triggers) && program.triggers.every(editableRule))) && Array.isArray(program.tests) && program.tests.every(c=>c && typeof c.name==='string'
        && (c.expectEffects===undefined || (Array.isArray(c.expectEffects) && c.expectEffects.every(e=>e && typeof e.phase==='string')))
        && (c.expectMissionVisits===undefined || (Array.isArray(c.expectMissionVisits) && c.expectMissionVisits.every(v=>v && typeof v.phase==='string')))
        && Array.isArray(c.cohort) && Array.isArray(c.expect) && Array.isArray(c.events) && c.events.every(f=>f && typeof f.kind==='string'))
        && (program.scenes===undefined || (Array.isArray(program.scenes) && program.scenes.every(s=>s && typeof s.title==='string' && Array.isArray(s.beats) && s.beats.every(b=>b && typeof b.id==='string'))))
        && typeof program.title === 'string' && program.audience && typeof program.audience.kind === 'string' && Array.isArray(program.phases) && program.phases.every(p=>
        p && typeof p.id==='string' && typeof p.journal==='string' && (!p.guestVoices || Array.isArray(p.guestVoices)) &&
        (p.triggers === undefined || (Array.isArray(p.triggers) && p.triggers.every(editableRule))) &&
        (p.missions === undefined || (Array.isArray(p.missions) && p.missions.every(m=>m && typeof m.id==='string' && typeof m.title==='string' &&
            ['individual','group','either'].includes(m.mode) && Array.isArray(m.objectives) && m.objectives.every(o=>o && typeof o.label==='string' && typeof o.on==='string')))) &&
        Array.isArray(p.exits) && p.exits.every(e=>e && typeof e.on==='string' && (e.missions===undefined || Array.isArray(e.missions))) &&
        Array.isArray(p.effects) && p.effects.every(e=>e && typeof e.kind==='string'));
    const reviewed = report?.passed && !report.uncovered?.length && program;
    return <div className="story-columns program-workspace"><section className="dm-panel"><h3>{program?.title || 'World story'}</h3>
        <fieldset disabled={busy}><div className="story-filebar"><button disabled={!catalog} onClick={()=>{if(catalog && window.confirm('Replace this draft with a blank story?')) edit(pretty(catalog.blank));}}>New blank story</button><label>Start from a story<select defaultValue="" onChange={e => {
            const example = catalog?.examples[e.target.value];
            if (example && (!source || window.confirm('Replace this draft with the selected story?'))) edit(pretty(example));
            e.target.value='';
        }}><option value="">Choose an example</option>{Object.entries(catalog?.examples ?? {}).map(([id,s]) => <option key={id} value={id}>{s.title}</option>)}</select></label>
        <label>Import a program<input type="file" accept=".json,application/json" onChange={e => {
            const f=e.target.files?.[0]; e.target.value=''; if (f) void perform(async () => {
                if (f.size > 1024*1024) throw Error('Choose a program under 1 MiB.');
                if (!source || window.confirm('Replace this draft with the imported story?')) edit(await f.text());
            });
        }}/></label>
        </div>{catalog && program && editableGraph &&
            <><EmbeddedScenes target={target} libraryKey={sceneLibraryKey} program={program} examples={catalog.sceneExamples ?? {}} edit={p=>edit(pretty(p))}/>
            <StoryPhaseEditor world={world} start={id=>{if(window.confirm('Replace this draft with the selected event story?'))
                void perform(async()=>edit(pretty((await storyProgram<{program:Program}>(target,'draft-event',{eventType:id})).program)));}} program={program} catalog={catalog} onEdit={p=>edit(pretty(p))}/></>}
        <details><summary>Program JSON and rehearsal cases</summary><label>Program JSON<textarea rows={18} spellCheck={false} value={source} onChange={e => edit(e.target.value)}/></label></details>
        <button disabled={!source} onClick={() => download('world-story.json',source)}>Export draft</button>
        <button disabled={!source} onClick={() => void perform(async () => setReport(await storyProgram<Report>(target,'rehearse',{program:JSON.parse(source)})))}>Rehearse all paths</button>
        </fieldset>
        {catalog && <details><summary>Available triggers and effects</summary><p>{catalog.events.join(', ')}</p>
            <ul>{Object.entries(catalog.effects).map(([kind,fields]) => <li key={kind}><code>{kind}</code> ({fields.join(', ')})</li>)}</ul></details>}
        {report && <section><h4>Rehearsal results</h4><ul>{report.tests.map((t,i) => <li key={i}>{t.passed?'Pass':'Fail'} · {t.name}<br/>{t.path.join(' → ')}{!t.passed && <p>Expected {t.expected.join(' → ')}</p>}
                {!!t.triggers?.length && <details><summary>Trigger trace</summary><ol>{t.triggers.map((f,i)=><li key={i}>{f.phase} · {f.trigger} ← {f.signal ?? f.on}{f.event?' ('+f.event+')':''}{tracePlace(f)}</li>)}</ol></details>}
                {t.counters && Object.keys(t.counters).length>0 && <details><summary>Final counters</summary><pre>{pretty(t.counters)}</pre></details>}
                {!!t.missionVisits?.length && <details><summary>Mission credit by phase</summary>{t.missionVisits.map(v=><div key={v.visit}>
                    <strong>{v.phase} · visit {v.visit}</strong><ul>{Object.entries(v.credits).map(([id,players])=><li key={id}>{id} · {players.join(', ') || 'No finishers'}</li>)}</ul></div>)}</details>}
                {!!t.effects?.length && <details><summary>Effects in this run</summary><ol>{t.effects.map((e,n)=><li key={n}><code>{JSON.stringify(e)}</code></li>)}</ol></details>}</li>)}</ul>
            {!!report.uncovered?.length && <p>Phases awaiting rehearsal · {report.uncovered.join(', ')}.</p>}
            {!!report.uncoveredTriggers?.length && <p>Triggers awaiting rehearsal · {report.uncoveredTriggers.join(', ')}</p>}
            <h4>Effects for approval</h4>{report.effects.length ? <ul>{report.effects.map((e,i) => <li key={i}><code>{JSON.stringify(e)}</code></li>)}</ul>:<p>Journal entries only.</p>}
            {report.voices?.map(v=><p key={v.phase+v.npc}>Optional guest voice · {v.npc} during {v.phase}</p>)}
            <p>Audience · {program?.audience.kind === 'active' ? 'Players active in the last '+program.audience.days+' days' : program?.audience.players?.join(', ')}</p>
            <details><summary>Version fingerprint</summary><code>{report.digest}</code></details>
            <button disabled={!canDirect || !reviewed || busy || !!ticket} onClick={() => void perform(async () => {
                const r=await storyProgram<{ticket:string;run:string}>(target,'approve',{program,digest:report.digest});
                setTicket(r.ticket); setSelected(r.run);
            })}>Approve this version for {worldName(target)}</button></section>}
        {ticket && <fieldset disabled={busy}><label>Publication ticket<input readOnly value={ticket} onFocus={e=>e.target.select()}/></label>
            <button onClick={() => void perform(async () => {const r=await storyProgram<{run:string}>(target,'publish',{ticket}); setTicket(''); setPublished(r.run);})}>Publish approved story</button>
            {openCampaigns && selected && <button onClick={()=>openCampaigns(selected)}>Open in Campaigns</button>}</fieldset>}
        {published && <p className="hint">Published. {openCampaigns && <button onClick={()=>openCampaigns(published)}>Open in Campaigns</button>}</p>}
        {feedback && <p role="alert">{feedback}</p>}
    </section>{canDirect && <aside className="dm-panel"><h3>For an author</h3>
        <p className="hint">The stories running in {worldName(target)} are watched in the Campaigns tab.
            {openCampaigns && <> <button onClick={()=>openCampaigns('')}>Open Campaigns</button></>}</p>
        <details><summary>World references for an author</summary>
            <label>Search Atlas names and IDs<input value={search} onChange={e=>setSearch(e.target.value)}/></label>
            <button disabled={busy} onClick={()=>void perform(async()=>{const r=await storyProgram<{matches:typeof places}>(target,'atlas.search',{query:search}); setPlaces(r.matches); setResident(undefined);})}>Find references</button>
            <ul>{places.map(p=><li key={p.kind+p.id}>{p.name} · <code>{p.id}</code> · {p.kind}
                {p.kind==='npc' && <button disabled={busy} onClick={()=>void perform(async()=>setResident(await storyProgram(target,'npc.context',{npc:p.id})))}>Read resident history</button>}</li>)}</ul>
            {resident && <pre>{pretty(resident)}</pre>}
            {!!places.length && <button onClick={()=>download('story-context.json',pretty({matches:places,residents:resident?[resident]:[]}))}>Export author context</button>}
        </details>
    </aside>}</div>;
}
