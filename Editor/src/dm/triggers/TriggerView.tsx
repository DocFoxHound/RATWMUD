// The selected trigger as Warcraft III shows one (Docs/Design/85, 4.2): Events, Conditions and Actions in sentences from the
// catalogue, every parameter a slot with the picker for its type. Also the action list that a phase's entry effects share.
import {useState} from 'react';
import {
    actionItem, actionParts, allActions, AMOUNTS, conditionParts, effectParts, eventOf, eventParts, FIELDS, headWords, isV2, isWorldCondition, itemOf, newAction, refusable,
    newCondition, triggersOf, usesOf, withEvent, withTriggers, type Action, type Condition, type Effect, type Part, type Program, type Ref,
    type CatItem, type Slot, type Trigger,
} from '../../lib/workbench';
import {useBench} from './context';
import {ActionPicker, EventPicker, type Values} from './pickers';
import {Sentence} from './Sentence';

const CUE_KINDS: Record<string, string> = {'scene arrived': 'npc.walk', 'scene blocked': 'npc.walk', 'scene spoken': 'npc.speak', 'scene posed': 'npc.pose',
    'scene finished': 'scene.play', 'scene failed': 'scene.play', 'scene choice': 'scene.play', 'scene response': 'scene.play'};
/** The cues a scene event of this kind can come back with: the cues the story's own effects give (as the retired SceneCueFilter worked them out). */
export function cuesFor(p: Program, on: string): string[] {
    const kind = CUE_KINDS[on];
    if (!kind) return [];
    const effects = [...p.phases.flatMap(ph => ph.effects), ...[...(p.triggers ?? []), ...p.phases.flatMap(ph => ph.triggers ?? [])]
        .flatMap(t => allActions(t.actions).flatMap(({a}) => a.do === 'effect' ? [a.effect] : []))].filter(e => e.kind === kind && typeof e.cue === 'string' && e.cue);
    const out = new Set<string>();
    for (const e of effects) {
        const cue = String(e.cue);
        if (on !== 'scene choice' && on !== 'scene response') { out.add(cue); continue; }
        for (const b of p.scenes?.find(s => s.id === e.scene)?.beats ?? []) {
            if (on === 'scene choice' && b.type === 'choice') b.options.forEach(o => out.add(cue + '.' + o.id));
            if (on === 'scene response' && b.type === 'response') { out.add(cue + '.' + b.id + '.submitted'); out.add(cue + '.' + b.id + '.timeout'); }
        }
    }
    return [...out];
}

/** Values picked for slots of a record: undefined removes the key. */
function merged<T extends object>(record: T, v: Values): T {
    const next = {...record} as Record<string, unknown>;
    for (const [k, value] of Object.entries(v)) { if (value === undefined) delete next[k]; else next[k] = value; }
    return next as T;
}

// ---------------------------------------------------------------------------------------------------------- conditions

function textOps(ps: Part[], c: Condition): Part[] {
    if (c.test !== 'field' || (AMOUNTS as readonly string[]).includes(c.field)) return ps;
    return ps.map(p => 'slot' in p && p.slot === 'op' && p.param ? {...p, param: {...p.param, values: ['=', '!=']}} : p);
}

