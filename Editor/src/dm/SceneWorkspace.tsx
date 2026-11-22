import {useEffect,useMemo,useRef,useState} from 'react';
import type {Project} from '../model/model.mjs';
import {download} from '../lib/api';
import {storyProgram,worldName,type Npcs,type Target} from './api';
import {SceneBeatEditor,IdInput} from './SceneBeatEditor';
import {ScenePreview} from './ScenePreview';
import {StoryRegions} from './StoryRegions';
import {loadScenes,newScene,uniqueId,type CastRole,type Scene} from './scenes';
import type {Program as WorldProgram} from '../lib/workbench';

interface Props {target:Target;libraryKey:string;world:Project|null;roster:Npcs|null;canDirect:boolean;openProgram:(program:WorldProgram)=>void}
export function SceneWorkspace({target,libraryKey,world,roster,canDirect,openProgram}:Props) {
    const [initial]=useState(()=>{try{return {docs:loadScenes(libraryKey),error:''};}
        catch(e){return {docs:[],error:(e as Error).message};}});
    const [docs,setDocs]=useState<Scene[]>(initial.docs.length?initial.docs:[newScene('scene-1')]);
    const [index,select]=useState(()=>{try{return Math.max(0,docs.findIndex(s=>s.id===localStorage.getItem(libraryKey+'.selected')));}catch{return 0;}});
    const [examples,setExamples]=useState<Record<string,Scene>>({});
    const [error,report]=useState(initial.error),[status,setStatus]=useState('');
    const [busy,setBusy]=useState(false),[player,setPlayer]=useState('');
    const [previewVersion,refreshPreview]=useState(0),[previewBeat,setPreviewBeat]=useState('');
    const monitor=useRef<HTMLElement>(null);
    const [review,setReview]=useState<{program:WorldProgram;digest:string;passed:boolean}|null>(null);
    const [ticket,setTicket]=useState('');
    const scene=docs[index];
    const worldLabel=worldName(target);
    const loadVersion=useRef(0);
    const storageRaw=useRef<string|null>(null);
    useEffect(()=>{
        let active=true;
        try{storageRaw.current=localStorage.getItem(libraryKey);}catch{/* Edits remain available for export. */}
        storyProgram<{sceneExamples:Record<string,Scene>}>(target,'catalog').then(c=>{if(active)setExamples(c.sceneExamples ?? {});})
            .catch(e=>{if(active)report(e.message);});return ()=>{active=false;};
    },[target,libraryKey]);
    const residents=useMemo(()=>[...new Map([...(world?.people ?? []),...(roster?.people ?? []),...(roster?.holders ?? [])]
        .filter(p=>!roster?.dead.includes(p.id)).map(p=>[p.id,p])).values()].sort((a,b)=>a.id.localeCompare(b.id)),[world,roster]);
    const places=[...(world?.cells ?? []),...(world?.rooms ?? [])];
    function clear(){setStatus('');setReview(null);setTicket('');}
    function save(next:Scene[],i=index) {
        try{
            if(localStorage.getItem(libraryKey)!==storageRaw.current){report('Another tab edited the scene library. Reload it before continuing.');return;}
            const raw=JSON.stringify(next);localStorage.setItem(libraryKey,raw);storageRaw.current=raw;report('');
        }catch{report('Browser storage is unavailable. Export the scene to keep a copy.');}
        clear();setDocs(next);select(i);loadVersion.current++;
    }
    function edit(next:Scene){save(docs.map((s,i)=>i===index?next:s));}
    function switchScene(i:number){setPreviewBeat('');select(i);clear();report('');refreshPreview(v=>v+1);loadVersion.current++;}
    function add(s:Scene) {
        setPreviewBeat('');
        const at=docs.findIndex(v=>v.id===s.id);
        if(at>=0 && !window.confirm('Replace the saved scene with this version?'))return;
        save(at<0?[...docs,s]:docs.map((old,i)=>i===at?s:old),at<0?docs.length:at);refreshPreview(v=>v+1);
    }
    function role(i:number,c:CastRole){edit({...scene,cast:scene.cast.map((r,j)=>j===i?c:r)});}
    async function work(fn:()=>Promise<void>){setBusy(true);report('');try{await fn();}catch(e){report((e as Error).message);}finally{setBusy(false);}}
    useEffect(()=>{try{localStorage.setItem(libraryKey+'.selected',scene.id);}catch{/* Selection lasts for this session. */}},[scene.id,libraryKey]);
    const changeAudience=(visibility:Scene['visibility'])=>edit({...scene,visibility,area:visibility==='public'?scene.area ?? {cell:places[0]?.id || '',x1:0,y1:0,x2:0,y2:0}:undefined});
    return <div className="scene-layout"><section className="dm-panel scene-workbench"><h3>Scene library</h3>
        <fieldset disabled={busy}><div className="scene-library-tools">
            <label>Saved scene<select value={index} onChange={e=>switchScene(Number(e.target.value))}>{docs.map((s,i)=><option key={i} value={i}>{s.title} · {s.id}</option>)}</select></label>
            <button onClick={()=>add(newScene(uniqueId('scene',docs.map(s=>s.id))))}>New scene</button>
            <label>Example scene<select value="" onChange={e=>{if(examples[e.target.value])add(structuredClone(examples[e.target.value]));}}>
                <option value="">Choose an example</option>{Object.entries(examples).map(([id,s])=><option key={id} value={id}>{s.title}</option>)}</select></label>
            <label>Import scene<input type="file" accept=".json,application/json" onChange={e=>{const f=e.target.files?.[0];e.target.value='';if(f)void work(async()=>{
                if(f.size>1024*1024)throw Error('Choose a scene under 1 MiB.');const version=loadVersion.current;
                const s=JSON.parse(await f.text()) as Scene;await storyProgram(target,'scene.validate',{scene:s});
                if(version!==loadVersion.current)throw Error('The library changed during import. Choose the file again.');add(s);
            });}}/></label>
            <button onClick={()=>download(scene.id+'.scene.json',JSON.stringify(scene,null,2))}>Export scene</button>
        </div>
        <div className="scene-fields"><label>Scene title<input value={scene.title} maxLength={100} onChange={e=>edit({...scene,title:e.target.value})}/></label>
            <IdInput name="Scene ID" value={scene.id} others={docs.filter((_,i)=>i!==index).map(s=>s.id)} change={id=>edit({...scene,id})}/>
            <label>Visibility<select value={scene.visibility} onChange={e=>changeAudience(e.target.value as Scene['visibility'])}>
                <option value="individual">Private · initiating player</option><option value="party">Private · initiating party</option>
                <option value="public">Public · anyone in the area</option></select></label></div>
        <datalist id="scene-cells">{places.map(c=><option key={c.id} value={c.id}>{c.name}</option>)}</datalist>
        <datalist id="scene-residents">{residents.map(p=><option key={p.id} value={p.id}>{p.name}</option>)}</datalist>
        <div className="scene-options"><label><input type="checkbox" checked={scene.lockMovement} onChange={e=>edit({...scene,lockMovement:e.target.checked})}/>Lock viewer movement</label>
            <label><input type="checkbox" checked={!!scene.camera} onChange={e=>edit({...scene,camera:e.target.checked?{cell:scene.area?.cell || places[0]?.id || '',x:0,y:0}:undefined})}/>Focus viewer camera</label></div>
        {scene.camera && <div className="scene-fields"><label>Camera cell<input list="scene-cells" value={scene.camera.cell} onChange={e=>edit({...scene,camera:{...scene.camera!,cell:e.target.value}})}/></label>
            {(['x','y'] as const).map(k=><label key={k}>Camera {k.toUpperCase()}<input type="number" min={0} max={4095} value={scene.camera![k]} onChange={e=>edit({...scene,camera:{...scene.camera!,[k]:Number(e.target.value)}})}/></label>)}</div>}
        {scene.visibility==='public' && scene.area && <StoryRegions world={world} regions={[{id:'audience',...scene.area}]}
            single change={r=>{if(r[0]){const {cell,x1,y1,x2,y2}=r[0];edit({...scene,area:{cell,x1,y1,x2,y2}});}}} used={()=>true}/>}
        <section><div className="scene-section-head"><h4>Cast</h4><button disabled={scene.cast.length>=16} onClick={()=>edit({...scene,cast:[...scene.cast,{id:uniqueId('role',scene.cast.map(c=>c.id)),npc:''}]})}>Add cast role</button></div>
            {scene.cast.map((c,i)=><fieldset key={i}><legend>{c.id}</legend><div className="scene-fields">
                <IdInput name="Cast role ID" value={c.id} others={scene.cast.filter((_,j)=>j!==i).map(r=>r.id)} change={id=>
                    edit({...scene,cast:scene.cast.map((r,j)=>j===i?{...r,id}:r),beats:scene.beats.map(b=>b.speaker===c.id?{...b,speaker:id}:b)})}/>
                <label>Cast from<select value={c.selector?'selector':'npc'} onChange={e=>role(i,e.target.value==='npc'?{id:c.id,npc:''}:{id:c.id,selector:{role:'guard'}})}>
                    <option value="npc">Named resident</option><option value="selector">Resident matching conditions</option></select></label>
                {c.selector?<><label>Resident role<input value={c.selector.role || ''} placeholder="Any role" onChange={e=>role(i,{...c,selector:{...c.selector,role:e.target.value || undefined}})}/></label>
                    <label>Resident in cell<input list="scene-cells" value={c.selector.cell || ''} placeholder="Any cell" onChange={e=>role(i,{...c,selector:{...c.selector,cell:e.target.value || undefined}})}/></label></>
                :<label>Resident ID<input list="scene-residents" value={c.npc ?? ''} onChange={e=>role(i,{id:c.id,npc:e.target.value})}/></label>}</div>
                {scene.cast.length>1 && <button disabled={scene.beats.some(b=>b.speaker===c.id)} onClick={()=>edit({...scene,cast:scene.cast.filter((_,j)=>j!==i)})}>Remove cast role</button>}</fieldset>)}</section>
        <label>Opening beat<select value={scene.start} onChange={e=>edit({...scene,start:e.target.value})}>{scene.beats.map(b=><option key={b.id}>{b.id}</option>)}</select></label>
        <SceneBeatEditor key={index} scene={scene} edit={edit} preview={id=>{setPreviewBeat(id);refreshPreview(v=>v+1);
            monitor.current?.scrollIntoView({block:'center'});}}/>
        </fieldset></section><aside ref={monitor} className="dm-panel scene-monitor">
        <ScenePreview key={index+'-'+previewVersion+'-'+scene.start} start={previewBeat} scene={scene} residents={residents}/>
        <section><h4>Standalone release</h4>
            {scene.visibility!=='public' && <label>Initiating character ID<input disabled={busy} value={player} placeholder="Player character ID" onChange={e=>{clear();setPlayer(e.target.value);}}/></label>}
            <p className="hint">{scene.visibility==='public'?'Publishing starts this scene for viewers inside its area.':player?'Publishing starts this scene for the named character or their party.':'With no character selected, the first eligible arrival starts this scene.'}</p>
            <button disabled={busy} onClick={()=>void work(async()=>{
                const {program}=await storyProgram<{program:WorldProgram}>(target,'scene.wrap',{scene,...(player && scene.visibility!=='public'?{player}:{})});
                const r=await storyProgram<{digest:string;passed:boolean;uncovered:string[]}>(target,'rehearse',{program});
                setReview({program,digest:r.digest,passed:r.passed && !r.uncovered.length});setTicket('');
                setStatus(r.passed && !r.uncovered.length?'Rehearsal passed.':'Review the rehearsal cases in World stories.');
            })}>Rehearse standalone release</button>
            {review && <><p>Audience · {review.program.audience.kind==='active'?'Recently active characters':review.program.audience.players?.join(', ')}</p>
                <button disabled={busy} onClick={()=>openProgram(review.program)}>Open release in World stories</button>
                <button disabled={busy || !canDirect || !review.passed || !!ticket} onClick={()=>void work(async()=>{
                    const r=await storyProgram<{ticket:string}>(target,'approve',{program:review.program,digest:review.digest});
                    setTicket(r.ticket);setStatus('Approved for '+worldLabel+'.');
                })}>Approve standalone scene</button></>}
            {ticket && <button disabled={busy || !canDirect} onClick={()=>void work(async()=>{
                const r=await storyProgram<{run:string}>(target,'publish',{ticket});setTicket('');setReview(null);setStatus('Published on '+worldLabel+' · '+r.run);
            })}>Publish approved scene</button>}
            {status && <p role="status">{status}</p>}
        </section>{error && <p role="alert">{error}</p>}
    </aside></div>;
}
