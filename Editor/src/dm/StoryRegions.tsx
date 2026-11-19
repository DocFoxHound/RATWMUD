import {useMemo, useRef, useState} from 'react';
import type {Place, Project} from '../model/model.mjs';
import {surfaceFor} from '../lib/surface';
import {MapView} from './MapView';

export interface Region {id:string; cell:string; x1:number; y1:number; x2:number; y2:number}
type Bounds = Pick<Region,'x1'|'y1'|'x2'|'y2'>;
const axes = ['x1','y1','x2','y2'] as const;
const regionId = /^[A-Za-z0-9_][A-Za-z0-9_.:-]{0,79}$/;
export function editableRegion(r:Region | null) {
    return !!r && typeof r.id==='string' && typeof r.cell==='string' && axes.every(k=>Number.isInteger(r[k]));
}
function corners(start:Place, end:Place):Bounds {
    return {x1:Math.min(start.x,end.x),y1:Math.min(start.y,end.y),x2:Math.max(start.x,end.x),y2:Math.max(start.y,end.y)};
}
export function RegionTilePicker({regions,pick}:{regions:Region[];pick:(tile:Place)=>void}) {
    if(!regions.length)return null;
    return <label>Copy center tile from region<select value="" onChange={ev=>{
        const r=regions.find(r=>r.id===ev.target.value);
        if(r)pick({cell:r.cell,x:Math.floor((r.x1+r.x2)/2),y:Math.floor((r.y1+r.y2)/2)});
    }}><option value="">Choose a region</option>{regions.map(r=><option key={r.id}>{r.id}</option>)}</select></label>;
}

export function StoryRegions({world,regions,change,used,single=false}:{
    single?:boolean; world:Project|null; regions:Region[]; change:(next:Region[],rename?:[string,string])=>void; used:(name:string)=>boolean
}) {
    const [index,setIndex]=useState(0);
    const [open,setOpen]=useState(false);
    const [preview,setPreview]=useState<Bounds|null>(null);
    const anchor=useRef<Place|null>(null);
    const cells=useMemo(()=>world?[...world.cells,...world.rooms].sort((a,b)=>a.name.localeCompare(b.name)):[],[world]);
    const at=Math.min(index,regions.length-1);
    const area=regions[at];
    const cell=cells.find(c=>c.id===area?.cell);
    const surface=useMemo(()=>world && cell?surfaceFor(world,{kind:'cell',id:cell.id}):null,[world,cell]);
    const current=preview ?? area;
    const inUse=area && used(area.id);
    function update(patch:Partial<Region>) {
        change(regions.map((r,i)=>i===at?{...r,...patch}:r),
            patch.id!==undefined && patch.id!==area.id?[area.id,patch.id]:undefined);
    }
    function stopDrawing() {anchor.current=null;setPreview(null);}
    function startArea() {
        let suffix=1;
        while(regions.some(r=>r.id==='area-'+suffix)) suffix++;
        const cellId=area?.cell || world?.cells[0]?.id || cells[0]?.id || '';
        change([...regions,{id:'area-'+suffix,cell:cellId,x1:0,y1:0,x2:0,y2:0}]);
        stopDrawing();setIndex(regions.length);
    }
    function setCell(value:string) {
        const next=cells.find(c=>c.id===value);
        stopDrawing();
        update(next?{cell:value,x1:Math.min(area.x1,next.width-1),x2:Math.min(area.x2,next.width-1),
            y1:Math.min(area.y1,next.height-1),y2:Math.min(area.y2,next.height-1)}:{cell:value});
    }
    return <details className="story-regions" onToggle={e=>setOpen(e.currentTarget.open)}>
        <summary>{single?'Public viewing area':'Map regions · '+regions.length}</summary>
        {open && <><div className="region-toolbar">
            {!single && area && <label>Selected region<select value={at} onChange={e=>{stopDrawing();setIndex(Number(e.target.value));}}>
                {regions.map((r,i)=><option key={i} value={i}>{r.id}</option>)}</select></label>}
            {!single && <button disabled={regions.length>=128} onClick={startArea}>Add region</button>}
        </div>
        {area && <><div className="region-toolbar">{!single && <label>Region name<input key={area.id} defaultValue={area.id} maxLength={80}
            onChange={e=>e.target.setCustomValidity('')} onBlur={e=>{
                const id=e.target.value.trim();
                if(!regionId.test(id) || regions.some((r,i)=>i!==at && r.id===id)) {
                    e.target.setCustomValidity('Enter a unique region ID of letters, numbers, underscores, periods, colons or hyphens.');
                    e.target.reportValidity();return;
                }
                if(id!==area.id)update({id});
            }}/></label>}
            <label>Map cell or interior<input list="story-region-places" value={area.cell} onChange={e=>setCell(e.target.value)}/>
                <datalist id="story-region-places">{cells.map(c=><option value={c.id} key={c.id}>{c.name}</option>)}</datalist></label></div>
            {surface && cell ? <>
                <div className="region-sheet-heading"><strong>{cell.name}</strong><small>{cell.width} × {cell.height} tiles</small></div>
                <div className="region-sheet"><MapView showElevationControl={false} surface={surface} markers={[]} selected={null} mode="paint"
                    overlay={{paths:[],tiles:[],rects:regions.flatMap((r,i)=>{
                        if(r.cell!==cell.id)return [];
                        const b=i===at?current:r;
                        return [{x:b.x1,y:b.y1,label:r.id,w:b.x2-b.x1+1,h:b.y2-b.y1+1,color:i===at?'#ffd277':'#71bdc0',strong:i===at}];
                    })}} onClick={()=>{}} onOpen={()=>{}}
                    onPaint={(place,start)=>{
                        if(start)anchor.current=place;
                        if(place && anchor.current)setPreview(corners(anchor.current,place));
                    }} onPaintEnd={place=>{
                        if(place && anchor.current)update(corners(anchor.current,place));
                        stopDrawing();
                    }}/>
                </div>
                <p className="region-hint">Drag between two tiles, or enter the corners below. Scroll to zoom. Right-drag to pan.</p>
            </>:<p className="region-hint">Choose a cell in this world to draw on its map. You can also enter the cell ID and tile coordinates.</p>}
            <div className="region-corners">{axes.map(k=><label key={k}>{({x1:'Left X',y1:'Top Y',x2:'Right X',y2:'Bottom Y'})[k]}
                <input type="number" min={0} max={cell?(k[0]==='x'?cell.width:cell.height)-1:4095} step={1} value={current[k]}
                    onChange={e=>update({[k]:Number(e.target.value)})}/></label>)}</div>
            <div className="region-footer"><small>{current.x2>=current.x1 && current.y2>=current.y1
                ? (current.x2-current.x1+1)+' × '+(current.y2-current.y1+1)+' tiles, including both corners'
                : 'The right and bottom corners must follow the left and top corners.'}</small>
                {!single && <button disabled={inUse} onClick={()=>{stopDrawing();change(regions.filter((_,i)=>i!==at));}}>Remove region</button>}</div>
            {!single && inUse && <small>Remove its condition or action references to delete this region.</small>}
        </>}</>}
    </details>;
}