/** One condition or group. `each` (doc 86, R1): a test for each wolf, where only the story's own state may be read. */
export function ConditionNode({node, change, remove, depth, phase, problems, each}: {node: Condition; change: (c: Condition) => void; remove?: () => void; depth: number;
    phase: number | null; problems?: string[]; each?: boolean}) {
    const {cat, names, program} = useBench();
    const [adding, setAdding] = useState(false);
    if (node.test === 'all' || node.test === 'any' || node.test === 'not') {
        const children = node.test === 'not' ? [node.condition] : node.conditions;
        const set = (i: number, c: Condition | null) => {
            if (node.test === 'not') { if (c) change({test: 'not', condition: c}); else remove?.(); return; }
            const list = c ? children.map((x, n) => n === i ? c : x) : children.filter((_, n) => n !== i);
            if (!list.length) remove?.(); else change({...node, conditions: list});
        };
        const regroup = (test: 'all' | 'any' | 'not') => {
            if (test === node.test) return;
            if (test === 'not') change({test: 'not', condition: children.length === 1 ? children[0] : {test: node.test as 'all' | 'any', conditions: children}});
            else change({test, conditions: node.test === 'not' ? [node.condition] : children});
        };
        return <div className={'wb-group depth-' + depth}>
            <div className="wb-group-head"><select aria-label="Group" value={node.test} onChange={e => regroup(e.target.value as 'all' | 'any' | 'not')}>
                <option value="all">All of these</option><option value="any">Any of these</option><option value="not">Not</option></select>
                {remove && <button type="button" className="wb-x" onClick={remove} title="Remove this group">×</button>}</div>
            {children.map((c, i) => <ConditionNode key={i} node={c} depth={depth + 1} phase={phase} change={x => set(i, x)} remove={() => set(i, null)} each={each}/>)}
            {(node.test !== 'not') && children.length < 8 && (adding
                ? <AddCondition phase={phase} depth={depth} each={each} close={() => setAdding(false)} pick={c => { change({...node, conditions: [...children, c]}); setAdding(false); }}/>
                : <button type="button" className="wb-add" onClick={() => setAdding(true)}>+ condition</button>)}
        </div>;
    }
    const item = itemOf(cat.conditions, node.test);
    return <div className="wb-line">
        <span className="wb-glyph">◆</span>
        <Sentence parts={textOps(conditionParts(cat, node, names), node)} problems={problems} pick={(slot: Slot, v) => {
            let next = merged(node as unknown as Record<string, unknown>, v) as unknown as Condition;
            if (slot.slot === 'source' && next.test === 'participation') {
                // (Doc 86, R1: who counts is the list or the test, never both.)
                const {list, where, ...rest} = next;
                void list; void where;
                const wolves = (program.variables ?? []).find(x => x.type === 'wolves' && ['story', 'world'].includes(x.scope))?.id ?? 'did';
                const own = (program.variables ?? []).find(x => x.scope === 'player' && x.type === 'number')?.id ?? 'progress';
                next = rest.source === 'where' ? {...rest, where: {test: 'compare', left: {var: own, of: '$each'}, op: '>=', right: 1}} : {...rest, list: wolves};
            }
            if (slot.slot === 'field' && next.test === 'field' && node.test === 'field') {
                const amount = (AMOUNTS as readonly string[]).includes(next.field);
                if (amount !== (AMOUNTS as readonly string[]).includes(node.field)) next = {...next, op: '=', value: amount ? 0 : ''};
            }
            change(next);
        }}/>
        <small className="wb-help" title={item?.help}>?</small>
        {remove && <button type="button" className="wb-x" onClick={remove} title="Remove this condition">×</button>}
        {program && null}
    </div>;
}

