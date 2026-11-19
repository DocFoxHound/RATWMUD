import type {Place} from '../model/model.mjs';
import type {Region} from './StoryRegions';

export interface CastRole {id:string; npc?:string; selector?:{role?:string;cell?:string}}
export interface Choice {id:string;label:string;next?:string}
interface AI {personality:string;intent:string}
interface BeatBase {id:string;speaker:string}
export type Beat =
    | BeatBase & {type:'line';ai?:AI;text:string;seconds:number;next?:string}
    | BeatBase & {type:'choice';ai?:AI;text:string;seconds:number;mode:'first'|'majority';options:Choice[];default:string}
    | BeatBase & {type:'response';ai?:AI;text:string;seconds:number;mode:'first'|'all';next?:string;timeoutNext?:string}
    | BeatBase & Place & {type:'move';pace:'walk'|'run';next?:string}
    | BeatBase & {type:'pose';posture:'standing'|'sitting'|'lying';next?:string};
export interface Scene {
    format:'ratw-scene'; version:1; id:string;title:string;visibility:'public'|'individual'|'party';
    lockMovement:boolean;area?:Omit<Region,'id'>;camera?:Place;cast:CastRole[];start:string;beats:Beat[];
}
export const beatName:Record<Beat['type'],string>={line:'Dialogue',response:'Written response',choice:'Player choice',move:'Movement',pose:'Posture'};
export function makeBeat(type:Beat['type'],id:string,speaker:string):Beat {
    if(type==='response')return {type,id,speaker,text:'',seconds:30,mode:'first'};
    if(type==='choice')return {type,id,speaker,text:'',seconds:20,mode:'first',default:'accept',
        options:[{id:'accept',label:'Accept'},{id:'decline',label:'Decline'}]};
    if(type==='move')return {type,id,speaker,cell:'',x:0,y:0,pace:'walk'};
    if(type==='pose')return {type,id,speaker,posture:'standing'};
    return {type,id,speaker,text:'',seconds:5};
}
export function uniqueId(prefix:string,ids:string[]) {
    let n=1;while(ids.includes(prefix+'-'+n))n++;return prefix+'-'+n;
}
export function newScene(id:string):Scene {
    return {format:'ratw-scene',version:1,id,title:'New scene',visibility:'individual',lockMovement:false,
        cast:[{id:'speaker',npc:''}],start:'opening',beats:[makeBeat('line','opening','speaker')]};
}
export function loadScenes(key:string):Scene[] {
    const raw=localStorage.getItem(key);if(!raw)return [];
    const docs:unknown=JSON.parse(raw);
    if(!Array.isArray(docs) || !docs.every(s=>s?.format==='ratw-scene' && s.version===1 && typeof s.id==='string' && Array.isArray(s.cast) && Array.isArray(s.beats)))throw Error('The saved scene library could not be read.');
    return docs;
}
export function redirect(scene:Scene,from:string,to:string|undefined):Scene {
    const dest=(s:string|undefined)=>s===from?to:s;
    return {...scene,start:dest(scene.start) ?? scene.beats.find(b=>b.id!==from)?.id ?? '',
        beats:scene.beats.map(b=>b.type==='choice'?{...b,options:b.options.map(o=>({...o,next:dest(o.next)}))}:b.type==='response'?{...b,next:dest(b.next),timeoutNext:dest(b.timeoutNext)}:{...b,next:dest(b.next)})};
}
