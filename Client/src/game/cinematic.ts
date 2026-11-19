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

export class Cinematic {
    current: Json | null = null;
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
        this.ends = at + Math.max(0,num(scene,'remainingMs'))/1000;
        this.key = key;
        this.last = at;
    }
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