const EACH_TESTS = ['all', 'any', 'not', 'compare', 'switch', 'in list', 'days since', 'quest is', 'objective is'];
function AddCondition({pick, close, phase, depth, each}: {pick: (c: Condition) => void; close: () => void; phase: number | null; depth: number; each?: boolean}) {
    const {cat, program} = useBench();
    return <div className="wb-picker" role="dialog" aria-label="Add a condition"><header><strong>Add a condition</strong>
        <button type="button" className="wb-close" onClick={close} aria-label="Close">×</button></header>
        {(() => {
            const list = cat.conditions.filter(c => (depth < 5 || !c.group) && !(c.phaseOnly && phase === null)
                && (isV2(program) ? c.id !== 'counter' : !c.v2)
                && !((c.id === 'region' || ['wolf in region', 'count in region', 'resident in region'].includes(c.id)) && !program.regions?.length)
                && !(['quest is', 'objective is'].includes(c.id) && !program.quests?.length)
                && !(c.id === 'mission' && !(phase !== null && program.phases[phase]?.missions?.length)));
            // (Doc 85, Phase 8: grouped as section 3.4 has them: the story's own state, the world's facts as of the event, quests.)
            const groups: [string, CatItem[]][] = [['The story', list.filter(c => !isWorldCondition(c))],
                ['The world, as of the event', list.filter(c => isWorldCondition(c) && !c.id.startsWith('quest') && !c.id.startsWith('objective'))],
                ['Quests', list.filter(c => c.id === 'quest is' || c.id === 'objective is')]];
            return <div className="wb-options">{groups.filter(([, items]) => items.length).map(([label, items]) => <div key={label}><h6>{label}</h6>
                {items.map(c => <button key={c.id} type="button" className="wb-action-option" disabled={each && !EACH_TESTS.includes(c.id)}
                    title={each && !EACH_TESTS.includes(c.id) ? 'Not in a test for each wolf: it is counted outside an event' : undefined}
                    onClick={() => pick(newCondition(c.id, program, phase, cat))}>
                    <span>{c.title}</span><small>{c.help}</small></button>)}</div>)}</div>;
        })()}</div>;
}

// ------------------------------------------------------------------------------------------------------------- actions

/** The condition of an If (or Else if) inside an action list, as the trigger's own are shown: an All group around it. */
function IfCondition({c, change, phase, problems}: {c: Condition; change: (c: Condition) => void; phase: number | null; problems?: string[]}) {
    const group = c.test === 'all' || c.test === 'any' || c.test === 'not';
    return <ConditionNode node={group ? c : {test: 'all', conditions: [c]}} depth={2} phase={phase} problems={problems}
        change={x => change(x.test === 'all' && x.conditions.length === 1 && !group ? x.conditions[0] : x)}/>;
}

/** Version 2's lists inside an action (doc 85, 4.2: "Actions nest under If, Else, For each, Random"), each an action list of its own. */
function Nested({a, set, path, at, phase, depth, trigger}: {a: Action; set: (a: Action) => void; path: string; at?: (part: string) => string[] | undefined;
    phase: number | null; depth: number; trigger?: string}) {
    const list = (actions: Action[], part: string, put: (l: Action[]) => void) => <ActionList actions={actions} change={put} path={part} at={at} depth={depth + 1}
        phase={phase} trigger={trigger} max={24}/>;
    if (a.do === 'if') return <div className="wb-nest">
        <div className="wb-nest-head">If</div>
        <IfCondition c={a.if} phase={phase} problems={at?.(path + '.if')} change={c => set({...a, if: c})}/>
        <div className="wb-nest-head">then</div>
        {list(a.then, path + '.then', then => set({...a, then}))}
        {(a.elif ?? []).map((b, k) => <div key={k} className="wb-nest-branch">
            <div className="wb-nest-head">Else if <button type="button" className="wb-x" title="Remove this Else if" onClick={() => {
                const elif = (a.elif ?? []).filter((_, n) => n !== k); const {elif: _e, ...rest} = a; void _e; set(elif.length ? {...rest, elif} : rest); }}>×</button></div>
            <IfCondition c={b.if} phase={phase} change={c => set({...a, elif: (a.elif ?? []).map((x, n) => n === k ? {...x, if: c} : x)})}/>
            <div className="wb-nest-head">then</div>
            {list(b.then, `${path}.elif[${k}].then`, then => set({...a, elif: (a.elif ?? []).map((x, n) => n === k ? {...x, then} : x)}))}</div>)}
        {a.else && <div className="wb-nest-branch"><div className="wb-nest-head">Else <button type="button" className="wb-x" title="Remove Else" onClick={() => {
            const {else: _e, ...rest} = a; void _e; set(rest); }}>×</button></div>
            {list(a.else, path + '.else', other => set({...a, else: other}))}</div>}
        <div className="wb-chips">{(a.elif ?? []).length < 4 && <button type="button" className="wb-add" onClick={() => set({...a, elif: [...(a.elif ?? []),
            {if: a.if, then: [{do: 'note', text: 'Else if…'}]}]})}>+ else if</button>}
            {!a.else && <button type="button" className="wb-add" onClick={() => set({...a, else: [{do: 'note', text: 'Otherwise…'}]})}>+ else</button>}</div>
    </div>;
    if (a.do === 'each') return <div className="wb-nest"><div className="wb-nest-head">do, for each ($each)</div>{list(a.actions, path + '.actions', actions => set({...a, actions}))}</div>;
    if (a.do === 'random') return <div className="wb-nest">{a.choices.map((c, k) => <div key={k} className="wb-nest-branch">
        <div className="wb-nest-head">Choice {k + 1}, weight <input type="number" min={1} max={100} value={c.weight} aria-label={'Weight of choice ' + (k + 1)}
            onChange={e => set({...a, choices: a.choices.map((x, n) => n === k ? {...x, weight: Math.max(1, Math.min(100, Math.round(Number(e.target.value) || 1)))} : x)})}/>
            {a.choices.length > 2 && <button type="button" className="wb-x" title="Remove this choice" onClick={() => set({...a, choices: a.choices.filter((_, n) => n !== k)})}>×</button>}</div>
        {list(c.actions, `${path}.choices[${k}].actions`, actions => set({...a, choices: a.choices.map((x, n) => n === k ? {...x, actions} : x)}))}</div>)}
        {a.choices.length < 8 && <button type="button" className="wb-add" onClick={() => set({...a, choices: [...a.choices, {weight: 1, actions: [{do: 'note', text: 'Or…'}]}]})}>+ choice</button>}
    </div>;
    return null;
}

