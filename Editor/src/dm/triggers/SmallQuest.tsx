// "New… → A small quest" (Docs/Design/85, 4.4; Phase 6): one quest any wolf may pick up from a resident. It asks who gives it,
// what to do and the reward (coins from a named purse, never minted), and the offer's words; it makes a version 2 story with
// an open audience, one phase that stays open, the quest, and a rehearsal case (a wolf takes it, does it, hands it in). Doc
// 58's personal-quest templates are offered as starting points (the templates stay as they are).
import {useMemo, useState} from 'react';
import templatesText from '../../../../Data/Storylines/templates.json?raw';
import {eventOf, eventParts, FIELDS, fromTemplate, namesOf, SIGNAL, smallQuestProgram, type Catalogue, type Part, type Program, type QuestObjective,
    type Refs, type SmallQuestInput, type Slot, type StorylineTemplate} from '../../lib/workbench';
import {BenchContext, useBench} from './context';
import {EventPicker, type Values} from './pickers';
import {Sentence} from './Sentence';

let templateList: StorylineTemplate[] | null = null;
/** Doc 58's templates (Data/Storylines/templates.json), read once. */
export function storylineTemplates(): StorylineTemplate[] {
    if (!templateList) { try { templateList = (JSON.parse(templatesText) as {templates: StorylineTemplate[]}).templates; } catch { templateList = []; } }
    return templateList;
}

const slot = (key: string, type: string, label: string, value: unknown, shown: string): Slot =>
    ({slot: key, keys: [key], type, label, value, shown, empty: value === undefined || value === ''});
type Reward = SmallQuestInput['reward'];

