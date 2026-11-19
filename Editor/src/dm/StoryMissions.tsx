import {useState} from 'react';
import type {Component} from './StoryTriggerEditor';

export type MissionMode = 'individual' | 'group' | 'either';
export interface Objective {
    label:string; on:string; signal?:string; sameAsFirst?:string; match?:Record<string,string>; measure?:'count'|'quantity'|'coins'; goal?:number;
}
export interface Mission {
    id:string; title:string; mode:MissionMode; objectives:Objective[]; contributors?:number; players?:number; percent?:number;
}
export interface EventType extends Mission {components:Component[]; category:string; scale:string; opening?:{journal:string}; ending?:{kind:string}[]}
interface Props {
    events:EventType[]; triggers:string[]; measures:Record<string,string[]>; missions:Mission[];
    change:(missions:Mission[])=>void; start:(id:string)=>void;
}
export function StoryMissions({events,triggers,measures,missions,change,start}:Props) {
    const [text,setText]=useState('');
    const [mode,setMode]=useState('');
    const [scale,setScale]=useState('');
    const [id,setId]=useState('');
    const matches=events.filter(e=>(!mode || e.mode===mode) && (!scale || e.scale===scale)
        && (e.title+' '+e.category+' '+e.objectives.map(o=>o.label).join(' ')).toLowerCase().includes(text.toLowerCase()));
    const selected=matches.find(e=>e.id===id) ?? matches[0];
    function revise(index:number,m:Mission) {change(missions.map((old,i)=>i===index?m:old));}
    function add(m:Mission) {
        let next=m.id;
        for(let n=2;missions.some(e=>e.id===next);n++) next=m.id+'-'+n;
        change([...missions,{...m,id:next}]);
    }
    return <section><details><summary>Complete mission examples</summary>
        <label>Search event types<input value={text} onChange={e=>setText(e.target.value)}/></label>
        <label>Play style<select aria-label="Play style" value={mode} onChange={e=>setMode(e.target.value)}>
            <option value="">All play styles</option>{['individual','group','either'].map(k=><option key={k}>{k}</option>)}</select></label>
        <label>Scale<select aria-label="Scale" value={scale} onChange={e=>setScale(e.target.value)}>
            <option value="">All scales</option>{['small','local','regional','realm'].map(k=><option key={k}>{k}</option>)}</select></label>
        <label>Event type ({matches.length})<select aria-label="Event type" value={selected?.id ?? ''} onChange={e=>setId(e.target.value)}>
            {matches.map(e=><option key={e.id} value={e.id}>{e.title}</option>)}</select></label>
        {selected && <><p>{selected.category} · {selected.scale} · {selected.mode}</p>
            {selected.opening && <p>{selected.opening.journal}</p>}
            <ol>{selected.objectives.map((o,i)=><li key={i}>{o.label}</li>)}</ol>
            {!!selected.ending?.length && <p>On resolution · {selected.ending.map(e=>e.kind).join(', ')}</p>}
            <button type="button" onClick={()=>start(selected.id)}>Start a story from this event</button>
            <button type="button" disabled={missions.length>=16} onClick={()=>add({id:selected.id,title:selected.title,mode:selected.mode,
                objectives:structuredClone(selected.objectives)})}>Add its mission to this phase</button></>}
        </details><h4>Journal missions in this phase</h4>
        {missions.map((m,i)=><details key={m.id}><summary>{m.title || m.id} · {m.mode}</summary>
            <label>Mission title<input value={m.title} onChange={e=>revise(i,{...m,title:e.target.value})}/></label>
            <label>Who completes it<select value={m.mode} onChange={e=>{
                const {players,percent,contributors,...other}=m;
                revise(i,{...other,mode:e.target.value as MissionMode});
            }}><option value="individual">Each player separately</option><option value="group">At least two contributors</option>
                <option value="either">One player or pooled contributions</option></select></label>
            {m.mode==='group' && <label>Distinct contributors required<input type="number" min={2} max={500} value={m.contributors ?? 2}
                onChange={e=>revise(i,{...m,contributors:Number(e.target.value)})}/></label>}
            {m.mode==='individual' && <><label>Campaign threshold<select value={m.percent===undefined?'players':'percent'} onChange={e=>{
                const {players,percent,...other}=m; revise(i,{...other,[e.target.value]:e.target.value==='percent'?50:1});
            }}><option value="players">Number of finishers</option><option value="percent">Percentage of eligible players</option></select></label>
                <label>{m.percent===undefined?'Finishers required':'Percentage required'}<input type="number" min={1} max={m.percent===undefined?500:100}
                    value={m.percent ?? m.players ?? 1} onChange={e=>revise(i,{...m,[m.percent===undefined?'players':'percent']:Number(e.target.value)})}/></label></>}
            {m.objectives.map((o,j)=>{
                const update=(patch:Partial<Objective>)=>revise(i,{...m,objectives:m.objectives.map((x,n)=>n===j?{...x,...patch}:x)});
                return <fieldset key={j}><legend>Objective {j+1}</legend>
                    <label>Journal instruction<input value={o.label} onChange={e=>update({label:e.target.value})}/></label>
                    <label>Evidence<select value={o.on} onChange={e=>update({on:e.target.value,signal:e.target.value==='story signal'?'route-open':undefined,measure:o.measure && measures[o.measure]?.includes(e.target.value) ? o.measure : 'count'})}>{[...triggers,'story signal'].map(t=><option key={t}>{t}</option>)}</select></label>
                    {o.on==='story signal' && <label>Journal signal<input value={o.signal ?? ''} onChange={e=>update({signal:e.target.value})}/></label>}
                    <label>Measure<select value={o.measure ?? 'count'} onChange={e=>update({measure:e.target.value as Objective['measure']})}>
                        <option value="count">Occurrences</option>{(o.on==='story signal'||measures.quantity.includes(o.on)) && <option value="quantity">Units recorded</option>}{(o.on==='story signal'||measures.coins.includes(o.on)) && <option value="coins">Coins recorded</option>}</select></label>
                    <label>Required amount<input type="number" min={1} value={o.goal ?? 1} onChange={e=>update({goal:Number(e.target.value)})}/></label>
                    {j>0 && <label>Match the first objective’s<select value={o.sameAsFirst ?? ''} onChange={e=>update({sameAsFirst:e.target.value || undefined})}>
                        <option value="">No linked field</option>{['actor','target','cell','item','detail'].map(k=><option key={k}>{k}</option>)}</select></label>}
                    <details><summary>Evidence filters</summary>{['actor','target','cell','item','detail'].map(key=><label key={key}>{key}
                        <input value={o.match?.[key] ?? ''} onChange={e=>{const filter={...o.match};
                            if(e.target.value) filter[key]=e.target.value; else delete filter[key]; update({match:filter});}}/></label>)}</details>
                    <button disabled={m.objectives.length<2} onClick={()=>revise(i,{...m,objectives:m.objectives.filter((_,n)=>j!==n)})}>Remove objective</button>
                </fieldset>;
            })}
            <button disabled={m.objectives.length>=8} onClick={()=>revise(i,{...m,objectives:[...m.objectives,{on:'conversation',label:''}]})}>Add objective</button>
            <button onClick={()=>change(missions.filter((_,n)=>n!==i))}>Remove mission</button>
        </details>)}
        <button disabled={missions.length>=16} onClick={()=>add({id:'mission',title:'New mission',mode:'individual',objectives:[{on:'conversation',label:''}]})}>Write a custom mission</button>
    </section>;
}