/** A list of actions (a trigger's, or one inside an If, For each or Random) or of effects (a phase's entry), as numbered sentences. */
export function ActionList({actions, change, problems, entry = false, max = 12, path = 'actions', at, depth = 0, phase = null, trigger, rewards = false}: {actions: Action[];
    change: (a: Action[]) => void; problems?: (i: number) => string[] | undefined; entry?: boolean; max?: number; rewards?: boolean;
    /** Version 2: where this list is in the trigger ("actions[2].then"), and the problems of a part by its name. */
    path?: string; at?: (part: string) => string[] | undefined; depth?: number; phase?: number | null; trigger?: string}) {
    const {cat, names, program} = useBench();
    const [adding, setAdding] = useState(false);
    const move = (from: number, to: number) => { const list = [...actions]; const [a] = list.splice(from, 1); list.splice(to, 0, a); change(list); };
    return <ol className={'wb-actions' + (depth ? ' nested' : '')}>
        {actions.map((a, i) => {
            const help = a.do === 'effect' ? itemOf(cat.effects, a.effect.kind) : actionItem(cat, a);
            const part = `${path}[${i}]`;
            const set = (next: Action) => change(actions.map((x, n) => n === i ? next : x));
            return <li key={i} className={'wb-line' + (help?.nest ? ' wb-container' : '')}><span className="wb-glyph">{i + 1}</span>
                <Sentence parts={actionParts(cat, a, names)} problems={problems?.(i) ?? at?.(part)} cues={a.do === 'effect' ? [] : undefined} pick={(_slot, v) => {
                    const next = a.do === 'effect' ? {...a, effect: merged(a.effect, v) as Effect} : merged(a, v);
                    set(next as Action);
                }}/>
                {help && <small className="wb-help" title={help.help + (help.actsOnWolves ? " (Acts on players' wolves.)" : '')}>?</small>}
                {a.do === 'effect' && refusable(cat, a.effect.kind) && <RefusalPicker effect={a.effect} reward={rewards}
                    change={effect => set({...a, effect} as Action)}/>}
                <span className="wb-tools"><button type="button" disabled={!i} onClick={() => move(i, i - 1)} title="Earlier" aria-label={'Move action ' + (i + 1) + ' earlier'}>↑</button>
                    <button type="button" disabled={i === actions.length - 1} onClick={() => move(i, i + 1)} title="Later" aria-label={'Move action ' + (i + 1) + ' later'}>↓</button>
                    <button type="button" className="wb-x" onClick={() => change(actions.filter((_, n) => n !== i))} title="Remove" aria-label={'Remove action ' + (i + 1)}>×</button></span>
                {help?.nest && <Nested a={a} set={set} path={part} at={at} phase={phase} depth={depth} trigger={trigger}/>}
            </li>;
        })}
        {actions.length < max && <li className="wb-add-row">{adding
            ? <ActionPicker entry={entry} rewards={rewards} close={() => setAdding(false)} pick={kind => {
                const a = newAction(cat, kind, program, trigger, phase);
                // (Going to a phase is the last action; a new action goes before it.)
                const last = actions.length && actions[actions.length - 1].do === 'enter' && a.do !== 'enter';
                change(last ? [...actions.slice(0, -1), a, actions[actions.length - 1]] : [...actions, a]); setAdding(false);
            }}/>
            : <button type="button" className="wb-add" onClick={() => setAdding(true)}>{rewards ? '+ reward' : entry ? '+ effect' : '+ action'}</button>}</li>}
    </ol>;
}

