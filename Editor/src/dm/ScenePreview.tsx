import {useEffect,useRef,useState} from 'react';
import {drawPortrait,Portraits} from '../../../Client/src/ui/portrait';
import type {Person} from '../model/model.mjs';
import type {Holder} from './api';
import type {Scene} from './scenes';

export type PreviewResident=Person|Holder;
let portraits:Portraits|undefined;
function SpeakerHead({person}:{person:Person}) {
    const el=useRef<HTMLCanvasElement>(null);
    useEffect(()=>{
        const ctx=el.current?.getContext('2d');
        if(!ctx)return;
        const images=portraits ??= new Portraits();
        function paint(){ctx!.clearRect(0,0,160,160);drawPortrait(ctx!,images,{gradientAmount:.35,patternAmount:.55,...person.appearance},0,0,160,160,undefined,{view:'face'});}
        paint();return images.listen(paint);
    },[person]);
    return <canvas ref={el} width={160} height={160} style={{width:80,height:80}} role="img" aria-label={person.name+'’s portrait'}/>;
}
export function ScenePreview({scene,residents,start}:{start?:string;scene:Scene;residents:PreviewResident[]}) {
    const [path,setPath]=useState<string[]>([start || scene.start]);
    const [reply,setReply]=useState('');
    const last=path.at(-1);
    const beat=scene.beats.find(b=>b.id===last);
    const cast=scene.cast.find(c=>c.id===beat?.speaker);
    const person=residents.find(p=>cast?.npc===p.id || (cast?.selector
        && (!cast.selector.role||cast.selector.role===p.role)
        && (!cast.selector.cell||[p.home?.cell,p.work?.cell].includes(cast.selector.cell))));
    function go(next?:string){setReply('');setPath([...path,next ?? '']);}
    return <section className="scene-preview"><div className="scene-section-head"><h4>Player preview</h4><button onClick={()=>{setPath([scene.start]);setReply('');}}>Restart preview</button></div>
        <p className="scene-audience">{scene.visibility==='public'?'Passing players':scene.visibility==='party'?'Initiator’s party':'Initiating player'}
            {scene.lockMovement?' · Movement locked':' · Free movement'}{scene.camera?' · Camera at '+scene.camera.cell+' ('+scene.camera.x+', '+scene.camera.y+')':''}</p>
        {beat?<><div className="scene-dialogue">
            {person && 'appearance' in person && person.appearance && <SpeakerHead person={person}/>}
            <div><small>{beat.type==='move'?'Movement':beat.type==='pose'?'Posture':beat.ai?'Authored fallback':'Speaking'}</small>
                <h4>{person?.name || cast?.npc || cast?.visitor || cast?.id || 'Choose a speaker'}</h4>
                {cast?.visitor!==undefined && <small>A stranger the story brings</small>}
                {cast?.selector && <small>{person?'Example cast · ':''}{cast.selector.role || 'Resident'}{cast.selector.cell?' in '+cast.selector.cell:''}</small>}
                <p>{'text' in beat?beat.text || 'Write the spoken line.':beat.type==='move'?beat.pace+' to '+(beat.cell||'a cell')+' ('+beat.x+', '+beat.y+')':beat.posture}</p></div>
        </div>{'seconds' in beat && <small>{beat.seconds} seconds{beat.type==='choice' && beat.mode==='majority'?' · Group vote':''}</small>}
        {beat.type==='choice'?<div className="scene-preview-buttons">{beat.options.map(o=><button key={o.id} onClick={()=>go(o.next)}>{o.label}</button>)}
            <button onClick={()=>go(beat.options.find(o=>o.id===beat.default)?.next)}>Simulate timeout</button></div>
        :beat.type==='response'?<><label>Preview reply<textarea maxLength={280} value={reply} onChange={e=>setReply(e.target.value)}/></label>
            <button disabled={!reply.trim()} onClick={()=>go(beat.next)}>{beat.mode==='all'?'Simulate all replies':'Submit preview reply'}</button>
            <button onClick={()=>go(beat.timeoutNext ?? beat.next)}>Simulate timeout</button></>
        :<button onClick={()=>go(beat.next)}>{beat.type==='line'?'Advance past line':'Complete action'}</button>}</>
        :<p>{last?'Choose a valid opening or next beat.':'Scene ended.'}</p>}
        <p className="scene-path">{path.filter(Boolean).join(' → ')}</p>
    </section>;
}
