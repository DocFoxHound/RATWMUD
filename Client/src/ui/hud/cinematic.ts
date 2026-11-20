// Story-scene playback occupies the map only for viewers included by the server.
import {actorPoint, type SceneSpeech} from '../../game/cinematic.ts';
import {arr, bool, clamp, num, obj, str, type Json} from '../../game/json.ts';
import type {GameState} from '../../game/state.ts';
import {terrainInfo} from '../../game/terrainLayer.ts';
import {css} from '../color.ts';
import {drawPortrait, type Portraits} from '../portrait.ts';
import {artCache} from '../artwork.ts';
import {el, setText, show} from './dom.ts';

interface SpeechCard { key: string; face: HTMLCanvasElement; faceKey: string; speech: SceneSpeech; }

// In a scene each speaker has their own panel with their face (the user, 2026-11-20): the resident's lines and each
// player's words stand as a column of panels, the newest last and clearest; the beat's controls go beneath them.
export class CinematicPanel {
    private frame: HTMLCanvasElement;
    private panel: HTMLElement;
    private speeches: HTMLElement;
    private controls: HTMLElement;
    private heading: HTMLElement;
    private narration: HTMLElement;
    private clock: HTMLElement;
    private options: HTMLElement;
    private form: HTMLFormElement;
    private reply: HTMLInputElement;
    private submit: HTMLButtonElement;
    private note: HTMLElement;
    private status: HTMLElement;
    private cards: SpeechCard[] = [];
    private cardsKey = '';
    private beat = '';
    private choiceKey = '';
    private cameraKey = '';

    constructor(map: HTMLElement, private s: GameState, private portraits: Portraits) {
        this.frame = el('canvas','cinematic-camera',map);
        this.frame.setAttribute('role','img');
        this.panel = el('section','cinematic-panel',map);
        this.panel.setAttribute('aria-label','Story scene');
        this.panel.addEventListener('focusin',()=>this.s.focusLost());
        this.speeches = el('div','cinematic-speeches',this.panel);
        this.speeches.setAttribute('aria-live','polite');
        this.controls = el('div','cinematic-controls',this.panel);
        const header = el('div','cinematic-header',this.controls);
        this.heading = el('strong','',header);
        this.clock = el('span','cinematic-time',header);
        this.clock.setAttribute('role','timer');
        this.narration = el('p','cinematic-narration',this.controls);
        this.options = el('div','cinematic-options',this.controls);
        this.form = el('form','cinematic-response',this.controls);
        const label = el('label','',this.form,'Your response');
        this.reply = el('input','',label);
        // HTML maxlength counts UTF-16 units; the wire limit counts Unicode characters.
        this.reply.maxLength=560;
        this.reply.required=true;
        this.reply.addEventListener('input',()=>{
            const length = [...this.reply.value.trim()].length;
            this.reply.setCustomValidity(length>280 ? 'Use up to 280 characters.' : length===0 ? 'Enter a response.' : '');
        });
        this.submit = el('button','',this.form,'Reply');
        this.submit.type='submit';
        this.form.addEventListener('submit',e=>{
            e.preventDefault();
            this.s.cinematic.reply(this.reply.value,this.s.clock,this.s.send);
            this.update();
        });
        this.note = el('p','cinematic-note',this.controls);
        this.status = el('div','cinematic-note',this.controls);
        this.status.setAttribute('role','status');
        show(this.panel,false); show(this.frame,false);
    }

