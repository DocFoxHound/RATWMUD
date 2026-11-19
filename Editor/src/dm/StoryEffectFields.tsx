import {RegionTilePicker,type Region} from './StoryRegions';
import {InjuryTypes} from './api';
export type Effect = Record<string,string|number>;
const names:Record<string,string>={
    'npc.line':'Give a resident a dialogue line','npc.move':'Move a resident','npc.protect':'Protect a resident',
    'npc.unprotect':'Remove resident protection','npc.revive':'Revive a resident','character.injury':'Give a character an injury',
    'ship.storm':'Delay a ship with a storm','harvest.override':'Change a harvest','band.override':'Change household prosperity',
    'festival.call':'Call a festival','storyline.give':'Give a personal quest','faction.relation':'Change faction relations',
    'bandits.call':'Call bandits','story.practice':'Grant skill practice','times.nudge':'Change market conditions',
    'deed.award':'Award a deed','project.post':'Post a town project',
    'npc.walk':'Move a resident along a route','npc.speak':'Have a resident speak','npc.pose':'Change a resident’s posture',
    'story.area.damage':'Damage participants in a region','story.area.item':'Give an item to participants in a region',
    'story.area.buff':'Give a buff to participants in a region','story.area.spawn':'Spawn bandits in a region'
};
export const effectName=(kind:string|number)=>names[kind] ?? String(kind);
const labels:Record<string,string>={add:'Injury type',npc:'Resident ID',cell:'Map cell ID',player:'Character ID or $actor',other:'Other faction ID',
    inDays:'Days until the festival',word:'Harvest condition',words:'Public description',disposition:'Faction disposition',region:'Target region',cue:'Scene cue',pace:'Movement pace',
    posture:'Resident posture',text:'Spoken line',item:'Item ID',quantity:'Quantity per participant',buff:'Buff',hours:'Duration in hours',x:'Tile X',y:'Tile Y'};
const choices:Record<string,string[]>={pace:['walk','run'],posture:['standing','sitting','lying'],buff:['fed'],add:InjuryTypes.map(([kind])=>kind),word:['poor','fair','good','clear'],band:['destitute','poor','comfortable','rich','clear'],
    stance:['allied','friendly','neutral','tense','hostile','war'],skill:['craft','commerce','labour','service','travel','watch']};
const numbers:Record<string,[number,number]>={x:[0,4095],y:[0,4095],days:[1,366],inDays:[0,30],points:[-100,100],count:[1,6],disposition:[-100,100],severity:[1,3],quantity:[1,100],hours:[1,168]};
export function makeEffect(kind:string,keys:string[],regions:string[]=[]) {
    const e:Effect={kind};
    for(const key of keys) e[key]=key==='region'?regions[0] ?? '':choices[key]?.[0] ?? (numbers[key]?1:'');
    return e;
}
export function StoryEffectFields({effect,fields,regions,change}:{regions:Region[];effect:Effect;fields:string[];change:(e:Effect)=>void}) {
    return <>{fields.includes('cell') && fields.includes('x') && fields.includes('y') &&
        <RegionTilePicker regions={regions} pick={tile=>change({...effect,...tile})}/>}
        {fields.map(k=><label key={k}>{labels[k] ?? k}
        {k==='region'?<select aria-label="Target region" value={effect[k] ?? ''} onChange={ev=>change({...effect,region:ev.target.value})}>
            <option value="">{regions.length?'Choose a region':'Add a region in Map regions'}</option>
            {regions.map(r=><option key={r.id}>{r.id}</option>)}</select>
        : choices[k]?<select aria-label={labels[k] ?? k} value={effect[k] ?? ''} onChange={ev=>change({...effect,[k]:ev.target.value})}>
            {!choices[k].includes(String(effect[k])) && <option value={effect[k] ?? ''}>{effect[k] || 'Choose'}</option>}
            {choices[k].map(v=><option key={v} value={v}>{v.replaceAll('_',' ')}</option>)}</select>
        : <input type={numbers[k]?'number':'text'} min={k==='points' && effect.kind==='story.area.damage'?1:numbers[k]?.[0]} max={k==='days' && effect.kind==='band.override'?112:k==='points' && effect.kind==='story.area.damage'?99:numbers[k]?.[1]}
            value={effect[k] ?? ''} onChange={ev=>change({...effect,[k]:numbers[k]?Number(ev.target.value):ev.target.value})}/>}</label>)}</>;
}
