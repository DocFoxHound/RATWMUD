// The Quest editor (Docs/Design/85-story-triggers-and-campaign-control.md, 3.6 and 4.6; Phase 6): one quest of a version 2
// story. Who may take it and where (its giver, the offer's words in the resident's own voice, the join rules), what to do
// (objectives in sentences from the catalogue: required, optional or hidden, in order or not, counted), what fails it,
// and what it gives (rewards to each wolf with credit, coins from a named purse, never minted).
import {useState} from 'react';
import {eventOf, eventParts, FIELDS, isId, objectiveEvents, operandWords, renameObjective, renameQuest, SIGNAL, withQuests, type FailRule, type Part,
    type Quest, type QuestObjective, type Ref, type Slot} from '../../lib/workbench';
import {useBench} from './context';
import {EventPicker, type Values} from './pickers';
import {Sentence} from './Sentence';
import {EffectList} from './TriggerView';

/** One slot as a sentence part (the giver, a region): the picker for its type, the name for its id. */
function slotPart(key: string, type: string, label: string, value: unknown, shownAs: string): Slot {
    return {slot: key, keys: [key], type, label, value, shown: shownAs, empty: value === undefined || value === ''};
}
const merged = (match: Record<string, string> | undefined, v: Values) => {
    const next = {...match} as Record<string, string>;
    for (const [k, value] of Object.entries(v)) { if (value === undefined) delete next[k]; else next[k] = String(value); }
    return next;
};
const MEASURES: Record<string, string> = {count: 'times', quantity: 'items given or used', coins: 'coins'};
// (Doc 86, SQ5: the events with a wolf it was done for.)
const CREDIT_TARGET = ['stall sale', 'vouch', 'told the wardens', 'vouched to the wardens'];