    update() {
        const playback = this.s.cinematic, scene = playback.current;
        show(this.panel,!!scene);
        show(this.frame,!!obj(scene,'frame'));
        if (!scene) {
            this.beat=''; this.choiceKey=''; this.cameraKey=''; this.reply.value='';
            if (this.cards.length) { this.cards=[]; this.cardsKey=''; this.speeches.replaceChildren(); }
            return;
        }
        const type = str(scene,'type'), speaker = obj(scene,'speaker'), name = str(speaker,'name');
        const generating = bool(scene,'generating'), voted=bool(scene,'voted'), eligible = scene.canRespond !== false || voted;
        const key = JSON.stringify([scene.instance,scene.beat]);
        if (this.beat!==key) { this.beat=key; this.reply.value=''; this.reply.setCustomValidity(''); this.choiceKey=''; }
        this.showSpeeches(speaker, generating);
        const narration = type==='move' ? name+' is moving.' : type==='pose' ? name+' is '+str(scene,'posture',playback.actors.get(str(speaker,'id'))?.posture || 'standing')+'.' : '';
        setText(this.heading,str(scene,'title','Story scene'));
        setText(this.narration,narration);
        show(this.narration,!!narration);
        const seconds = Math.max(0,Math.ceil(playback.ends-this.s.clock));
        setText(this.clock,['choice','response','line'].includes(type) && !generating ? seconds+'s remaining' : '');
        const choices = playback.choices(), choiceKey=JSON.stringify(choices);
        if (this.choiceKey!==choiceKey) {
            this.choiceKey=choiceKey; this.options.replaceChildren();
            for (const o of choices) {
                const button = el('button','',this.options,str(o,'label'));
                button.type='button';
                button.addEventListener('click',()=>{
                    playback.vote(str(o,'id'),this.s.clock,this.s.send);
                    this.update();
                });
            }
        }
        const canAct = playback.canAnswer(this.s.clock);
        for (const b of this.options.children) (b as HTMLButtonElement).disabled = !canAct;
        show(this.options,type==='choice' && !generating);
        show(this.form,type==='response' && eligible && !voted && !generating);
        this.reply.disabled=!canAct; this.submit.disabled=!canAct;
        setText(this.note,type==='response' && bool(scene,'usesAI') ? 'Your reply may be sent to the host’s linked AI to write later dialogue.' : '');
        const interactive = type==='response' || type==='choice';
        const group = str(scene,'mode')==='all' ? 'Waiting for the group to respond.' : str(scene,'mode')==='majority' ? 'The group is voting.' : '';
        setText(this.status, !interactive || generating ? '' : !eligible ? 'Observing this decision.' :
            voted ? (type==='choice'?'Vote recorded.':'Response received.')+' '+group : !seconds ? 'Time is up.' : group);
        const frame=obj(scene,'frame');
        if (frame) this.drawFrame(frame);
    }

    /** The speakers' panels: what was said, newest last, and the speaker still finding their words ("…"). */
    private showSpeeches(speaker: Json|null, generating: boolean) {
        const list: SceneSpeech[] = this.s.cinematic.speeches();
        if (generating && speaker) list.push({key:'pending:'+this.beat, kind:'resident', speakerId:str(speaker,'id'), name:str(speaker,'name'),
            appearance:obj(speaker,'appearance'), artwork:str(speaker,'artwork'), text:'…', self:false, at:Infinity});
        const key = JSON.stringify(list.map(l=>l.key));
        if (key!==this.cardsKey) {
            this.cardsKey=key;
            const old = new Map(this.cards.map(c=>[c.key,c]));
            this.cards = list.map(speech=>old.get(speech.key) ?? this.card(speech));
            this.speeches.replaceChildren(...this.cards.map(c=>c.face.parentElement!));
            this.cards.forEach((c,i)=>{
                const box=c.face.parentElement!;
                box.classList.toggle('newest',i===this.cards.length-1);
                box.classList.toggle('older',i<this.cards.length-2);
            });
        }
        for (const c of this.cards) this.drawFace(c);
    }

    private card(speech: SceneSpeech): SpeechCard {
        const box = document.createElement('article');
        box.className = 'cinematic-speech '+speech.kind+(speech.self?' self':'');
        const face = el('canvas','cinematic-portrait',box);
        face.width=160; face.height=180;
        face.setAttribute('aria-hidden','true');
        const body = el('div','cinematic-body',box);
        el('strong','cinematic-name',body,speech.self ? speech.name+' (you)' : speech.name);
        el('p','cinematic-line',body,speech.text);
        return {key:speech.key, face, faceKey:'', speech};
    }

