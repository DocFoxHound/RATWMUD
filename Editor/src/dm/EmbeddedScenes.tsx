import {useRef,useState} from 'react';
import {storyProgram,type Target} from './api';
import {loadScenes,type Scene} from './scenes';
import type {WorldProgram} from './StoryPhaseEditor';

export function EmbeddedScenes({target,program,libraryKey,examples,edit}:{target:Target;program:WorldProgram;libraryKey:string;examples:Record<string,Scene>;edit:(p:WorldProgram)=>void}) {
    const current=useRef(program);current.current=program;
    const [choice,select]=useState(''),[error,report]=useState(''),[busy,setBusy]=useState(false);
    const [saved]=useState(()=>{try{return loadScenes(libraryKey);}catch{return [];}});
    const docs=Object.values({...examples,...Object.fromEntries(saved.map(s=>[s.id,s]))});
    const selected=docs.find(s=>s.id===choice) ?? docs[0];
    const embedded=program.scenes ?? [];
    return <details className="phase-tools"><summary>Embedded scenes · {embedded.length}</summary>
        {embedded.map(s=><details key={s.id}><summary>{s.title} · {s.id}</summary>
            <p>{s.visibility} · {s.beats.length} beats · {s.lockMovement?'Locks viewer movement':'Free movement'}</p>
            {s.beats.map(b=><p key={b.id}>{b.id} · {b.speaker} · {'text' in b?b.text:b.type}
                {'ai' in b && b.ai && <small> · Linked AI · {b.ai.personality} / {b.ai.intent}</small>}</p>)}</details>)}
        <label>Scene from library<select value={selected?.id ?? ''} onChange={e=>select(e.target.value)}>{docs.map(s=><option key={s.id} value={s.id}>{s.title} · {s.id}</option>)}</select></label>
        <button disabled={!selected || busy} onClick={async()=>{
            if(!selected)return;
            const prior=embedded.some(s=>s.id===selected.id);
            if(prior && !window.confirm('Replace the embedded copy with this version?'))return;
            setBusy(true);report('');
            try {
                await storyProgram(target,'scene.validate',{scene:selected});
                if(current.current!==program)throw Error('The world story changed. Embed the scene again.');
                edit({...program,scenes:prior?embedded.map(s=>s.id===selected.id?structuredClone(selected):s):[...embedded,structuredClone(selected)]});
            } catch(e){report((e as Error).message);}finally{setBusy(false);}
        }}>{embedded.some(s=>s.id===selected?.id)?'Update embedded scene':'Embed scene'}</button>
        {embedded.length>0 && <p className="hint">Choose Play a scene in a trigger action or phase entry effect.</p>}
        {error && <p role="alert">{error}</p>}
    </details>;
}