/** Doc 85, 3.5 and Phase 7: what the runner does if the game refuses a world effect: pause the story (the default), note
 *  it and carry on, or hold it for its wolf until they are next in the world (for up to so many days). */
function RefusalPicker({effect, reward, change}: {effect: Effect; reward: boolean; change: (e: Effect) => void}) {
    const how = typeof effect.ifRefused === 'string' ? effect.ifRefused : '';
    const toWolf = typeof effect.player === 'string' && effect.player !== '' && effect.player !== 'world' && effect.kind !== 'scene.play';
    const set = (next: string, days?: number) => {
        const {ifRefused: _i, waitDays: _w, ...rest} = effect; void _i; void _w;
        change({...rest, ...(next ? {ifRefused: next} : {}), ...(next === 'when here' && days ? {waitDays: days} : {})} as Effect);
    };
    return <span className="wb-refusal" title="If the game refuses it (a wolf away, a purse short, a full pack)">
        <select aria-label="If the game refuses it" value={how} onChange={e => set(e.target.value, typeof effect.waitDays === 'number' ? effect.waitDays : undefined)}>
            <option value="">{reward && toWolf ? 'if refused: wait for the wolf' : 'if refused: pause the story'}</option>
            <option value="pause">if refused: pause the story</option>
            <option value="skip">if refused: note it, carry on</option>
            {toWolf && <option value="when here">if refused: wait for the wolf</option>}
        </select>
        {how === 'when here' && <label> up to <input type="number" min={1} max={90} aria-label="Days to wait for the wolf"
            value={typeof effect.waitDays === 'number' ? effect.waitDays : 7}
            onChange={e => set('when here', Math.max(1, Math.min(90, Math.round(Number(e.target.value) || 7))))}/> days</label>}
    </span>;
}

/** A phase's entry effects, as the action list shows them (effects only). */
export function EffectList({effects, change, problems, rewards = false, max}: {effects: Effect[]; change: (e: Effect[]) => void; problems?: (i: number) => string[] | undefined;
    rewards?: boolean; max?: number}) {
    return <ActionList entry={!rewards} rewards={rewards} max={max} actions={effects.map(effect => ({do: 'effect', effect}) as Action)} problems={problems}
        change={list => change(list.flatMap(a => a.do === 'effect' ? [a.effect] : []))}/>;
}
void effectParts;

