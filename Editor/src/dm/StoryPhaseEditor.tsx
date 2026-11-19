import {useState} from 'react';

type Effect = Record<string,string | number>;
interface Exit {
    on:string; to:string; match?:Record<string,string>; afterDays?:number; percent?:number; players?:number; participant?:boolean;
}
interface Phase {guestVoices?:string[]; id:string; journal:string; effects:Effect[]; exits:Exit[]}
export interface WorldProgram {id:string; title:string; audience:{kind:string; days?:number; players?:string[]}; phases:Phase[]}
export interface ProgramCatalogue {events:string[]; effects:Record<string,string[]>; examples:Record<string,WorldProgram>}

function newExit(on:string, to:string):Exit {
    if (on === 'clock') return {on,to,afterDays:1};
    if (on === 'dm signal') return {on,to,match:{detail:'direction'}};
    return {on,to};
}

const numeric = new Set(['x','y','days','inDays','points','count','disposition']);
export function StoryPhaseEditor({program,catalog,onEdit}: {program:WorldProgram; catalog:ProgramCatalogue; onEdit:(p:WorldProgram)=>void}) {
    const [index,setIndex] = useState(0);
    const at = Math.min(index,program.phases.length-1);
    const phase = program.phases[at];
    if (!phase) return null;
    function update(patch:Partial<Phase>) {
        onEdit({...program,phases:program.phases.map((p,i)=>i===at?{...phase,...patch}:p)});
    }
    function exit(i:number, value:Exit) {update({exits:phase.exits.map((e,n)=>n===i?value:e)});}
    function effect(i:number, value:Effect) {update({effects:phase.effects.map((e,n)=>n===i?value:e)});}
    return <><label>Story ID<input value={program.id} onChange={e=>onEdit({...program,id:e.target.value})}/></label><label>Campaign title<input value={program.title} onChange={e=>onEdit({...program,title:e.target.value})}/></label>
        <label>Audience<select aria-label="Audience" value={program.audience.kind} onChange={e=>onEdit({...program,
            audience:e.target.value==='active'?{kind:'active',days:7}:{kind:'players',players:[]}})}>
            <option value="active">Recently active players</option><option value="players">Specific players</option></select></label>
        {program.audience.kind==='active' ? <label>Activity lookback in real days<input type="number" min="1" max="90"
            value={program.audience.days ?? 7} onChange={e=>onEdit({...program,audience:{kind:'active',days:e.target.valueAsNumber || 0}})}/></label>
            : <label>Player character IDs, one per line<textarea value={(program.audience.players ?? []).join('\n')}
                onChange={e=>onEdit({...program,audience:{kind:'players',players:e.target.value.split('\n')}})}/></label>}
        <label>Phase<select aria-label="Phase" value={at} onChange={e=>setIndex(Number(e.target.value))}>{program.phases.map((p,i)=><option key={i} value={i}>{p.id}</option>)}</select></label>
        <button onClick={()=>{let n=program.phases.length+1; while(program.phases.some(p=>p.id==='phase-'+n)) n++;
            onEdit({...program,phases:[...program.phases,{id:'phase-'+n,journal:'',effects:[],exits:[]}]}); setIndex(program.phases.length);
        }}>Add phase</button>
        <label>Player journal<textarea rows={3} value={phase.journal} onChange={e=>update({journal:e.target.value})}/></label>
        <h4>Phase triggers</h4>
        {!phase.exits.length && <p>This phase ends the story.</p>}
        {phase.exits.map((edge,i)=><fieldset key={i}><label>When<select value={edge.on} onChange={e=>exit(i,newExit(e.target.value,edge.to))}>
            {catalog.events.map(on=><option key={on}>{on}</option>)}</select></label>
            <label>Continue to<select value={edge.to} onChange={e=>exit(i,{...edge,to:e.target.value})}>
                {program.phases.map(p=><option key={p.id}>{p.id}</option>)}</select></label>
            {edge.on==='clock' ? <label>After game days<input type="number" min={1/1440} step="any" value={edge.afterDays ?? 1} onChange={e=>exit(i,{...edge,afterDays:e.target.valueAsNumber || 0})}/></label>
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
        <button onClick={()=>update({exits:[...phase.exits,newExit('conversation',program.phases[(at+1)%program.phases.length].id)]})}>Add trigger</button>
        <label>Optional guest speakers (resident IDs, one per line)<textarea rows={2}
            value={(phase.guestVoices ?? []).join('\n')} onChange={e=>update({guestVoices:e.target.value ? e.target.value.split('\n') : []})}/></label>
        <h4>When this phase opens</h4>{phase.effects.map((e,i)=><fieldset key={i}><legend>{e.kind}</legend>
            {(catalog.effects[e.kind] ?? []).map(key=><label key={key}>{key}<input type={numeric.has(key)?'number':'text'} value={e[key] ?? ''}
                onChange={ev=>effect(i,{...e,[key]:numeric.has(key)?Number(ev.target.value):ev.target.value})}/></label>)}
            <button onClick={()=>update({effects:phase.effects.filter((_,n)=>n!==i)})}>Remove effect</button></fieldset>)}
        <label>Add phase effect<select value="" onChange={e=>{const kind=e.target.value; const next:Effect={kind};
            for(const key of catalog.effects[kind]) next[key]=numeric.has(key)?1:'';
            update({effects:[...phase.effects,next]});}}><option value="">Choose an effect</option>
            {Object.keys(catalog.effects).map(kind=><option key={kind}>{kind}</option>)}</select></label></>;
}