function ObjectiveRow({q, o, i, change, remove, move, rename}: {q: Quest; o: QuestObjective; i: number; change: (o: QuestObjective, mark?: string) => void;
    remove: () => void; move: (to: number) => void; rename: (to: string) => void}) {
    const {cat, names, program} = useBench();
    const [choosing, setChoosing] = useState(false);
    const [id, setId] = useState(o.id);
    const ev = eventOf(cat, o.on);
    const others = q.objectives.filter(x => x.id !== o.id);
    const after = new Set(o.after ?? []);
    const measures = ['count', ...(ev?.measures ?? [])];
    const filterable = (ev ? Object.keys(ev.fields) : [...FIELDS]).filter(k => (FIELDS as readonly string[]).includes(k) && !(k in (o.match ?? {})));
    // (Doc 86, Phase 2: a goal from a number variable; where it counts; who it credits.)
    const numbers = (program.variables ?? []).filter(v => v.type === 'number' && (v.scope === 'story' || v.scope === 'world')).map(v => v.id);
    const regions = (program.regions ?? []).map(r => r.id);
    return <li className="wb-line wb-objective">
        <span className="wb-glyph">{i + 1}</span>
        <div className="wb-objective-body">
            <div className="wb-head">
                <label>Id<input value={id} spellCheck={false} onChange={e => setId(e.target.value)} onBlur={() => {
                    const to = id.trim();
                    if (to !== o.id && isId(to) && !q.objectives.some(x => x.id === to)) rename(to); else setId(o.id);
                }}/></label>
                <label className="wb-grow">The journal says<input value={o.label} maxLength={180} onChange={e => change({...o, label: e.target.value}, `q:${q.id}:${i}:label`)}/></label>
            </div>
            <div className="wb-line"><span className="wb-glyph">⚑</span>
                <Sentence parts={eventParts(cat, o, names)} clearable={s => (FIELDS as readonly string[]).includes(s.slot)}
                    pick={(slot, v) => {
                        if (slot.slot === 'signal') change({...o, signal: String(v.signal)});
                        else { const match = merged(o.match, v); const {match: _m, ...rest} = o; void _m; change(Object.keys(match).length ? {...rest, match} : rest); }
                    }}/>
                <button type="button" onClick={() => setChoosing(!choosing)}>Change event…</button>
                {filterable.length > 0 && o.on !== SIGNAL && <select value="" aria-label="Add a filter" onChange={e => { if (e.target.value) change({...o, match: {...o.match, [e.target.value]: ''}}); }}>
                    <option value="">+ filter: the event's…</option>{filterable.map(k => <option key={k} value={k}>{ev?.fields[k]?.label ?? k}</option>)}</select>}
            </div>
            {choosing && <EventPicker value={o.on} close={() => setChoosing(false)} filter={e => !!e.mission || e.id === SIGNAL} pick={on => {
                const {match: _m, signal: _s, measure: _x, ...rest} = o; void _m; void _s; void _x;
                change({...rest, on, ...(on === SIGNAL ? {signal: 'next'} : {})}); setChoosing(false);
            }}/>}
            <div className="wb-head">
                {typeof o.goal === 'object'
                    ? <span className="wb-chips"><span>Goal: {operandWords(o.goal, names)}</span>
                        <button type="button" onClick={() => { const {goal: _g, goalLive: _l, ...rest} = o; void _g; void _l; change(rest); }}>a number instead</button>
                        {q.scope === 'story' && <label className="wb-check" title="Doc 86, C1c: read again as it is credited, never below what is made">
                            <input type="checkbox" checked={!!o.goalLive} onChange={e => { const {goalLive: _l, ...rest} = o; void _l;
                                change(e.target.checked ? {...rest, goalLive: true} : rest); }}/>keeps up with it</label>}</span>
                    : <label>Goal<input type="number" min={1} max={1000000} value={typeof o.goal === "number" ? o.goal : 1} onChange={e => {
                        const n = Math.max(1, Math.min(1000000, Math.round(e.target.valueAsNumber || 1))); const {goal: _g, ...rest} = o; void _g;
                        change(n === 1 ? rest : {...rest, goal: n}, `q:${q.id}:${i}:goal`); }}/></label>}
                {typeof o.goal !== 'object' && numbers.length > 0 && <select value="" aria-label="The goal from a value" onChange={e => {
                    const v = e.target.value; if (!v) return;
                    change({...o, goal: v === '*active' ? {max: [typeof o.goal === 'number' ? o.goal : 1, {times: [{audience: 'active'}, 2]}]} : {var: v}}); }}>
                    <option value="">or a value…</option>{numbers.map(v => <option key={v} value={v}>the number {v}</option>)}
                    <option value="*active">this goal, or 2 for each active wolf</option></select>}
                <label>Counts<select value={o.where ? ('cells' in o.where ? 'cells' : 'region') : ''} aria-label="Where it counts" onChange={e => {
                    const {where: _w, ...rest} = o; void _w; const v = e.target.value;
                    change(v === 'cells' ? {...rest, where: {cells: [typeof o.match?.cell === 'string' && o.match.cell ? o.match.cell : 'cell']}}
                        : v === 'region' && regions.length ? {...rest, where: {region: regions[0]}} : rest); }}>
                    <option value="">anywhere</option><option value="cells">only in some cells</option>{regions.length > 0 && <option value="region">only inside a region</option>}</select></label>
                {o.where && 'cells' in o.where && <label>Cells<input value={o.where.cells.join(', ')} placeholder="cell ids, separated by commas" onChange={e => {
                    const cells = [...new Set(e.target.value.split(',').map(x => x.trim()).filter(Boolean))].slice(0, 16);
                    change({...o, where: {cells: cells.length ? cells : ['']}}, `q:${q.id}:${i}:where`); }}/></label>}
                {o.where && 'region' in o.where && <label>Region<select value={o.where.region} onChange={e => change({...o, where: {region: e.target.value}})}>
                    {regions.map(r => <option key={r} value={r}>{r}</option>)}</select></label>}
                <label>Who is credited<select value={o.credit ?? 'actor'} onChange={e => { const {credit: _c, ...rest} = o; void _c;
                    change(e.target.value === 'actor' ? rest : {...rest, credit: e.target.value as QuestObjective['credit']}); }}>
                    <option value="actor">the acting wolf</option>
                    {CREDIT_TARGET.includes(o.on) && <option value="target">the one it was done for</option>}
                    {!!ev?.fields.cell && <option value="present">every wolf there</option>}</select></label>
                <label>Counting<select value={o.measure ?? 'count'} onChange={e => { const {measure: _m, ...rest} = o; void _m;
                    change(e.target.value === 'count' ? rest : {...rest, measure: e.target.value as QuestObjective['measure']}); }}>
                    {measures.map(m => <option key={m} value={m}>{MEASURES[m] ?? m}</option>)}</select></label>
                <label className="wb-check"><input type="checkbox" checked={!!o.optional} onChange={e => { const {optional: _o, ...rest} = o; void _o;
                    change(e.target.checked ? {...rest, optional: true} : rest); }}/>optional</label>
                <label className="wb-check"><input type="checkbox" checked={!!o.hidden} onChange={e => { const {hidden: _h, ...rest} = o; void _h;
                    change(e.target.checked ? {...rest, hidden: true} : rest); }}/>hidden until revealed or done</label>
            </div>
            {others.length > 0 && <div className="wb-chips"><span>After:</span>{others.map(x => <label key={x.id} className="wb-check"><input type="checkbox" checked={after.has(x.id)}
                onChange={e => { const next = new Set(after); if (e.target.checked) next.add(x.id); else next.delete(x.id);
                    const {after: _a, ...rest} = o; void _a; change(next.size ? {...rest, after: others.map(y => y.id).filter(y => next.has(y))} : rest); }}/>{x.id}</label>)}
                {!after.size && <small className="hint">(open from the start)</small>}</div>}
        </div>
        <span className="wb-tools"><button type="button" disabled={!i} onClick={() => move(i - 1)} aria-label={'Move objective ' + (i + 1) + ' earlier'}>↑</button>
            <button type="button" disabled={i === q.objectives.length - 1} onClick={() => move(i + 1)} aria-label={'Move objective ' + (i + 1) + ' later'}>↓</button>
            <button type="button" className="wb-x" disabled={q.objectives.length < 2} onClick={remove} aria-label={'Remove objective ' + (i + 1)}>×</button></span>
    </li>;
}

function FailRow({rule, change, remove}: {rule: FailRule; change: (f: FailRule, mark?: string) => void; remove: () => void}) {
    const {cat, names} = useBench();
    const [choosing, setChoosing] = useState(false);
    if ('afterDays' in rule) return <li className="wb-line"><span className="wb-glyph">⏱</span>
        <label>When it has run for<input type="number" min={0.01} max={3650} step="any" value={rule.afterDays}
            onChange={e => change({...rule, afterDays: e.target.valueAsNumber || 1}, 'fail-days')}/></label>
        <select value={rule.clock ?? 'game'} aria-label="Which clock" onChange={e => change(e.target.value === 'real' ? {...rule, clock: 'real'} : {afterDays: rule.afterDays})}>
            <option value="game">game days</option><option value="real">real days</option></select>
        <span>without being done</span><button type="button" className="wb-x" onClick={remove} aria-label="Remove this fail rule">×</button></li>;
    return <li className="wb-line"><span className="wb-glyph">✖</span>
        <Sentence parts={eventParts(cat, rule, names)} clearable={s => (FIELDS as readonly string[]).includes(s.slot)} pick={(slot, v) => {
            if (slot.slot === 'signal') change({...rule, signal: String(v.signal)});
            else { const match = merged(rule.match, v); const {match: _m, ...rest} = rule; void _m; change(Object.keys(match).length ? {...rest, match} : rest); }
        }}/>
        <button type="button" onClick={() => setChoosing(!choosing)}>Change event…</button>
        <button type="button" className="wb-x" onClick={remove} aria-label="Remove this fail rule">×</button>
        {choosing && <EventPicker value={rule.on} close={() => setChoosing(false)} pick={on => { change({on, ...(on === SIGNAL ? {signal: 'next'} : {})}); setChoosing(false); }}/>}
    </li>;
}

export function QuestEditor({index, select}: {index: number; select: (r: Ref) => void}) {
    const {cat, names, program, edit, problems} = useBench();
    const quests = program.quests ?? [];
    const q = quests[index];
    const [id, setId] = useState(q?.id ?? '');
    if (!q) return <p className="hint">This quest is gone.</p>;
    const key = `q:${index}`;
    const save = (next: Quest, mark?: string) => edit(withQuests(program, quests.map((x, i) => i === index ? next : x)), mark);
    const set = <K extends keyof Quest>(k: K, v: Quest[K] | undefined, mark?: string) => {
        const next = {...q} as Record<string, unknown>;
        if (v === undefined || v === '' || (Array.isArray(v) && !v.length)) delete next[k]; else next[k] = v;
        save(next as unknown as Quest, mark);
    };
    const offer = q.offer ?? {say: ''};
    const setOffer = (k: keyof typeof offer, v: string) => {
        const next = {...offer, [k]: v} as Record<string, string>;
        if (!v && k !== 'say') delete next[k];
        set('offer', next as unknown as Quest['offer'], `${key}:offer:${k}`);
    };
    const join = q.join ?? {};
    const setJoin = (k: keyof typeof join, v: unknown) => {
        const next = {...join} as Record<string, unknown>;
        if (v === undefined || v === '' || (typeof v === 'number' && !Number.isFinite(v))) delete next[k]; else next[k] = v;
        set('join', Object.keys(next).length ? next as Quest['join'] : undefined, `${key}:join:${k}`);
    };
    const v2 = program.version === 2;
    const open = program.audience.kind === 'open' || program.audience.kind === 'everyone';
    const giverPart: Part[] = [{text: 'Given (and taken back) by '}, slotPart('giver', 'resident', 'a resident', q.giver, q.giver ? names.resident?.[q.giver] ?? q.giver : '')];
    const regionPart: Part[] = [{text: 'Taken only standing inside '}, slotPart('region', 'region', 'anywhere', join.region, join.region ?? '')];
    const boardPart: Part[] = [{text: 'Posted on the notice board of '}, slotPart('board', 'town', 'no town', q.board, q.board ? names.town?.[q.board] ?? q.board : '')];
    const mine = Object.entries(problems).filter(([k]) => k === key || k.startsWith(key + ':')).flatMap(([, v]) => v);
    const objectives = q.objectives;
    const putObjectives = (list: QuestObjective[], mark?: string) => save({...q, objectives: list}, mark);
    const rename = () => {
        const to = id.trim();
        if (to === q.id) return;
        if (!isId(to) || quests.some(x => x.id === to)) { setId(q.id); return; }
        edit(renameQuest(program, q.id, to));
    };
    return <article className="wb-trigger wb-quest" aria-label={'Quest ' + q.id}>
        {!v2 && <p className="wb-problem">Quests need a version 2 story: Make it version 2… (under Variables, on the right).</p>}
        {mine.length > 0 && <ul className="wb-problem">{mine.map((m, i) => <li key={i}>✖ {m}</li>)}</ul>}
        <div className="wb-head">
            <label>Quest id<input value={id} spellCheck={false} onChange={e => setId(e.target.value)} onBlur={rename} onKeyDown={e => { if (e.key === 'Enter') rename(); }}/></label>
            <label className="wb-grow">Title<input value={q.title} maxLength={120} onChange={e => save({...q, title: e.target.value}, key + ':title')}/></label>
        </div>
        <label>Summary (the journal's first line)<textarea rows={2} maxLength={300} value={q.summary ?? ''} onChange={e => set('summary', e.target.value, key + ':summary')}/></label>
        <div className="wb-head">
            <label>Whose<select value={q.scope ?? 'player'} onChange={e => { const s = e.target.value as Quest['scope'];
                const {contributors: _c, repeat: _r, ...rest} = q; void _c; void _r; save({...rest, scope: s, ...(s !== 'story' && q.repeat ? {repeat: q.repeat} : {})}); }}>
                <option value="player">each wolf's own copy</option><option value="party">one copy for the wolf's party</option><option value="story">one copy for everyone</option></select></label>
            {q.scope === 'story' && <label>At least … different wolves<input type="number" min={1} max={500} value={q.contributors ?? 1}
                onChange={e => set('contributors', Math.max(1, Math.min(500, Math.round(e.target.valueAsNumber || 1))) === 1 ? undefined : Math.round(e.target.valueAsNumber), key + ':contributors')}/></label>}
            <label>Lasts<select value={q.lifetime ?? 'campaign'} onChange={e => set('lifetime', e.target.value === 'phase' ? 'phase' : undefined)}>
                <option value="campaign">the whole campaign</option><option value="phase">until the phase visit ends</option></select></label>
            <label>Taken again<select value={q.repeat ?? 'once'} disabled={q.scope === 'story'} onChange={e => set('repeat', e.target.value === 'once' ? undefined : e.target.value as Quest['repeat'])}>
                <option value="once">never (once)</option><option value="daily">after a day</option><option value="weekly">after a week</option><option value="always">at once</option></select></label>
            <label className="wb-check"><input type="checkbox" checked={q.abandon !== false} onChange={e => set('abandon', e.target.checked ? undefined : false)}/>may be abandoned from the journal</label>
        </div>
        <div className="wb-chips"><span>Offered or started in:</span>{program.phases.map(ph => <label key={ph.id} className="wb-check"><input type="checkbox"
            checked={!q.phases || q.phases.includes(ph.id)} onChange={e => {
                const now = new Set(q.phases ?? program.phases.map(p => p.id));
                if (e.target.checked) now.add(ph.id); else now.delete(ph.id);
                set('phases', now.size === program.phases.length ? undefined : program.phases.map(p => p.id).filter(p => now.has(p)));
            }}/>{ph.id}</label>)}{!q.phases && <small className="hint">(every phase)</small>}</div>

        <h5>Who gives it</h5>
        <div className="wb-line"><span className="wb-glyph">☻</span>
            <Sentence parts={giverPart} clearable={() => true} pick={(_s, v) => {
                const g = v.giver === undefined ? undefined : String(v.giver);
                if (g) save({...q, giver: g, offer: q.offer ?? {say: 'There is something you could do for me.'}});
                else { const {giver: _g, offer: _o, handIn: _h, ...rest} = q; void _g; void _o; void _h; save(rest); }
            }}/></div>
        <div className="wb-line"><span className="wb-glyph">▤</span>
            <Sentence parts={boardPart} clearable={() => true} pick={(_s, v) => {
                const b = v.board === undefined ? undefined : String(v.board);
                if (b) save({...q, board: b}); else { const {board: _b, ...rest} = q; void _b; save(rest); }
            }}/></div>
        {!q.giver && !q.board && <p className="hint">No giver: a trigger gives it (Start a quest) or offers it (Offer a quest: the wolf accepts or declines in the journal).</p>}
        {q.board && <p className="hint">On the town's notice board, under QUESTS, to wolves who may take it (TAKE IT ON){q.giver ? '; handed in to its giver' : ''}.</p>}
        {q.giver && <div className="wb-offer">
            <p className="hint">In the resident's dialogue window. The resident's lines are said in their own voice (#symbols# of the talk library expand).</p>
            <label>The wolf asks<input maxLength={120} value={offer.ask ?? ''} placeholder={'Ask about: ' + q.title} onChange={e => setOffer('ask', e.target.value)}/></label>
            <label>The resident offers it<textarea rows={2} maxLength={300} value={offer.say} onChange={e => setOffer('say', e.target.value)}/></label>
            <div className="wb-head">
                <label>The wolf accepts<input maxLength={120} value={offer.accept ?? ''} placeholder="I'll do it." onChange={e => setOffer('accept', e.target.value)}/></label>
                <label>The wolf declines<input maxLength={120} value={offer.decline ?? ''} placeholder="Not now." onChange={e => setOffer('decline', e.target.value)}/></label>
            </div>
            <label className="wb-check"><input type="checkbox" checked={q.handIn !== false} onChange={e => set('handIn', e.target.checked ? undefined : false)}/>
                handed in to the resident to finish it</label>
            {q.handIn !== false && <div className="wb-head">
                <label>The wolf hands it in<input maxLength={120} value={offer.handIn ?? ''} placeholder={`About ${q.title}: it's done.`} onChange={e => setOffer('handIn', e.target.value)}/></label>
                <label className="wb-grow">The resident's thanks<input maxLength={300} value={offer.thanks ?? ''} placeholder="(the game's own thanks)" onChange={e => setOffer('thanks', e.target.value)}/></label>
            </div>}
        </div>}

        <h5>Who may take it</h5>
        <div className="wb-head">
            <label>Who<select value={join.who ?? (open ? 'anyone' : 'participants')} onChange={e => setJoin('who', e.target.value === (open ? 'anyone' : 'participants') ? undefined : e.target.value)}>
                <option value="anyone">any wolf who asks (joins the story)</option><option value="participants">only the story's wolves</option></select></label>
            <label>Under way at once, at most<input type="number" min={1} max={5000} value={join.cap ?? ''} placeholder="no cap"
                onChange={e => setJoin('cap', e.target.value === '' ? undefined : Math.max(1, Math.min(5000, Math.round(e.target.valueAsNumber))))}/></label>
            <label>Taken in all, at most<input type="number" min={1} max={100000} value={join.limit ?? ''} placeholder="no limit"
                onChange={e => setJoin('limit', e.target.value === '' ? undefined : Math.max(1, Math.min(100000, Math.round(e.target.valueAsNumber))))}/></label>
            <label title="Doc 86, N4: after it ends any way (done, failed or abandoned), the same wolf waits this long">The same wolf again after (game days)
                <input type="number" min={1} max={365} value={join.again ?? ''} placeholder="at once"
                onChange={e => setJoin('again', e.target.value === '' ? undefined : Math.max(1, Math.min(365, Math.round(e.target.valueAsNumber))))}/></label>
        </div>
        {(join.who ?? (open ? 'anyone' : 'participants')) === 'anyone' && !open && <p className="hint">Any wolf may take it only with an open or "everyone" audience (Campaign settings).</p>}
        {(program.regions?.length ?? 0) > 0 && <div className="wb-line"><span className="wb-glyph">▭</span>
            <Sentence parts={regionPart} clearable={() => true} pick={(_s, v) => setJoin('region', v.region === undefined ? undefined : String(v.region))}/></div>}

        <h5>What to do <small className="hint">{objectives.filter(o => !o.optional).length} required, {objectives.filter(o => o.optional).length} optional, up to 12</small></h5>
        <ol className="wb-actions">{objectives.map((o, i) => <ObjectiveRow key={o.id + ':' + i} q={q} o={o} i={i}
            change={(next, mark) => putObjectives(objectives.map((x, n) => n === i ? next : x), mark)}
            remove={() => putObjectives(objectives.filter((_, n) => n !== i).map(x => x.after ? (() => { const a = x.after!.filter(y => y !== o.id);
                const {after: _a, ...rest} = x; void _a; return a.length ? {...rest, after: a} : rest; })() : x))}
            move={to => { const list = [...objectives]; const [x] = list.splice(i, 1); list.splice(to, 0, x); putObjectives(list); }}
            rename={to => edit(renameObjective(program, q.id, o.id, to))}/>)}
            {objectives.length < 12 && <li className="wb-add-row"><button type="button" className="wb-add" onClick={() => {
                let n = objectives.length + 1;
                while (objectives.some(o => o.id === 'step-' + n)) n++;
                const ev = objectiveEvents(cat).find(e => e.id === 'conversation');
                putObjectives([...objectives, {id: 'step-' + n, label: 'Another thing to do.', on: ev?.id ?? 'conversation'}]);
            }}>+ objective</button></li>}</ol>

        <h5>What fails it {!(q.fail ?? []).length && <small className="hint">nothing: it stays under way until done or abandoned</small>}</h5>
        <ol className="wb-actions">{(q.fail ?? []).map((f, i) => <FailRow key={i} rule={f} change={(next, mark) => set('fail', (q.fail ?? []).map((x, n) => n === i ? next : x), mark)}
            remove={() => set('fail', (q.fail ?? []).filter((_, n) => n !== i))}/>)}
            {(q.fail ?? []).length < 4 && <li className="wb-add-row"><span className="wb-chips">
                <button type="button" className="wb-add" onClick={() => set('fail', [...(q.fail ?? []), {on: 'death', ...(q.giver ? {match: {actor: q.giver}} : {})}])}>+ an event fails it</button>
                <button type="button" className="wb-add" onClick={() => set('fail', [...(q.fail ?? []), {afterDays: 3}])}>+ a deadline</button></span></li>}</ol>

        <h5>Rewards <small className="hint">to each wolf with credit (“the acting wolf”); coins come from a named purse, never minted</small></h5>
        <EffectList rewards max={8} effects={q.rewards ?? []} change={e => set('rewards', e)} problems={i => problems[`${key}:rewards[${i}]`]}/>
        <h5>Bonus <small className="hint">given too when every optional objective was done</small></h5>
        <EffectList rewards max={4} effects={q.bonus ?? []} change={e => set('bonus', e)} problems={i => problems[`${key}:bonus[${i}]`]}/>

        <p><button type="button" onClick={() => {
            const taken = new Set(quests.map(x => x.id));
            let copy = q.id + '-copy';
            for (let n = 2; taken.has(copy); n++) copy = q.id + '-copy-' + n;
            edit(withQuests(program, [...quests.slice(0, index + 1), {...structuredClone(q), id: copy}, ...quests.slice(index + 1)])); select({kind: 'quest', index: index + 1});
        }}>Duplicate</button>
            <button type="button" className="wb-x-text" onClick={() => {
                if (!window.confirm(`Remove the quest ${q.id}? Actions that name it will need another. (Undo brings it back.)`)) return;
                edit(withQuests(program, quests.filter((_, i) => i !== index))); select({kind: 'quests'});
            }}>Remove this quest</button></p>
    </article>;
}

/** The Quests node of the tree: the story's quests, and + Quest. */
export function QuestList({select, add}: {select: (r: Ref) => void; add: () => void}) {
    const {program, names} = useBench();
    const quests = program.quests ?? [];
    return <article className="wb-trigger" aria-label="Quests">
        <h4>Quests · {quests.length}</h4>
        <p className="hint">A quest any wolf may pick up from a resident, or one a trigger gives: objectives in any order, optional and hidden ones, fail rules,
            rewards. {program.version !== 2 && 'Quests need a version 2 story: Make it version 2… (under Variables, on the right).'}</p>
        <ul>{quests.map((q, i) => <li key={q.id + i}><button type="button" className="link-button" onClick={() => select({kind: 'quest', index: i})}>{q.title || q.id}</button>
            <small> {q.id} · {q.objectives.length} objective{q.objectives.length === 1 ? '' : 's'}{q.giver ? ' · from ' + (names.resident?.[q.giver] ?? q.giver) : ''}</small></li>)}</ul>
        <button type="button" disabled={program.version !== 2} onClick={add}>+ Quest</button>
    </article>;
}
