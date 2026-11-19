import {useId} from 'react';
import type {Scene} from './scenes';
import type {Effect} from './StoryEffectFields';

const cueKinds:Record<string,string>={
    'scene arrived':'npc.walk','scene blocked':'npc.walk','scene spoken':'npc.speak','scene posed':'npc.pose',
    'scene finished':'scene.play','scene failed':'scene.play','scene choice':'scene.play','scene response':'scene.play'
};
export const cueEvent=(on:string)=>Object.hasOwn(cueKinds,on);
export function SceneCueFilter({on,effects,scenes,match,change}:{on:string;effects:Effect[];scenes:Scene[];
    match?:Record<string,string>;change:(match:Record<string,string>)=>void}) {
    const list=useId();
    const candidates=new Set(effects.filter(e=>e.kind===cueKinds[on] && typeof e.cue==='string' && e.cue).flatMap(e=>{
        const cue=String(e.cue);
        if(on!=='scene choice' && on!=='scene response')return [cue];
        return scenes.find(s=>s.id===e.scene)?.beats.flatMap(b=>{
            if(on==='scene choice' && b.type==='choice')return b.options.map(o=>cue+'.'+o.id);
            if(on==='scene response' && b.type==='response')return [cue+'.'+b.id+'.submitted',cue+'.'+b.id+'.timeout'];
            return [];
        }) ?? [];
    }));
    return <><label>Scene cue to wait for<input aria-label="Scene cue to wait for" list={list} value={match?.detail ?? ''} placeholder="Any cue" onChange={ev=>{
        const next={...match};if(ev.target.value)next.detail=ev.target.value;else delete next.detail;change(next);
    }}/><datalist id={list}>{Array.from(candidates,c=><option key={c} value={c}/>)}</datalist></label>
    {on==='scene response' && <p className="hint">Choose <code>cue.beat.submitted</code> when the required players reply, or <code>cue.beat.timeout</code> when the deadline passes. Triggers receive only the outcome.</p>}
    {on==='scene choice' && <p className="hint">Choose <code>cue.option</code> to follow the selected answer.</p>}</>;
}