    private drawFace(card: SpeechCard) {
        const {speech} = card, picture = speech.artwork ? artCache.get(speech.artwork) : undefined;
        const key = JSON.stringify([speech.speakerId,speech.appearance,speech.artwork,!!picture,this.portraits.version]);
        if (key===card.faceKey) return;
        const has = !!speech.speakerId && (!!speech.appearance || !!speech.artwork);
        show(card.face,has);
        if (!has) { card.faceKey=key; return; }
        const c=card.face.getContext('2d');
        if (!c) return;
        c.clearRect(0,0,160,180);
        if (drawPortrait(c,this.portraits,speech.appearance,0,0,160,180,speech.artwork || undefined,{view:'face',paper:true})) card.faceKey=key;
    }

    private drawFrame(frame:Json) {
        const width=Math.max(1,this.frame.clientWidth),height=Math.max(1,this.frame.clientHeight);
        const w=clamp(Math.trunc(num(frame,'width')),1,64),h=clamp(Math.trunc(num(frame,'height')),1,64);
        const x0=num(frame,'x'),y0=num(frame,'y'), rows=arr(frame,'rows');
        const cast=[...this.s.cinematic.actors.values()].filter(a=>a.cell===str(frame,'cell')).map(a=>({actor:a,point:actorPoint(a,this.s.clock)}));
        const space = Math.max(height*.35,height-this.panel.offsetHeight-24);
        const key=JSON.stringify([width,height,space,frame,this.s.cinematic.current?.camera,cast.map(({actor:a,point})=>[a.id,a.posture,point])]);
        if (key===this.cameraKey) return;
        this.cameraKey=key;
        const dpr=Math.min(2,window.devicePixelRatio||1);
        if (this.frame.width!==Math.round(width*dpr)) this.frame.width=Math.round(width*dpr);
        if (this.frame.height!==Math.round(height*dpr)) this.frame.height=Math.round(height*dpr);
        const c=this.frame.getContext('2d'); if (!c) return;
        c.setTransform(dpr,0,0,dpr,0,0);
        c.fillStyle='#101714'; c.fillRect(0,0,width,height);
        const tile=Math.max(1,Math.min(width/w,space/h,46));
        const camera=obj(this.s.cinematic.current,'camera');
        const left=width/2-(num(camera,'x',x0+(w-1)/2)+.5-x0)*tile;
        const top=space/2-(num(camera,'y',y0+(h-1)/2)+.5-y0)*tile;
        c.textAlign='center'; c.textBaseline='middle'; c.font=Math.floor(tile*.60)+'px "RATW Mono", monospace';
        for (let y=0;y<h;y++) {
            const row=typeof rows[y]==='string'?rows[y] as string:'';
            for (let x=0;x<w;x++) {
                const code=row[x]??' ', ground=terrainInfo(code);
                if (!ground) continue;
                c.fillStyle=css(ground.bg); c.fillRect(left+x*tile,top+y*tile,tile+.5,tile+.5);
                c.fillStyle=css(ground.fg); c.fillText(ground.ascii || code,left+(x+.5)*tile,top+(y+.5)*tile);
            }
        }
        for (const {actor,point:[x,y]} of cast) {
            if (x<x0 || y<y0 || x>=x0+w || y>=y0+h) continue;
            c.save(); c.translate(left+(x-x0)*tile,top+(y-y0)*tile);
            if (actor.posture==='lying') c.rotate(Math.PI/2);
            c.fillStyle='#14201b'; c.fillRect(-tile*.38,-tile*.4,tile*.76,tile*.8);
            c.fillStyle='#d5dcca'; c.fillText('W',0,0);
            if (actor.posture==='sitting') c.fillRect(-tile*.14,tile*.28,tile*.28,1);
            c.restore();
        }
        this.frame.setAttribute('aria-label', 'Scene camera at '+str(frame,'name',str(frame,'cell'))+
            (cast.length?' with '+cast.map(a=>a.actor.name).join(', '):''));
    }
}