// ------------------------------------------------------------------------------------------------------------- trigger

export function TriggerView({at, select, fired}: {at: Ref & {kind: 'trigger'}; select: (r: Ref) => void; fired?: number}) {
    const {cat, names, program, edit, problems} = useBench();
    const [choosing, setChoosing] = useState(false);
    const [filtering, setFiltering] = useState(false);
    const list = triggersOf(program, at.phase);
    const t = list[at.index];
    if (!t) return <p className="hint">This trigger is gone.</p>;
    const key = `t:${at.phase ?? 'c'}:${at.index}`;
    const save = (next: Trigger, mark?: string) => edit(withTriggers(program, at.phase, list.map((x, i) => i === at.index ? next : x)), mark);
    const ev = eventOf(cat, t.on);
    const cues = cuesFor(program, t.on);
    const scene = !!CUE_KINDS[t.on] && ['arrived', 'spoken', 'posed', 'blocked'].some(w => t.on.endsWith(w));
    const filterable = (ev ? Object.keys(ev.fields) : [...FIELDS]).filter(k => (FIELDS as readonly string[]).includes(k) && !(k in (t.match ?? {})));
    const folders = [...new Set(list.map(x => x.folder).filter(Boolean) as string[])];
    const sends = allActions(t.actions).flatMap(({a}) => a.do === 'emit' || a.do === 'schedule' ? [a.signal] : []);
    const heard = t.on === 'story signal' && t.signal ? [t.signal] : [];
    return <article className="wb-trigger" aria-label={'Trigger ' + t.id}>
        <div className="wb-head">
            <label>Name<input value={t.id} onChange={e => save({...t, id: e.target.value}, key + ':id')} spellCheck={false}/></label>
            <label>Folder<input list={key + '-folders'} value={t.folder ?? ''} placeholder="(none)" onChange={e => {
                const {folder, ...rest} = t; void folder; save(e.target.value ? {...rest, folder: e.target.value} : rest, key + ':folder'); }}/>
                <datalist id={key + '-folders'}>{folders.map(f => <option key={f} value={f}/>)}</datalist></label>
            <label>Fires<select value={t.once ?? 'each'} disabled={t.on === 'clock'} onChange={e => save({...t, once: e.target.value})}>
                {Object.entries(cat.trigger.once).map(([k, w]) => <option key={k} value={k} disabled={scene && (k === 'player' || k === 'member')}>{w}</option>)}</select></label>
            <label>For<select value={t.audience ?? 'world'} disabled={t.on === 'clock' || scene} onChange={e => save({...t, audience: e.target.value})}>
                {Object.entries(cat.trigger.audience).map(([k, w]) => <option key={k} value={k}>{w}</option>)}</select></label>
            {/* (Doc 85, 4.2 and Phase 5: Enabled; off, it starts turned off until an Enable action or the console turns it on.) */}
            <label className="wb-check" title={isV2(program) ? 'Off: the trigger starts turned off, until an Enable action or the Campaigns console turns it on.'
                : 'A version 2 story can start a trigger turned off (Settings: Make it version 2).'}>
                <input type="checkbox" checked={t.enabled !== false} disabled={!isV2(program)} onChange={e => {
                    const {enabled, ...rest} = t; void enabled; save(e.target.checked ? rest : {...rest, enabled: false}); }}/>Enabled at the start</label>
        </div>
        <label className="wb-note">Note<textarea rows={1} maxLength={300} value={t.note ?? ''} placeholder="A line for whoever reads this trigger next"
            onChange={e => { const {note, ...rest} = t; void note; save(e.target.value ? {...rest, note: e.target.value} : rest, key + ':note'); }}/></label>
        <p className="hint">{headWords(cat, t)}{at.phase === null ? ' · a campaign trigger, tried before the phase\'s own' : ` · tried ${at.index + 1}${['st', 'nd', 'rd'][at.index] ?? 'th'} in ${program.phases[at.phase].id}`}
            {fired !== undefined && <> · <strong className={fired ? 'ok' : 'warn'}>{fired ? `fired ${fired}× in the last rehearsal` : 'no rehearsal reached it'}</strong></>}</p>

        <h5>Events</h5>
        <div className="wb-line"><span className="wb-glyph">⚑</span>
            <Sentence parts={eventParts(cat, t, names)} problems={problems[key]} cues={cues} clearable={s => (FIELDS as readonly string[]).includes(s.slot)}
                pick={(slot, v) => {
                    if (slot.slot === 'signal') save({...t, signal: String(v.signal)});
                    else if (slot.slot === 'afterDays') save({...t, afterDays: Number(v.afterDays)});
                    else { const match = merged(t.match ?? {}, v) as Record<string, string>; const {match: _m, ...rest} = t; void _m; save(Object.keys(match).length ? {...rest, match} : rest); }
                }}/>
            <small className="wb-help" title={ev?.help}>?</small>
            <button type="button" onClick={() => setChoosing(!choosing)}>Change event…</button>
            {filterable.length > 0 && t.on !== 'story signal' && t.on !== 'clock' && <button type="button" onClick={() => setFiltering(!filtering)}>+ filter</button>}
        </div>
        {ev?.help && <p className="hint wb-event-line">{ev.help}{Object.keys(ev.fields).length ? ' ' + Object.entries(ev.fields).map(([k, f]) => `${k}: ${f.label}`).join(' · ') : ''}</p>}
        {filtering && <div className="wb-chips"><span>Only when the event's</span>{filterable.map(k => <button key={k} type="button" onClick={() => {
            save({...t, match: {...t.match, [k]: ''}}); setFiltering(false); }}>{ev?.fields[k]?.label ?? k}</button>)}<span>is…</span></div>}
        {choosing && <EventPicker value={t.on} close={() => setChoosing(false)} pick={on => { save(withEvent(t, on)); setChoosing(false); }}/>}

        <h5>Conditions {t.if ? '' : <small className="hint">none: it fires on every matching event</small>}</h5>
        {t.if ? <ConditionNode node={t.if.test === 'all' || t.if.test === 'any' || t.if.test === 'not' ? t.if : {test: 'all', conditions: [t.if]}} depth={1} phase={at.phase}
            problems={problems[key + ':if']} change={c => save({...t, if: c.test === 'all' && c.conditions.length === 1 && t.if?.test !== 'all' ? c.conditions[0] : c})}
            remove={() => { const {if: _i, ...rest} = t; void _i; save(rest); }}/>
            : <button type="button" className="wb-add" onClick={() => save({...t, if: newCondition('all', program, at.phase)})}>+ condition</button>}
        {problems[key + ':if'] && <p className="wb-problem">✖ {problems[key + ':if'].join(' ')}</p>}

        <h5>Actions{isV2(program) && <small className="hint"> up to 24, counting those inside If, For each and Random</small>}</h5>
        <ActionList actions={t.actions} change={actions => save({...t, actions})} at={part => problems[`${key}:${part}`]} phase={at.phase} trigger={t.id}
            max={isV2(program) ? 24 : 12}/>

        {(sends.length > 0 || heard.length > 0) && <section className="wb-links"><h5>Signal connections</h5>
            {[...new Set([...sends, ...heard])].map(name => {
                const uses = usesOf(program, {type: 'signal', id: name}).filter(u => !(u.ref.kind === 'trigger' && u.ref.phase === at.phase && u.ref.index === at.index));
                return <p key={name}><code>{name}</code>: {uses.length ? uses.map((u, i) => <button key={i} type="button" onClick={() => select(u.ref)}>{u.where}</button>)
                    : <span className="warn">{sends.includes(name) ? 'nothing else listens for it' : 'nothing sends it'}</span>}</p>;
            })}</section>}
    </article>;
}
