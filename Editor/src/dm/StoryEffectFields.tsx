import {InjuryTypes} from './api';
export type Effect = Record<string,string|number>;
const names:Record<string,string>={
    'npc.line':'Give a resident a dialogue line','npc.move':'Move a resident','npc.protect':'Protect a resident',
    'npc.unprotect':'Remove resident protection','npc.revive':'Revive a resident','character.injury':'Give a character an injury',
    'ship.storm':'Delay a ship with a storm','harvest.override':'Change a harvest','band.override':'Change household prosperity',
    'festival.call':'Call a festival','storyline.give':'Give a personal quest','faction.relation':'Change faction relations',
    'bandits.call':'Call bandits','story.practice':'Grant skill practice','times.nudge':'Change market conditions',
    'deed.award':'Award a deed','project.post':'Post a town project'
};
export const effectName=(kind:string|number)=>names[kind] ?? String(kind);
const labels:Record<string,string>={add:'Injury type',npc:'Resident ID',cell:'Map cell ID',player:'Character ID or $actor',other:'Other faction ID',
    inDays:'Days until the festival',word:'Harvest condition',words:'Public description',disposition:'Faction disposition',x:'Tile X',y:'Tile Y'};
const choices:Record<string,string[]>={add:InjuryTypes.map(([kind])=>kind),word:['poor','fair','good','clear'],band:['destitute','poor','comfortable','rich','clear'],
    stance:['allied','friendly','neutral','tense','hostile','war'],skill:['craft','commerce','labour','service','travel','watch']};
const numbers:Record<string,[number,number]>={x:[0,4095],y:[0,4095],days:[1,366],inDays:[0,30],points:[-100,100],count:[1,6],disposition:[-100,100],severity:[1,3]};
export function makeEffect(kind:string,keys:string[]) {
    const e:Effect={kind};
    for(const key of keys) e[key]=choices[key]?.[0] ?? (numbers[key]?1:'');
    return e;
}
export function StoryEffectFields({effect,fields,change}:{effect:Effect;fields:string[];change:(e:Effect)=>void}) {
    return <>{fields.map(k=><label key={k}>{labels[k] ?? k}
        {choices[k]?<select aria-label={labels[k] ?? k} value={effect[k] ?? ''} onChange={ev=>change({...effect,[k]:ev.target.value})}>
            {!choices[k].includes(String(effect[k])) && <option value={effect[k] ?? ''}>{effect[k] || 'Choose'}</option>}
            {choices[k].map(v=><option key={v} value={v}>{v.replaceAll('_',' ')}</option>)}</select>
        : <input type={numbers[k]?'number':'text'} min={numbers[k]?.[0]} max={k==='days' && effect.kind==='band.override'?112:numbers[k]?.[1]}
            value={effect[k] ?? ''} onChange={ev=>change({...effect,[k]:numbers[k]?Number(ev.target.value):ev.target.value})}/>}</label>)}</>;
}
