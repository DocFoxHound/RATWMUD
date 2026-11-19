import {useState} from 'react';
import {RegionTilePicker,type Region} from './StoryRegions';
export interface Fact {id:string;kind:string;day:number;elapsedDays?:number;actor?:string;target?:string;item?:string;cell?:string;x?:number;y?:number;detail?:string;quantity?:number;coins?:number}
export interface Scenario {expectEffects?:{phase:string;[key:string]:unknown}[];expectMissionVisits?:{phase:string;visit:number;credits:Record<string,string[]>}[];name:string;cohort:string[];events:Fact[];expect:string[]}
export function StoryRehearsalEditor({regions,cases,events,phases,change}:{regions:Region[];cases:Scenario[];events:string[];phases:string[];change:(cases:Scenario[])=>void}) {
    const [selected,select]=useState(0);
    const at=Math.min(selected,cases.length-1), scene=cases[at];
    function update(patch:Partial<Scenario>) {change(cases.map((c,i)=>i===at?{...c,...patch}:c));}
    function fact(i:number,patch:Partial<Fact>) {update({events:scene.events.map((f,n)=>n===i?{...f,...patch}:f)});}
    function move(from:number,to:number) {
        const events=[...scene.events];
        const [item]=events.splice(from,1);
        events.splice(to,0,item);
        update({events});
    }
    return <details><summary>Rehearsal cases</summary>
        <label>Case<select aria-label="Case" value={at} onChange={e=>select(Number(e.target.value))}>{cases.map((c,i)=><option key={i} value={i}>{c.name}</option>)}</select></label>
        <button disabled={cases.length>=32} onClick={()=>{change([...cases,{name:'New case',cohort:['player-ash'],events:[],expect:[phases[0]]}]);select(cases.length);}}>Add case</button>
        {scene && <><button disabled={cases.length===1} onClick={()=>change(cases.filter((_,i)=>i!==at))}>Remove case</button>
        <label>Case name<input value={scene.name} onChange={e=>update({name:e.target.value})}/></label>
        <label>Eligible characters, one per line<textarea value={scene.cohort.join('\n')} onChange={e=>update({cohort:e.target.value.split('\n')})}/></label>
        <label>Expected phase path, one per line<textarea value={scene.expect.join('\n')} onChange={e=>update({expect:e.target.value.split('\n')})}/></label>
        {scene.events.map((f,i)=><details key={i}><summary>{i+1}. {f.kind} · day {f.day}</summary>
            <label>World event<select aria-label="World event" value={f.kind} onChange={e=>fact(i,{kind:e.target.value})}>{events.map(k=><option key={k}>{k}</option>)}</select></label>
            <label>Game day<input type="number" min={0} step="any" value={f.day} onChange={e=>fact(i,{day:Number(e.target.value)})}/></label>
            <label>Elapsed real days<input type="number" step="any" min={0} value={f.elapsedDays ?? ''} onChange={e=>fact(i,{elapsedDays:e.target.value===''?undefined:Number(e.target.value)})}/></label>
            <RegionTilePicker regions={regions} pick={tile=>fact(i,tile)}/>
            {(f.kind==='region entered'||f.kind==='region left') && <label>Crossed region<select value={f.target ?? ''} onChange={e=>fact(i,{target:e.target.value})}>
                <option value="">Choose a region</option>{regions.map(r=><option key={r.id}>{r.id}</option>)}</select></label>}
            {(['actor','target','cell','item','detail'] as const).filter(k=>k!=='target' || (f.kind!=='region entered' && f.kind!=='region left')).map(k=><label key={k}>{k}<input value={f[k] ?? ''} onChange={e=>fact(i,{[k]:e.target.value})}/></label>)}
            {(['x','y'] as const).map(k=><label key={k}>Event tile {k.toUpperCase()}<input type="number" min={0} max={4095} step={1} value={f[k] ?? ''}
                onChange={e=>fact(i,{[k]:e.target.value===''?undefined:Number(e.target.value)})}/></label>)}
            {(['quantity','coins'] as const).map(k=><label key={k}>{k}<input type="number" value={f[k] ?? 0} onChange={e=>fact(i,{[k]:Number(e.target.value)})}/></label>)}
            <button disabled={i===0} onClick={()=>move(i,i-1)}>Move fact earlier</button>
            <button disabled={i===scene.events.length-1} onClick={()=>move(i,i+1)}>Move fact later</button>
            <button onClick={()=>update({events:scene.events.filter((_,n)=>n!==i)})}>Remove fact</button></details>)}
        <button disabled={scene.events.length>=256} onClick={()=>{
            let n=scene.events.length+1; while(scene.events.some(f=>f.id==='fact-'+n)) n++;
            update({events:[...scene.events,{id:'fact-'+n,kind:'conversation',day:scene.events.at(-1)?.day ?? 0,actor:scene.cohort[0] ?? ''}]});
        }}>Add rehearsal fact</button></>}
    </details>;
}
