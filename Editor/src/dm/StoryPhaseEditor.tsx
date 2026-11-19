import {useMemo, useState} from 'react';
import type {Project} from '../model/model.mjs';
import {StoryRegions,type Region} from './StoryRegions';
import {StoryMissions, type Mission, type EventType} from './StoryMissions';

import {StoryTriggerEditor, type Trigger} from './StoryTriggerEditor';
import {effectName,makeEffect,StoryEffectFields,type Effect} from './StoryEffectFields';
import {StoryRehearsalEditor, type Scenario} from './StoryRehearsalEditor';
interface Exit {
    missions?:string[];
    on:string; to:string; match?:Record<string,string>; afterDays?:number; percent?:number; players?:number; participant?:boolean;
}
interface Phase {campaignRules?:boolean;triggers?:Trigger[]; missions?:Mission[]; guestVoices?:string[]; id:string; journal:string; effects:Effect[]; exits:Exit[]}
export interface WorldProgram {regions?:Region[];triggers?:Trigger[];start:string; tests:Scenario[]; id:string; title:string; audience:{kind:string; days?:number; players?:string[]}; phases:Phase[]}
export interface ProgramCatalogue {blank:WorldProgram;triggerRules:{events:string[]}; eventTypes:EventType[]; missionEvents:string[]; missionMeasures:Record<string,string[]>; events:string[]; effects:Record<string,string[]>; examples:Record<string,WorldProgram>}

function newExit(on:string, to:string):Exit {
    if (on==='missions completed') return {on,to,missions:[]};
    if (on === 'clock') return {on,to,afterDays:1};
    if (on === 'dm signal') return {on,to,match:{detail:'direction'}};
    return {on,to};
}

// Region IDs can occur in nested conditions, effects, entrance/exit filters and rehearsal assertions.
function regionReferences(program:WorldProgram) {
    const refs:[Record<string,unknown>,string][]=[];
    function visit(value:unknown) {
        if(!value || typeof value!=='object')return;
        if(Array.isArray(value)){value.forEach(visit);return;}
        const node=value as Record<string,unknown>;
        if(typeof node.region==='string')refs.push([node,'region']);
        if([node.on,node.kind].some(event=>event==='region entered'||event==='region left')) {
            if(typeof node.target==='string')refs.push([node,'target']);
            if(node.match && typeof node.match==='object' && 'target' in node.match)refs.push([node.match as Record<string,unknown>,'target']);
        }
        Object.values(node).forEach(visit);
    }
    visit(program);
    return refs;
}

