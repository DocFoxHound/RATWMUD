import {useState} from 'react';
import {beatName,makeBeat,redirect,uniqueId,type Beat,type Scene} from './scenes';

export function IdInput({name,value,others,change}:{name:string;value:string;others:string[];change:(v:string)=>void}) {
    return <label>{name}<input key={value} defaultValue={value} onChange={e=>e.target.setCustomValidity('')} onBlur={e=>{
        const id=e.target.value.trim();
        if(!/^[A-Za-z0-9_][A-Za-z0-9_.:-]{0,79}$/.test(id)||others.includes(id)){
            e.target.setCustomValidity('Use a unique identifier of 1–80 letters, numbers, underscores, periods, colons or hyphens.');e.target.reportValidity();return;
        }
        if(value!==id)change(id);
    }}/></label>;
}
export function SceneBeatEditor({scene,edit,preview}:{scene:Scene;preview:(id:string)=>void;edit:(s:Scene)=>void}) {
    const [selected,select]=useState(0);
    const i=Math.min(selected,scene.beats.length-1), beat=scene.beats[i];
    function write(b:Beat){edit({...scene,beats:scene.beats.map((v,j)=>j===i?b:v)});}
    function nextPicker(label:string,value:string|undefined,change:(next:string|undefined)=>void,empty='End scene') {
        return <label>{label}<select value={value ?? ''} onChange={e=>change(e.target.value || undefined)}>
            <option value="">{empty}</option>{scene.beats.filter(b=>b.id!==beat.id).map(b=><option key={b.id}>{b.id}</option>)}</select></label>;
    }
    function add() {
        const id=uniqueId('beat',scene.beats.map(b=>b.id));
        const beats=[...scene.beats,makeBeat('line',id,beat?.speaker || scene.cast[0]?.id || '')];
        if(beat && beat.type!=='choice' && !beat.next)beats[i]={...beat,next:id};
        edit({...scene,beats});select(beats.length-1);
    }
    return <section className="scene-beats"><div className="scene-section-head"><h4>Scene beats</h4><button disabled={scene.beats.length>=64} onClick={add}>Add beat</button></div>
        <div className="scene-beat-list" role="group" aria-label="Scene beats">{scene.beats.map((b,j)=><button key={b.id} aria-pressed={i===j} onClick={()=>select(j)}>
            <span>{beatName[b.type]} · {b.id}</span><small>{'text' in b?b.text || 'Write dialogue':b.type==='move'?b.pace+' to '+(b.cell || '…')+' ('+b.x+', '+b.y+')':b.posture}</small>
            <small>{b.type==='choice'?b.options.map(o=>o.label+' → '+(o.next ?? 'end')).join(' · '):'→ '+(b.next ?? 'end')}</small></button>)}</div>
        {beat && <fieldset><legend>{beatName[beat.type]} · {beat.id}</legend><div className="scene-fields">
            <IdInput name="Beat ID" value={beat.id} others={scene.beats.filter((_,j)=>j!==i).map(b=>b.id)} change={id=>{
                const s=redirect(scene,beat.id,id);edit({...s,beats:s.beats.map((b,j)=>i===j?{...b,id}:b)});
            }}/>
            <label>Beat type<select value={beat.type} onChange={e=>{
                const b=makeBeat(e.target.value as Beat['type'],beat.id,beat.speaker);
                if('text' in beat && 'text' in b){b.text=beat.text;b.seconds=beat.seconds;b.ai=beat.ai;}
                if(b.type!=='choice' && beat.type!=='choice')b.next=beat.next;
                write(b);
            }}>{Object.entries(beatName).map(([id,name])=><option key={id} value={id}>{name}</option>)}</select></label>
            <label>Acting role<select value={beat.speaker} onChange={e=>write({...beat,speaker:e.target.value})}>
                {scene.cast.map(c=><option key={c.id}>{c.id}</option>)}</select></label></div>
            {'text' in beat && <><label>{beat.ai?'Authored fallback line':'Spoken text'}<textarea rows={3} maxLength={400} value={beat.text} onChange={e=>write({...beat,text:e.target.value})}/></label>
                <label>Time on screen in seconds<input type="number" min={1} max={600} value={beat.seconds} onChange={e=>write({...beat,seconds:Number(e.target.value)})}/></label></>}
            {'text' in beat && <fieldset><legend>Speaker delivery</legend><label><input type="checkbox" checked={!!beat.ai}
                onChange={e=>write({...beat,ai:e.target.checked?{personality:'',intent:''}:undefined})}/>Use linked AI for this line</label>
                {beat.ai && <><label>Personality<textarea maxLength={600} value={beat.ai.personality} onChange={e=>write({...beat,ai:{...beat.ai!,personality:e.target.value}})}/></label>
                <label>Response intent<textarea maxLength={600} value={beat.ai.intent} onChange={e=>write({...beat,ai:{...beat.ai!,intent:e.target.value}})}/></label>
                <p className="hint">The linked AI can use player replies from this scene. The preview and offline game use the authored fallback.</p></>}</fieldset>}
            {beat.type==='response' && <><label>Whose written answer is required<select value={beat.mode} onChange={e=>write({...beat,mode:e.target.value as 'first'|'all'})}>
                <option value="first">First participant to reply</option><option value="all">Everyone present at the start of this beat</option></select></label>
                {nextPicker('On timeout (defaults to next beat)',beat.timeoutNext,next=>write({...beat,timeoutNext:next}),'Follow next beat')}</>}
            {beat.type==='move' && <div className="scene-fields"><label>Destination cell<input list="scene-cells" value={beat.cell} onChange={e=>write({...beat,cell:e.target.value})}/></label>
                {(['x','y'] as const).map(k=><label key={k}>Destination {k.toUpperCase()}<input type="number" min={0} max={4095} value={beat[k]} onChange={e=>write({...beat,[k]:Number(e.target.value)})}/></label>)}
                <label>Pace<select value={beat.pace} onChange={e=>write({...beat,pace:e.target.value as 'walk'|'run'})}><option>walk</option><option>run</option></select></label></div>}
            {beat.type==='pose' && <label>Posture<select value={beat.posture} onChange={e=>write({...beat,posture:e.target.value as typeof beat.posture})}>{['standing','sitting','lying'].map(p=><option key={p}>{p}</option>)}</select></label>}
            {beat.type==='choice'?<>
                <label>Decision rule<select value={beat.mode} onChange={e=>write({...beat,mode:e.target.value as 'first'|'majority'})}><option value="first">First eligible answer</option><option value="majority">Group vote</option></select></label>
                {beat.mode==='majority' && <p className="hint">A majority ends the vote early. At timeout the most votes win. A tie or no votes selects the default.</p>}
                {beat.options.map((o,n)=><fieldset key={n}><legend>Answer {n+1}</legend><div className="scene-fields">
                    <IdInput name="Option ID" value={o.id} others={beat.options.filter((_,j)=>n!==j).map(v=>v.id)} change={id=>
                        write({...beat,default:beat.default===o.id?id:beat.default,options:beat.options.map((v,j)=>n===j?{...v,id}:v)})}/>
                    <label>Answer label<input maxLength={80} value={o.label} onChange={e=>write({...beat,options:beat.options.map((v,j)=>n===j?{...v,label:e.target.value}:v)})}/></label>
                    {nextPicker('Answer leads to',o.next,next=>write({...beat,options:beat.options.map((v,j)=>j===n?{...v,next}:v)}))}</div>
                    <button disabled={beat.options.length<=2} onClick={()=>{const options=beat.options.filter((_,j)=>n!==j);
                        write({...beat,options,default:beat.default===o.id?options[0].id:beat.default});}}>Remove answer</button></fieldset>)}
                <button disabled={beat.options.length>=8} onClick={()=>write({...beat,options:[...beat.options,{id:uniqueId('option',beat.options.map(o=>o.id)),label:'New answer'}]})}>Add answer</button>
                <label>Default answer<select value={beat.default} onChange={e=>write({...beat,default:e.target.value})}>{beat.options.map(o=><option key={o.id} value={o.id}>{o.label}</option>)}</select></label>
            </>:nextPicker('Next beat',beat.next,next=>write({...beat,next}))}
            <button onClick={()=>preview(beat.id)}>Preview this beat</button>
            <button disabled={scene.beats.length<=1} onClick={()=>{if(window.confirm('Remove this beat? Paths leading to it will end.')){
                const s=redirect(scene,beat.id,undefined);edit({...s,beats:s.beats.filter(b=>b.id!==beat.id)});
            }}}>Remove beat</button>
        </fieldset>}
    </section>;
}