function ObjectiveLine({o, change, remove, canRemove}: {o: Omit<QuestObjective, 'id'>; change: (o: Omit<QuestObjective, 'id'>) => void; remove: () => void; canRemove: boolean}) {
    const [choosing, setChoosing] = useState(false);
    const {cat, names} = useBench();
    {
        const ev = eventOf(cat, o.on);
        const filterable = (ev ? Object.keys(ev.fields) : []).filter(k => (FIELDS as readonly string[]).includes(k) && k !== 'actor' && !(k in (o.match ?? {})));
        return <li className="wb-line wb-objective"><div className="wb-objective-body">
            <label>The journal says<input value={o.label} maxLength={180} onChange={e => change({...o, label: e.target.value})}/></label>
            <div className="wb-line"><span className="wb-glyph">⚑</span>
                <Sentence parts={eventParts(cat, o, names)} clearable={s => (FIELDS as readonly string[]).includes(s.slot)} pick={(_s, v: Values) => {
                    const match = {...o.match} as Record<string, string>;
                    for (const [k, value] of Object.entries(v)) { if (value === undefined) delete match[k]; else match[k] = String(value); }
                    const {match: _m, ...rest} = o; void _m; change(Object.keys(match).length ? {...rest, match} : rest);
                }}/>
                <button type="button" onClick={() => setChoosing(!choosing)}>Change event…</button>
                {filterable.length > 0 && <select value="" aria-label="Add a filter" onChange={e => { if (e.target.value) change({...o, match: {...o.match, [e.target.value]: ''}}); }}>
                    <option value="">+ filter: the event's…</option>{filterable.map(k => <option key={k} value={k}>{ev?.fields[k]?.label ?? k}</option>)}</select>}
                <label>Times<input type="number" min={1} max={100} value={o.goal ?? 1} onChange={e => {
                    const n = Math.max(1, Math.min(100, Math.round(e.target.valueAsNumber || 1))); const {goal: _g, ...rest} = o; void _g; change(n === 1 ? rest : {...rest, goal: n}); }}/></label>
                {canRemove && <button type="button" className="wb-x" onClick={remove} aria-label="Remove this step">×</button>}
            </div>
            {choosing && <EventPicker value={o.on} close={() => setChoosing(false)} filter={e => !!e.mission && e.id !== SIGNAL && !e.story}
                pick={on => { change({label: o.label, on}); setChoosing(false); }}/>}
        </div></li>;
    }
}

export function SmallQuestFlow({cat, refs, pick, close}: {cat: Catalogue; refs: Refs | null; pick: (p: Program) => void; close?: () => void}) {
    const [input, setInput] = useState<SmallQuestInput>({title: 'An errand', summary: '', giver: '', objectives: [{label: 'Speak with them.', on: 'conversation'}],
        reward: {kind: 'deed', detail: 'Ran an errand.'}, offer: {say: 'There is something you could do for me, if you have the time.'}});
    const [template, setTemplate] = useState('');
    const set = (patch: Partial<SmallQuestInput>) => setInput(i => ({...i, ...patch}));
    const program = useMemo(() => smallQuestProgram(input), [input]);
    const names = useMemo(() => namesOf(refs, program), [refs, program]);
    const r = input.reward;
    const setReward = (next: Reward) => set({reward: next});
    const templates = storylineTemplates();
    const giverParts: Part[] = [{text: 'Who gives it (and takes it back): '}, slot('giver', 'resident', 'a resident', input.giver, input.giver ? names.resident?.[input.giver] ?? input.giver : '')];
    const ready = input.title.trim() && input.giver && input.objectives.length && input.objectives.every(o => o.label.trim() && o.on)
        && (r.kind !== 'coins' || (r.purse && r.reason.trim() && r.coins > 0)) && (r.kind !== 'item' || r.item) && (r.kind !== 'standing' || (r.faction && r.reason.trim()))
        && (r.kind !== 'deed' || r.detail.trim());
    return <BenchContext.Provider value={{cat, refs, names, program, world: null, edit: () => {}, problems: {}}}>
        <div className="wb-small-quest" aria-label="A small quest">
            {templates.length > 0 && <label>Start from a personal-quest template (doc 58's, kept as they are)<select value={template} onChange={e => {
                setTemplate(e.target.value);
                const t = templates.find(x => x.id === e.target.value);
                if (t) { const from = fromTemplate(t, input.giver); set({title: from.title, summary: from.summary, ...(from.objectives.length ? {objectives: from.objectives} : {})}); }
            }}><option value="">(none: start blank)</option>{templates.map(t => <option key={t.id} value={t.id}>{t.title.replace(/\[\[\{?|\}?\]\]/g, '')} ({t.id})</option>)}</select></label>}
            <div className="wb-head"><label className="wb-grow">Title<input value={input.title} maxLength={120} onChange={e => set({title: e.target.value})}/></label>
                <label>Taken again<select value={input.repeat ?? 'once'} onChange={e => set({repeat: e.target.value as SmallQuestInput['repeat']})}>
                    <option value="once">never (once)</option><option value="daily">after a day</option><option value="weekly">after a week</option><option value="always">at once</option></select></label></div>
            <label>What the journal says of it<textarea rows={2} maxLength={300} value={input.summary} onChange={e => set({summary: e.target.value})}/></label>
            <h5>Who gives it</h5>
            <div className="wb-line"><span className="wb-glyph">☻</span><Sentence parts={giverParts} pick={(_s, v) => {
                const giver = v.giver === undefined ? '' : String(v.giver);
                // (A first step of talking to someone is talking to the giver, until the author says otherwise.)
                set({giver, objectives: input.objectives.map((o, i) => i === 0 && o.on === 'conversation' && (!o.match?.target || o.match.target === input.giver)
                    ? {...o, match: {...o.match, target: giver}} : o)});
            }}/></div>
            <h5>What to do</h5>
            <ol className="wb-actions">{input.objectives.map((o, i) => <ObjectiveLine key={i} o={o} canRemove={input.objectives.length > 1}
                change={next => set({objectives: input.objectives.map((x, n) => n === i ? next : x)})} remove={() => set({objectives: input.objectives.filter((_, n) => n !== i)})}/>)}
                {input.objectives.length < 6 && <li className="wb-add-row"><button type="button" className="wb-add" onClick={() => set({objectives: [...input.objectives,
                    {label: 'Bring them what they asked for.', on: 'gift', ...(input.giver ? {match: {target: input.giver}} : {})}]})}>+ a step</button></li>}</ol>
            <h5>The reward</h5>
            <div className="wb-head"><label>What<select value={r.kind} onChange={e => {
                const k = e.target.value;
                setReward(k === 'deed' ? {kind: 'deed', detail: 'Ran an errand.'} : k === 'coins' ? {kind: 'coins', coins: 10, purse: '', reason: 'for the errand'}
                    : k === 'item' ? {kind: 'item', item: '', quantity: 1} : k === 'standing' ? {kind: 'standing', faction: '', points: 2, reason: 'for the errand'} : {kind: 'none'});
            }}><option value="deed">a deed told of them</option><option value="coins">coins, from a purse</option><option value="item">an item</option>
                <option value="standing">standing with a faction</option><option value="none">nothing but thanks</option></select></label>
                {r.kind === 'deed' && <label className="wb-grow">The deed<input maxLength={200} value={r.detail} onChange={e => setReward({...r, detail: e.target.value})}/></label>}
                {r.kind === 'coins' && <><label>Pennies<input type="number" min={1} max={2500} value={r.coins} onChange={e => setReward({...r, coins: Math.max(1, Math.min(2500, Math.round(e.target.valueAsNumber || 1)))})}/></label>
                    <label>Why<input maxLength={80} value={r.reason} onChange={e => setReward({...r, reason: e.target.value})}/></label></>}
                {r.kind === 'item' && <label>How many<input type="number" min={1} max={20} value={r.quantity} onChange={e => setReward({...r, quantity: Math.max(1, Math.min(20, Math.round(e.target.valueAsNumber || 1)))})}/></label>}
                {r.kind === 'standing' && <><label>Points<input type="number" min={-10} max={10} value={r.points} onChange={e => setReward({...r, points: Math.max(-10, Math.min(10, Math.round(e.target.valueAsNumber || 1))) || 1})}/></label>
                    <label>Why<input maxLength={80} value={r.reason} onChange={e => setReward({...r, reason: e.target.value})}/></label></>}
            </div>
            {r.kind === 'coins' && <div className="wb-line"><span className="wb-glyph">¤</span><Sentence parts={[{text: 'Paid by '},
                slot('purse', 'purse', 'a purse', r.purse, r.purse ? names.purse?.[r.purse] ?? r.purse : '')]} pick={(_s, v) => setReward({...r, purse: String(v.purse ?? '')})}/>
                <small className="hint">refused when the purse is short: never made from nothing</small></div>}
            {r.kind === 'item' && <div className="wb-line"><span className="wb-glyph">▣</span><Sentence parts={[{text: 'The item: '},
                slot('item', 'item', 'an item', r.item, r.item ? names.item?.[r.item] ?? r.item : '')]} pick={(_s, v) => setReward({...r, item: String(v.item ?? '')})}/></div>}
            {r.kind === 'standing' && <div className="wb-line"><span className="wb-glyph">⚖</span><Sentence parts={[{text: 'With '},
                slot('faction', 'faction', 'a faction', r.faction, r.faction ? names.faction?.[r.faction] ?? r.faction : '')]} pick={(_s, v) => setReward({...r, faction: String(v.faction ?? '')})}/></div>}
            <h5>The offer, in the resident's dialogue window</h5>
            <label>The resident says<textarea rows={2} maxLength={300} value={input.offer.say} onChange={e => set({offer: {...input.offer, say: e.target.value}})}/></label>
            <div className="wb-head">
                <label>The wolf asks<input maxLength={120} value={input.offer.ask ?? ''} placeholder={'Ask about: ' + input.title} onChange={e => set({offer: {...input.offer, ask: e.target.value}})}/></label>
                <label>Accepts<input maxLength={120} value={input.offer.accept ?? ''} placeholder="I'll do it." onChange={e => set({offer: {...input.offer, accept: e.target.value}})}/></label>
                <label>Hands it in<input maxLength={120} value={input.offer.handIn ?? ''} placeholder={`About ${input.title}: it's done.`} onChange={e => set({offer: {...input.offer, handIn: e.target.value}})}/></label>
                <label className="wb-grow">The resident's thanks<input maxLength={300} value={input.offer.thanks ?? ''} placeholder="(the game's own thanks)" onChange={e => set({offer: {...input.offer, thanks: e.target.value}})}/></label>
            </div>
            <p className="hint">It makes a version 2 story with an open audience (anyone who asks may take it), one phase that stays open, the quest, and a rehearsal
                case: a wolf takes it, does {input.objectives.length === 1 ? 'it' : 'each step'} and hands it in. The Quest editor does the rest (optional and hidden steps, fail rules, caps).</p>
            <div className="camp-controls"><button type="button" className="primary" disabled={!ready} onClick={() => pick(program)}>Make the small quest</button>
                {close && <button type="button" onClick={close}>Cancel</button>}
                {!input.giver && <small className="hint">Choose who gives it.</small>}</div>
        </div>
    </BenchContext.Provider>;
}
