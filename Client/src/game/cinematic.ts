// The current viewer's server-selected scene. Coordinates here affect presentation only.
import {arr, bool, clamp, num, obj, objects, str, type Json} from './json.ts';

interface CastPosition {
    id: string; name: string; cell: string; posture: string; fromX: number; fromY: number;
    x: number; y: number; from: number; duration: number;
}
export function actorPoint(a: CastPosition, at: number): [number,number] {
    const t = clamp((at-a.from) / a.duration, 0, 1);
    return [a.fromX + (a.x-a.fromX)*t, a.fromY + (a.y-a.fromY)*t];
}

/** One thing said in the scene, shown in its speaker's own panel with their face (the user, 2026-11-20: "in scenes when
 *  anyone speaks it's in their own panel with their face"): the cast's lines and players' typed replies, from the
 *  server; players' ordinary speech heard while the scene plays, from the chat. */
export interface SceneSpeech {
    key: string;
    kind: 'resident' | 'player';
    speakerId: string;          // '' when this wolf isn't shown who said it (no face, never a guess).
    name: string;
    appearance: Json | null;
    artwork: string;
    text: string;
    self: boolean;
    at: number;                 // The client's clock when it arrived: the order the panels stand in.
}
/** How many speakers' panels stand at once, the newest last. */
export const ScenePanels = 4;

export class Cinematic {
    current: Json | null = null;
    private said: SceneSpeech[] = [];
    private heardKeys = new Set<string>();
    private saidFor = '';
    actors = new Map<string, CastPosition>();
    suppressedActors = new Set<string>();
    ends = 0;
    private key = '';
    private requestedAt = -Infinity;
    private last = 0;

    get locked() { return bool(this.current,'lockMovement'); }
    update(self: Json | null, at: number) {
        const scene = obj(self, 'scene');
        if (!scene && !this.current) return;
        const key = scene && str(scene,'instance') && str(scene,'beat') ? JSON.stringify([scene.instance, scene.beat]) : '';
        const same = key !== '' && this.key === key;
        if (!same) this.requestedAt = -Infinity;
        if (!this.current || str(scene,'instance') !== str(this.current,'instance')) this.actors.clear();
        const next = new Map<string, CastPosition>();
        for (const a of key ? objects(scene,'actors') : []) {
            const id = str(a,'id'), cell = str(a,'cell'), x = num(a,'x',NaN), y = num(a,'y',NaN);
            if (!id || !cell || !Number.isFinite(x) || !Number.isFinite(y)) continue;
            const old = this.actors.get(id);
            const [fromX,fromY] = old?.cell === cell ? actorPoint(old,at) : [x,y];
            // Snapshots can repeat a stationary target between motion samples.
            const pose = old && old.cell === cell && old.x === x && old.y === y ? old
                : {id, cell, x, y, fromX, fromY, from:at, duration:clamp(at-this.last,.05,.35)};
            next.set(id,{...pose,name:str(a,'name'),posture:str(a,'posture','standing')});
        }
        this.actors = next;
        this.suppressedActors = new Set(next.keys());
        for (const id of arr(scene,'suppressedActors')) if (key && typeof id==='string') this.suppressedActors.add(id);
        this.current = key ? scene : null;
        this.noteLines(key ? str(scene,'instance') : '', key ? objects(scene,'lines') : [], at);
        this.ends = at + Math.max(0,num(scene,'remainingMs'))/1000;
        this.key = key;
        this.last = at;
    }
    private noteLines(instance: string, lines: Json[], at: number) {
        if (instance !== this.saidFor) { this.said = []; this.heardKeys.clear(); this.saidFor = instance; }
        let n = 0;
        for (const l of lines) {
            const key = 's:'+str(l,'key');
            if (!str(l,'key') || this.heardKeys.has(key)) continue;
            this.heardKeys.add(key);
            const speakerId = str(l,'speakerId');
            this.add({key, kind:str(l,'kind')==='resident'?'resident':'player', speakerId, name:str(l,'name','Someone'),
                appearance:speakerId?obj(l,'appearance'):null, artwork:speakerId?str(l,'artwork'):'', text:str(l,'text'),
                self:bool(l,'self'), at:at+(n++)*1e-6});
        }
    }
    private add(line: SceneSpeech) {
        if (!line.text) return;
        this.said.push(line);
        this.said.sort((a,b)=>a.at-b.at);
        if (this.said.length > 12) this.said.splice(0,this.said.length-12);
    }
    /** A player's line heard in the chat while the scene plays: into that player's own panel. */
    hear(key: string, speaker: {id: string; name: string; appearance: Json|null; artwork: string; self: boolean}, text: string, at: number) {
        if (!this.current || this.heardKeys.has('c:'+key)) return;
        this.heardKeys.add('c:'+key);
        this.add({key:'c:'+key, kind:'player', speakerId:speaker.id, name:speaker.name, appearance:speaker.appearance,
            artwork:speaker.artwork, text, self:speaker.self, at});
    }
    /** The panels standing now, oldest first: at most ScenePanels. */
    speeches(): SceneSpeech[] { return this.current ? this.said.slice(-ScenePanels) : []; }
    choices() { return objects(this.current,'options'); }
    canAnswer(at: number) {
        return !bool(this.current,'generating') && this.current?.canRespond !== false && !bool(this.current,'voted') &&
            at < this.ends && at-this.requestedAt >= 2;
    }
    reply(draft: string, at: number, send: (command: Json)=>void) {
        const text = draft.trim();
        if (str(this.current,'type') !== 'response' || !this.canAnswer(at) || !text || [...text].length>280) return;
        this.requestedAt = at;
        send({type:'scene_reply',instance:this.current!.instance,beat:this.current!.beat,text});
    }
    vote(option: string, at: number, send: (command: Json)=>void) {
        if (str(this.current,'type') !== 'choice' || !this.canAnswer(at) || !this.choices().some(o=>str(o,'id')===option)) return;
        this.requestedAt = at;
        send({type:'scene_vote',instance:this.current!.instance,beat:this.current!.beat,option});
    }
}