export function StoryPhaseEditor({world,program,catalog,onEdit,start}: {world:Project|null;start:(id:string)=>void; program:WorldProgram; catalog:ProgramCatalogue; onEdit:(p:WorldProgram)=>void}) {
    const [index,setIndex] = useState(0);
    const [scope,setScope]=useState<'phase'|'campaign'>('phase');
    const [ruleIndex,setRuleIndex]=useState(0);
    function goPhase(i:number) {setIndex(i);setRuleIndex(0);}
    function goScope(s:'phase'|'campaign') {setScope(s);setRuleIndex(0);}
    const at = Math.min(index,program.phases.length-1);
    const phase = program.phases[at];
    const refs=useMemo(()=>program.regions?.length?regionReferences(program):[],[program]);
    if (!phase) return null;
    function update(patch:Partial<Phase>) {
        onEdit({...program,phases:program.phases.map((p,i)=>i===at?{...phase,...patch}:p)});
    }
    const regions=program.regions ?? [];
    const regionIds=regions.map(r=>r.id);
    function changeRegions(next:Region[],rename?:[string,string]) {
        const updated=rename?structuredClone(program):program;
        if(rename) for(const [obj,key] of regionReferences(updated)) {
            if(obj[key]===rename[0])obj[key]=rename[1];
        }
        onEdit({...updated,regions:next});
    }
    function rename(next:string) {
        const renamed=(id:string)=>id===phase.id?next:id;
        const rewrite=(t:Trigger)=>({...t,actions:t.actions.map(a=>a.do==='enter'?{...a,phase:renamed(a.phase)}:a)});
        onEdit({...program,start:renamed(program.start),triggers:program.triggers?.map(rewrite),
            phases:program.phases.map(p=>({...p,id:renamed(p.id),triggers:p.triggers?.map(rewrite),
                exits:p.exits.map(e=>({...e,to:renamed(e.to)}))})),
            tests:program.tests.map(c=>({...c,expect:c.expect.map(renamed),expectEffects:c.expectEffects?.map(e=>({...e,phase:renamed(e.phase)})),
                expectMissionVisits:c.expectMissionVisits?.map(v=>({...v,phase:renamed(v.phase)}))}))});
    }
    function exit(i:number, value:Exit) {update({exits:phase.exits.map((e,n)=>n===i?value:e)});}
    function effect(i:number, value:Effect) {update({effects:phase.effects.map((e,n)=>n===i?value:e)});}
    return <><details className="story-settings"><summary>Campaign settings</summary><label>Story ID<input value={program.id} onChange={e=>onEdit({...program,id:e.target.value})}/></label><label>Campaign title<input value={program.title} onChange={e=>onEdit({...program,title:e.target.value})}/></label>
        <label>Audience<select aria-label="Audience" value={program.audience.kind} onChange={e=>onEdit({...program,
            audience:e.target.value==='active'?{kind:'active',days:7}:{kind:'players',players:[]}})}>
            <option value="active">Recently active players</option><option value="players">Specific players</option></select></label>
        {program.audience.kind==='active' ? <label>Activity lookback in real days<input type="number" min="1" max="90"
            value={program.audience.days ?? 7} onChange={e=>onEdit({...program,audience:{kind:'active',days:e.target.valueAsNumber || 0}})}/></label>
            : <label>Player character IDs, one per line<textarea value={(program.audience.players ?? []).join('\n')}
                onChange={e=>onEdit({...program,audience:{kind:'players',players:e.target.value.split('\n')}})}/></label>}
        <label>Opening phase<select value={program.start} onChange={e=>onEdit({...program,start:e.target.value})}>
            {program.phases.map(p=><option key={p.id}>{p.id}</option>)}</select></label></details>
        <StoryRegions world={world} regions={regions} change={changeRegions} used={id=>refs.some(([obj,key])=>obj[key]===id)}/>
        <div className="phase-strip"><h4>Campaign phases</h4><nav aria-label="Campaign phases">{program.phases.map((p,i)=><button key={i}
            aria-current={at===i?'step':undefined} onClick={()=>goPhase(i)}>{p.id}</button>)}</nav>
        <button onClick={()=>{let n=program.phases.length+1; while(program.phases.some(p=>p.id==='phase-'+n)) n++;
            onEdit({...program,phases:[...program.phases,{id:'phase-'+n,journal:'',effects:[],exits:[]}]});goPhase(program.phases.length);
        }}>Add phase</button></div>
        <details className="phase-journal"><summary>Player journal · {phase.journal||phase.id}</summary>
            <label>Phase name<input key={phase.id} defaultValue={phase.id} onChange={e=>e.target.setCustomValidity('')} onBlur={e=>{
                const name=e.target.value.trim();
                if(name===phase.id)return;
                if(!/^[A-Za-z0-9_][A-Za-z0-9_.:-]{0,79}$/.test(name) || program.phases.some(p=>p.id===name)){
                    e.target.setCustomValidity('Use a unique phase ID of 1–80 letters, digits, underscores, periods, colons or hyphens.');e.target.reportValidity();return;
                }
                rename(name);
            }}/></label><label>Player journal<textarea rows={3} value={phase.journal} onChange={e=>update({journal:e.target.value})}/></label></details>
        <div className="rule-scope" role="group" aria-label="Trigger scope">
            <button aria-pressed={scope==='phase'} onClick={()=>goScope('phase')}>Phase triggers</button>
            <button aria-pressed={scope==='campaign'} onClick={()=>goScope('campaign')}>Campaign triggers</button></div>
        {scope==='campaign' && <label><input type="checkbox" checked={phase.campaignRules ?? !!(phase.triggers?.length || phase.exits.length)}
            onChange={e=>update({campaignRules:e.target.checked})}/>Run campaign triggers during {phase.id}</label>}
        <StoryTriggerEditor regions={regions} index={ruleIndex} select={setRuleIndex}
            ruleGroups={[{phase:null,label:'Campaign',rules:program.triggers ?? [],missions:[],effects:[]},
                ...program.phases.map((p,i)=>({phase:i,label:p.id,rules:p.triggers ?? [],missions:p.missions ?? [],effects:p.effects}))]}
            navigate={(target,rule)=>{if(target!==null)setIndex(target);setScope(target===null?'campaign':'phase');setRuleIndex(rule);}}
            key={scope+'-'+phase.id} campaign={scope==='campaign'} triggers={(scope==='campaign'?program.triggers:phase.triggers) ?? []} sources={catalog.eventTypes} events={catalog.triggerRules.events} effects={catalog.effects}
            phases={program.phases.map(p=>p.id)} missions={phase.missions ?? []} change={triggers=>{if(scope==='phase') update({triggers});
                else onEdit({...program,triggers,phases:program.phases.map((p,i)=>i===at && !program.triggers?.length && p.campaignRules===undefined?{...p,campaignRules:true}:p)});}} changeMissions={missions=>update({missions})}/>
        <details className="phase-tools"><summary>Journal missions · {phase.missions?.length ?? 0}</summary>
        <StoryMissions events={catalog.eventTypes} triggers={catalog.missionEvents} measures={catalog.missionMeasures} missions={phase.missions ?? []}
            change={missions=>update({missions,exits:phase.exits.map(edge=>edge.on==='missions completed'
                ? {...edge,missions:edge.missions?.filter(id=>missions.some(m=>m.id===id))}:edge)})} start={start}/></details>
        <details className="phase-tools"><summary>Phase exits and entry effects</summary>
        {!phase.exits.length && !phase.triggers?.length && !(program.triggers?.length && phase.campaignRules) && <p>This phase ends the story.</p>}
        {phase.exits.map((edge,i)=><fieldset key={i}><label>When<select value={edge.on} onChange={e=>exit(i,newExit(e.target.value,edge.to))}>
            {[...catalog.events,'missions completed'].map(on=><option key={on}>{on}</option>)}</select></label>
            <label>Continue to<select value={edge.to} onChange={e=>exit(i,{...edge,to:e.target.value})}>
                {program.phases.map(p=><option key={p.id}>{p.id}</option>)}</select></label>
            {edge.on==='missions completed' ? <fieldset><legend>Required missions</legend>{(phase.missions ?? []).map(m=><label key={m.id}>
                <input type="checkbox" checked={edge.missions?.includes(m.id) ?? false} onChange={e=>exit(i,{...edge,missions:e.target.checked?
                    [...(edge.missions ?? []),m.id]:(edge.missions ?? []).filter(id=>id!==m.id)})}/>{m.title}</label>)}</fieldset>
            : edge.on==='clock' ? <label>After game days<input type="number" min={1/1440} step="any" value={edge.afterDays ?? 1} onChange={e=>exit(i,{...edge,afterDays:e.target.valueAsNumber || 0})}/></label>
            : edge.on==='dm signal' ? <label>Named direction<input value={edge.match?.detail ?? ''} onChange={e=>exit(i,{...edge,match:{detail:e.target.value}})}/></label>
            : <><label>Participation<select value={edge.percent!==undefined?'percent':edge.players!==undefined?'players':edge.participant?'participant':'any'}
                onChange={e=>{const {percent,players,participant,...base}=edge; const mode=e.target.value;
                    exit(i,mode==='any'?base:{...base,[mode]:mode==='percent'?50:mode==='players'?1:true});}}>
                <option value="any">Any matching event</option><option value="participant">A participant</option>
                <option value="players">Number of distinct participants</option><option value="percent">Percentage of the opening audience</option></select></label>
                {(edge.percent!==undefined || edge.players!==undefined) && <label>{edge.percent!==undefined?'Percentage required':'Players required'}
                    <input type="number" min="1" max={edge.percent!==undefined?100:500} value={edge.percent ?? edge.players}
                        onChange={e=>exit(i,{...edge,[edge.percent!==undefined?'percent':'players']:e.target.valueAsNumber || 0})}/></label>}
                <details><summary>Event filters</summary>{['actor','target','cell','item','detail'].map(key=><label key={key}>{key}
                    <input value={edge.match?.[key] ?? ''} onChange={e=>{const match={...edge.match}; if(e.target.value) match[key]=e.target.value; else delete match[key]; exit(i,{...edge,match});}}/></label>)}</details></>}
            <button onClick={()=>update({exits:phase.exits.filter((_,n)=>n!==i)})}>Remove trigger</button></fieldset>)}
        <button onClick={()=>update({exits:[...phase.exits,newExit('conversation',program.phases[(at+1)%program.phases.length].id)]})}>Add phase exit</button>
        <label>Optional guest speakers (resident IDs, one per line)<textarea rows={2}
            value={(phase.guestVoices ?? []).join('\n')} onChange={e=>update({guestVoices:e.target.value ? e.target.value.split('\n') : []})}/></label>
        <h4>When this phase opens</h4>{phase.effects.map((e,i)=><fieldset key={i}><legend>{effectName(e.kind)}</legend>
            <StoryEffectFields regions={regions} effect={e} fields={catalog.effects[e.kind] ?? []} change={next=>effect(i,next)}/>
            <button onClick={()=>update({effects:phase.effects.filter((_,n)=>n!==i)})}>Remove effect</button></fieldset>)}
        <label>Add phase effect<select value="" onChange={e=>{const kind=e.target.value; const next=makeEffect(kind,catalog.effects[kind],regionIds);
            update({effects:[...phase.effects,next]});}}><option value="">Choose an effect</option>
            {Object.keys(catalog.effects).map(kind=><option key={kind} value={kind}>{effectName(kind)}</option>)}</select></label>
        </details><StoryRehearsalEditor regions={regions} cases={program.tests} events={catalog.events} phases={program.phases.map(p=>p.id)} change={tests=>onEdit({...program,tests})}/></>;
}
