import {useState} from 'react';
import type {Mission, Objective} from './StoryMissions';

import {effectName,makeEffect,StoryEffectFields,type Effect} from './StoryEffectFields';
export type Action =
    | {do:'emit'; signal:string}
    | {do:'schedule'; signal:string; days:number; repeat?:boolean; scope?:string; clock?:string; owner?:string}
    | {do:'cancel'; signal:string; owner?:string}
    | {do:'enter'; phase:string}
    | {do:'effect'; effect:Effect}
    | {do:'add'|'set'; scope:string; key:string; value:string|number};
export type Condition =
    | {test:'all'|'any'; conditions:Condition[]}
    | {test:'not'; condition:Condition}
    | {test:'mission'; id:string}
    | {test:'field'; field:string; op:string; value:string|number}
    | {test:'counter'; scope:string; key:string; op:string; value:number};
export interface Trigger {
    id:string; on:string; signal?:string; match?:Record<string,string>; if?:Condition;
    audience?:string; once?:string; afterDays?:number; actions:Action[];
}
export interface Component {role:string; label:string; trigger?:Trigger; objective?:Objective; gate?:Pick<Mission,'mode'|'percent'|'contributors'>; action?:Action}
export interface ComponentSource {id:string; title:string; category:string; components:Component[]}
interface Props {
    index:number; select:(index:number)=>void;
    ruleGroups:{phase:number|null; label:string; rules:Trigger[]; missions:Mission[]}[];
    navigate:(phase:number|null,index:number)=>void;
    campaign:boolean; triggers:Trigger[]; sources:ComponentSource[]; events:string[]; effects:Record<string,string[]>;
    phases:string[]; missions:Mission[]; change:(triggers:Trigger[])=>void; changeMissions:(missions:Mission[])=>void;
}
// Raw JSON can be incomplete while the author types. Render the form only for editable shapes.
const record=(v:unknown):v is Record<string,unknown>=>typeof v==='object' && v!==null && !Array.isArray(v);
function editableCondition(v:unknown,depth=0):boolean {
    if(!record(v) || depth>5) return false;
    if(v.test==='all'||v.test==='any') return Array.isArray(v.conditions) && v.conditions.every(c=>editableCondition(c,depth+1));
    if(v.test==='not') return editableCondition(v.condition,depth+1);
    if(v.test==='mission') return typeof v.id==='string';
    return (v.test==='counter'||v.test==='field') && ['string','number'].includes(typeof v.value);
}
export function editableRule(v:unknown) {
    return record(v) && typeof v.id==='string' && typeof v.on==='string' && (v.if===undefined || editableCondition(v.if))
        && Array.isArray(v.actions) && v.actions.every(a=>record(a) && (a.do==='effect' ? record(a.effect) && typeof a.effect.kind==='string'
            : ['enter','emit','set','add','schedule','cancel'].includes(String(a.do))));
}
const fields=['actor','target','item','cell','detail'];
const amounts=['quantity','coins'];
const operators=['=','!=','>','>=','<','<='];
function condition(kind:string,missions:Mission[]):Condition {
    const base:Condition={test:'counter',scope:'story',key:'supplies',op:'>=',value:1};
    switch(kind) {
    case 'all': case 'any': return {test:kind,conditions:[base]};
    case 'not': return {test:'not',condition:base};
    case 'mission': return {test:'mission',id:missions[0]?.id ?? ''};
    case 'field': return {test:'field',field:'item',op:'=',value:''};
    default: return base;
    }
}
const options=(values:string[])=>values.map(v=><option key={v}>{v}</option>);
function ConditionRow({node,edit,missions,depth=1}:{node:Condition; edit:(c:Condition)=>void; missions:Mission[];depth?:number}) {
    return <fieldset><label>Test<select aria-label="Test" value={node.test} onChange={e=>edit(condition(e.target.value,missions))}>
        {options([...(depth<5?['all','any','not']:[]),'field','counter',...(missions.length?['mission']:[])])}</select></label>
        {(node.test==='all'||node.test==='any') && <>
            {node.conditions.map((child,i)=><div key={i}><ConditionRow node={child} missions={missions} depth={depth+1}
                edit={next=>edit({...node,conditions:node.conditions.map((c,n)=>n===i?next:c)})}/>
                <button disabled={node.conditions.length<2} onClick={()=>edit({...node,conditions:node.conditions.filter((_,n)=>n!==i)})}>Remove condition {i+1}</button></div>)}
            <button disabled={node.conditions.length>=8} onClick={()=>edit({...node,conditions:[...node.conditions,condition('field',missions)]})}>Add condition</button></>}
        {node.test==='not' && <ConditionRow node={node.condition} edit={c=>edit({...node,condition:c})} depth={depth+1} missions={missions}/>}
        {node.test==='mission' && <label>Completed mission<select aria-label="Completed mission" value={node.id} onChange={e=>edit({...node,id:e.target.value})}>
            {missions.map(m=><option key={m.id} value={m.id}>{m.title}</option>)}</select></label>}
        {node.test==='counter' && <><label>Counter scope<select aria-label="Counter scope" value={node.scope} onChange={e=>edit({...node,scope:e.target.value})}>{options(['story','player'])}</select></label>
            <label>Counter name<input value={node.key} onChange={e=>edit({...node,key:e.target.value})}/></label></>}
        {node.test==='field' && <label>Event field<select aria-label="Event field" value={node.field} onChange={e=>edit({...node,field:e.target.value,op:'=',value:amounts.includes(e.target.value)?0:''})}>
            {options([...fields,...amounts])}</select></label>}
        {(node.test==='counter'||node.test==='field') && <><label>Comparison<select aria-label="Comparison" value={node.op} onChange={e=>edit({...node,op:e.target.value})}>
            {options(node.test==='counter'||amounts.includes(node.field)?operators:['=','!='])}</select></label>
            <label>Compare with<input type={typeof node.value==='number'?'number':'text'} value={node.value} onChange={e=>{
                if(node.test==='counter') edit({...node,value:Number(e.target.value)});
                else edit({...node,value:amounts.includes(node.field)?Number(e.target.value):e.target.value});
            }}/></label></>}
    </fieldset>;
}
function makeAction(kind:string,phases:string[],effects:Props['effects']):Action {
    if(kind==='schedule') return {do:'schedule',signal:'deadline',days:1};
    if(kind==='cancel') return {do:'cancel',signal:'deadline'};
    if(kind==='enter') return {do:'enter',phase:phases.at(-1) ?? ''};
    if(kind==='effect') return {do:'effect',effect:makeEffect('deed.award',effects['deed.award'])};
    if(kind==='set'||kind==='add') return {do:kind,scope:'story',key:'supplies',value:1};
    return {do:'emit',signal:'route-open'};
}
function unique(base:string,used:string[]) {
    let id=base;
    for(let n=2;used.includes(id);n++) id=base+'-'+n;
    return id;
}
function reorder<T,>(items:T[],from:number,to:number) {
    const copy=[...items];
    const [item]=copy.splice(from,1);
    copy.splice(to,0,item);
    return copy;
}
function describeCondition(c:Condition):string {
    if(c.test==='field') return c.field+' '+c.op+' '+c.value;
    if(c.test==='counter') return c.key+' '+c.op+' '+c.value;
    if(c.test==='mission') return c.id+' complete';
    if(c.test==='not') return 'Not ('+describeCondition(c.condition)+')';
    return '('+c.conditions.map(describeCondition).join(c.test==='all'?' and ':' or ')+')';
}
function describeAction(a:Action) {
    switch(a.do) {
        case 'emit':return 'Send '+a.signal;
        case 'enter':return 'Enter '+a.phase;
        case 'set':return a.key+' = '+a.value;
        case 'add':return a.key+' + '+a.value;
        case 'effect':return effectName(a.effect.kind);
        case 'schedule':return a.signal+' in '+a.days+'d';
        case 'cancel':return 'Cancel '+a.signal;
    }
}
export function StoryTriggerEditor({index,select,ruleGroups,navigate,campaign,triggers,sources,events,effects,phases,missions,change,changeMissions}:Props) {
    const [filter,setFilter]=useState('');
    const [search,setSearch]=useState('');
    const [eventSearch,setEventSearch]=useState('');
    const [recipe,setRecipe]=useState('');
    const [piece,setPiece]=useState(0);
    const [missionId,setMissionId]=useState('');
    const at=Math.min(index,triggers.length-1);
    const rule=triggers[at];
    const found=sources.filter(s=>(s.title+' '+s.category+' '+s.components.map(c=>c.label+' '+(c.trigger?.on ?? '')).join(' ')).toLowerCase().includes(search.toLowerCase()));
    const source=found.find(s=>s.id===recipe) ?? found[0];
    const part=source?.components[Math.min(piece,source.components.length-1)];
    const mission=missions.find(m=>m.id===missionId);
    const outputs=(t:Trigger)=>t.actions.flatMap(a=>a.do==='emit'||a.do==='schedule'?[a.signal]:[]);
    const connected=new Set(rule?[...outputs(rule),...(rule.on==='story signal' && rule.signal?[rule.signal]:[])]:[]);

    function revise(next:Trigger) {change(triggers.map((t,i)=>i===at?next:t));}
    function add(t:Trigger) {
        change([...triggers,{...t,id:unique(t.id,triggers.map(x=>x.id))}]);
        select(triggers.length);
    }
    function changeAction(i:number,a:Action) {
        revise({...rule,actions:rule.actions.map((old,n)=>i===n?a:old)});
    }
    function addListener(o:Objective) {
        if(mission) changeMissions(missions.map(m=>m===mission?{...m,objectives:[...m.objectives,structuredClone(o)]}:m));
        else {
            const id=unique('mission',missions.map(m=>m.id));
            changeMissions([...missions,{id,title:o.label,mode:'individual',objectives:[structuredClone(o)]}]);
            setMissionId(id);
        }
    }
    return <section className="story-trigger-editor"><h4>Trigger editor</h4>
        {triggers.length>6 && <label>Find a trigger<input value={filter} onChange={e=>setFilter(e.target.value)}/></label>}
        {!!triggers.length && <div className="trigger-ledger" aria-label="Trigger overview">
            <div className="trigger-head"><span>Event</span><span>Conditions</span><span>Actions</span></div>
            {triggers.map((t,i)=>{const event=t.on==='story signal'?t.signal:t.on;
                const tests=[...Object.entries(t.match ?? {}).map(([k,v])=>k+' = '+v),...(t.if?[describeCondition(t.if)]:[])];
                const actions=t.actions.map(describeAction);
                if(![t.id,event,...tests,...actions].join(' ').toLowerCase().includes(filter.toLowerCase()))return null;
                return <button key={i} className="trigger-row" aria-pressed={at===i} onClick={()=>select(i)}>
                    <span><strong>{t.id}</strong><small>{event}</small></span>
                    <span>{tests.join(' · ') || (t.audience==='participant'?'Eligible player':'Any match')}</span>
                    <span>{actions.join(' → ')}</span></button>;
            })}
        </div>}
        <div className="trigger-toolbar">
        <button disabled={triggers.length>=64} onClick={()=>add({id:'trigger',on:'conversation',audience:'participant',actions:[{do:'emit',signal:'route-open'}]})}>New trigger</button>
        {rule && <><button disabled={at===0} onClick={()=>{change(reorder(triggers,at,at-1));select(at-1);}}>Move trigger earlier</button>
            <button disabled={at===triggers.length-1} onClick={()=>{change(reorder(triggers,at,at+1));select(at+1);}}>Move trigger later</button>
            <button onClick={()=>change(triggers.filter((_,i)=>i!==at))}>Remove trigger</button></>}
        </div><details className="component-library" open={!triggers.length || undefined}><summary>Components from the 100 examples</summary>
            <label>Find a component<input value={search} onChange={e=>{setSearch(e.target.value);setPiece(0);}}/></label>
            <label>Source example<select aria-label="Source example" value={source?.id ?? ''} onChange={e=>{setRecipe(e.target.value);setPiece(0);}}>
                {found.map(s=><option key={s.id} value={s.id}>{s.title}</option>)}</select></label>
            {part && <><label>Component<select aria-label="Component" value={Math.min(piece,source.components.length-1)} onChange={e=>setPiece(Number(e.target.value))}>
                {source.components.map((c,i)=><option key={i} value={i}>{c.role} · {c.label}</option>)}</select></label>
                {part.trigger && <><p>{part.trigger.on} {Object.entries(part.trigger.match ?? {}).map(([k,v])=>k+' = '+v).join(', ')}</p>
                    <button disabled={triggers.length>=64} onClick={()=>{
                        const t=structuredClone(part.trigger!);
                        if(part.role==='opening') t.actions=[makeAction('enter',phases,effects)];
                        add(t);
                    }}>Copy event as a trigger</button>
                    {rule && <button onClick={()=>revise({...rule,on:part.trigger!.on,match:structuredClone(part.trigger!.match),signal:undefined,afterDays:undefined})}>Use event on selected trigger</button>}</>}
                {part.objective && <p>Journal signal · {part.objective.signal} · {part.objective.goal ?? 1} {part.objective.measure ?? 'occurrence(s)'}
                    {part.objective.sameAsFirst?' · Same '+part.objective.sameAsFirst+' as the first objective':''}</p>}
                {(part.objective||part.gate) && <label>Journal destination<select aria-label="Journal destination" value={mission?.id ?? ''} onChange={e=>setMissionId(e.target.value)}>
                    <option value="">{part.gate?'Choose a journal mission':'New journal mission'}</option>{missions.map(m=><option key={m.id} value={m.id}>{m.title}</option>)}</select></label>}
                {part.objective && <button disabled={(mission?mission.objectives.length>=8:missions.length>=16) || (!!part.objective.sameAsFirst && !mission?.objectives.length)} onClick={()=>addListener(part.objective!)}>Copy journal objective</button>}
                {part.gate && <button disabled={!mission} onClick={()=>changeMissions(missions.map(m=>{
                    if(m!==mission) return m;
                    const {percent,players,contributors,...rest}=m;
                    return {...rest,...part.gate!};
                }))}>Copy participation rule</button>}
                {part.action && <button disabled={!rule||rule.actions.length>=12} onClick={()=>revise({...rule,actions:[...rule.actions,structuredClone(part.action!)]})}>Copy outcome to trigger</button>}
            </>}
        </details>
        {rule && <><label>Trigger name<input value={rule.id} onChange={e=>revise({...rule,id:e.target.value})}/></label>
            {connected.size>0 && <details><summary>Signal connections</summary>{[...connected].map(name=>{
                const related=ruleGroups.flatMap(group=>group.rules.map((trigger,i)=>({group,trigger,i})));
                const buttons=(links:typeof related)=>links.map(({group,trigger,i})=>
                    <button key={group.phase+'-'+i} onClick={()=>navigate(group.phase,i)}>{group.label} · {trigger.id}</button>);
                const senders=related.filter(({trigger})=>outputs(trigger).includes(name));
                const listeners=related.filter(({trigger})=>trigger.on==='story signal' && trigger.signal===name);
                const journal=ruleGroups.flatMap(group=>group.missions.filter(m=>m.objectives.some(o=>o.on==='story signal' && o.signal===name))
                    .map(m=><span key={group.phase+'-'+m.id}> · {group.label} · {m.title} (journal)</span>));
                return <section key={name}><strong>{name}</strong><div>Sent by {senders.length?buttons(senders):'no trigger'}</div>
                    <div>Received by {buttons(listeners)}{journal}
                    {!listeners.length && !journal.length && 'no trigger or journal objective'}</div></section>;
            })}</details>}
            <div className="trigger-inspector"><fieldset><legend>Event</legend><label>Find an event<input type="search" value={eventSearch} onChange={e=>setEventSearch(e.target.value)}/></label><label>Listen for<select aria-label="Listen for" value={rule.on} onChange={e=>{
                const {signal,afterDays,...rest}=rule; const on=e.target.value;
                revise({...rest,on,match:undefined,...(on==='story signal'?{signal:'route-open'}:{}),...(on==='clock'?{afterDays:1,once:'visit',audience:'world'}:{})});
            }}>{options(events.filter(k=>k===rule.on || k.toLowerCase().includes(eventSearch.toLowerCase())).sort())}</select></label>
            {rule.on==='story signal' && <label>Incoming signal<input value={rule.signal ?? ''} onChange={e=>revise({...rule,signal:e.target.value})}/></label>}
            {rule.on==='clock' && <label>Delay in game days<input type="number" step="any" value={rule.afterDays ?? 1} onChange={e=>revise({...rule,afterDays:Number(e.target.value)})}/></label>}
            <label>Whose event<select aria-label="Whose event" disabled={rule.on==='clock'} value={rule.audience ?? 'world'} onChange={e=>revise({...rule,audience:e.target.value})}>{options(['world','participant'])}</select></label>
            <label>Fire<select aria-label="Fire" disabled={rule.on==='clock'} value={rule.once ?? 'each'} onChange={e=>revise({...rule,once:e.target.value})}>
                <option value="each">Every matching event</option><option value="visit">Once this phase visit</option><option value="player">Once per player this visit</option><option value="campaign">Once per campaign</option><option value="member">Once per player per campaign</option></select></label>
            <details><summary>Match event fields</summary>{fields.map(k=><label key={k}>{k}<input value={rule.match?.[k] ?? ''} onChange={e=>{
                const match={...rule.match}; if(e.target.value) match[k]=e.target.value;else delete match[k];revise({...rule,match});
            }}/></label>)}</details></fieldset>
            <fieldset><legend>Conditions</legend>{rule.if?<><ConditionRow node={rule.if} missions={campaign?[]:missions} edit={c=>revise({...rule,if:c})}/>
                <button onClick={()=>revise({...rule,if:undefined})}>Clear conditions</button></>:<button onClick={()=>revise({...rule,if:condition('all',missions)})}>Add condition group</button>}</fieldset>
            <fieldset><legend>Actions in order</legend>{rule.actions.map((a,i)=><fieldset key={i}><legend>Action {i+1}</legend>
                <label>Action type<select aria-label="Action type" value={a.do} onChange={e=>changeAction(i,makeAction(e.target.value,phases,effects))}>
                <option value="emit">Send a story signal</option><option value="set">Set a counter</option><option value="add">Add to a counter</option>
                <option value="schedule">Start a named timer</option><option value="cancel">Cancel a named timer</option>
                <option value="effect">Apply a world effect</option><option value="enter">Enter a phase</option></select></label>
                {(a.do==='schedule'||a.do==='cancel') && <label>Timer signal<input value={a.signal} onChange={e=>changeAction(i,{...a,signal:e.target.value})}/></label>}
                {(a.do==='schedule'||a.do==='cancel') && <label>Timer owner<select value={a.owner ?? 'story'} onChange={e=>changeAction(i,{...a,owner:e.target.value})}>
                    <option value="story">Shared by the campaign</option><option value="player" disabled={rule.audience!=='participant'}>Separate timer for each participant</option></select></label>}
                {a.do==='schedule' && <><label>Timer clock<select value={a.clock ?? 'game'} onChange={e=>changeAction(i,{...a,clock:e.target.value})}><option value="game">World calendar</option><option value="real">Elapsed real time</option></select></label>
                    <label>Delay in days<input type="number" step="any" value={a.days} onChange={e=>changeAction(i,{...a,days:Number(e.target.value)})}/></label>
                    <label>Timer lifetime<select value={a.scope ?? 'visit'} onChange={e=>changeAction(i,{...a,scope:e.target.value})}><option value="visit">This phase visit</option><option value="campaign">Across phases</option></select></label>
                    <label><input type="checkbox" checked={!!a.repeat} onChange={e=>changeAction(i,{...a,repeat:e.target.checked})}/>Repeat this timer</label></>}
                {a.do==='emit' && <label>Outgoing signal<input value={a.signal} onChange={e=>changeAction(i,{...a,signal:e.target.value})}/></label>}
                {a.do==='enter' && <label>Next phase<select aria-label="Next phase" value={a.phase} onChange={e=>changeAction(i,{...a,phase:e.target.value})}>{options(phases)}</select></label>}
                {(a.do==='add'||a.do==='set') && <><label>Counter scope<select aria-label="Counter scope" value={a.scope} onChange={e=>changeAction(i,{...a,scope:e.target.value})}>{options(['story','player'])}</select></label>
                    <label>Counter name<input value={a.key} onChange={e=>changeAction(i,{...a,key:e.target.value})}/></label>
                    <label>Value source<select aria-label="Value source" value={typeof a.value==='number'?'number':a.value} onChange={e=>changeAction(i,{...a,value:e.target.value==='number'?1:e.target.value})}>{options(['number',...amounts])}</select></label>
                    {typeof a.value==='number' && <label>Counter value<input type="number" value={a.value} onChange={e=>changeAction(i,{...a,value:Number(e.target.value)})}/></label>}</>}
                {a.do==='effect' && <><label>World effect<select aria-label="World effect" value={a.effect.kind} onChange={e=>changeAction(i,{do:'effect',effect:makeEffect(e.target.value,effects[e.target.value])})}>
                    {Object.keys(effects).filter(k=>k!=='npc.line').map(k=><option key={k} value={k}>{effectName(k)}</option>)}</select></label>
                    <StoryEffectFields effect={a.effect} fields={effects[a.effect.kind] ?? []} change={effect=>changeAction(i,{...a,effect})}/></>}
                <button disabled={!i} onClick={()=>revise({...rule,actions:reorder(rule.actions,i,i-1)})}>Move action earlier</button>
                <button disabled={i===rule.actions.length-1} onClick={()=>revise({...rule,actions:reorder(rule.actions,i,i+1)})}>Move action later</button>
                <button onClick={()=>revise({...rule,actions:rule.actions.filter((_,n)=>n!==i)})}>Remove action</button></fieldset>)}
                <button disabled={rule.actions.length>=12} onClick={()=>revise({...rule,actions:[...rule.actions,makeAction('emit',phases,effects)]})}>Add action</button></fieldset></div></>}
    </section>;
}
