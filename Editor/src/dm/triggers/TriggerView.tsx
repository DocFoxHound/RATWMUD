// The selected trigger as Warcraft III shows one (Docs/Design/85, 4.2): Events, Conditions and Actions in sentences from the
// catalogue, every parameter a slot with the picker for its type. Also the action list that a phase's entry effects share.
import {useState} from 'react';
import {
    actionParts, AMOUNTS, conditionParts, effectParts, eventOf, eventParts, FIELDS, headWords, itemOf, newAction, newCondition,
    triggersOf, usesOf, withEvent, withTriggers, type Action, type Condition, type Effect, type Part, type Program, type Ref,
    type Slot, type Trigger,
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
        .flatMap(t => t.actions.flatMap(a => a.do === 'effect' ? [a.effect] : []))].filter(e => e.kind === kind && typeof e.cue === 'string' && e.cue);
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

function ConditionNode({node, change, remove, depth, phase, problems}: {node: Condition; change: (c: Condition) => void; remove?: () => void; depth: number;
    phase: number | null; problems?: string[]}) {
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
            {children.map((c, i) => <ConditionNode key={i} node={c} depth={depth + 1} phase={phase} change={x => set(i, x)} remove={() => set(i, null)}/>)}
            {(node.test !== 'not') && children.length < 8 && (adding
                ? <AddCondition phase={phase} depth={depth} close={() => setAdding(false)} pick={c => { change({...node, conditions: [...children, c]}); setAdding(false); }}/>
                : <button type="button" className="wb-add" onClick={() => setAdding(true)}>+ condition</button>)}
        </div>;
    }
    const item = itemOf(cat.conditions, node.test);
    return <div className="wb-line">
        <span className="wb-glyph">◆</span>
        <Sentence parts={textOps(conditionParts(cat, node, names), node)} problems={problems} pick={(slot: Slot, v) => {
            let next = merged(node as unknown as Record<string, unknown>, v) as unknown as Condition;
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

function AddCondition({pick, close, phase, depth}: {pick: (c: Condition) => void; close: () => void; phase: number | null; depth: number}) {
    const {cat, program} = useBench();
    return <div className="wb-picker" role="dialog" aria-label="Add a condition"><header><strong>Add a condition</strong>
        <button type="button" className="wb-close" onClick={close} aria-label="Close">×</button></header>
        <div className="wb-options">{cat.conditions.filter(c => (depth < 5 || !c.group) && !(c.phaseOnly && phase === null)
            && !(c.id === 'region' && !program.regions?.length) && !(c.id === 'mission' && !(phase !== null && program.phases[phase]?.missions?.length))).map(c =>
            <button key={c.id} type="button" className="wb-action-option" onClick={() => pick(newCondition(c.id, program, phase))}>
                <span>{c.title}</span><small>{c.help}</small></button>)}</div></div>;
}

// ------------------------------------------------------------------------------------------------------------- actions

/** A list of actions (a trigger's) or of effects (a phase's entry), as numbered sentences. */
export function ActionList({actions, change, problems, entry = false, max = 12}: {actions: Action[]; change: (a: Action[]) => void;
    problems?: (i: number) => string[] | undefined; entry?: boolean; max?: number}) {
    const {cat, names, program} = useBench();
    const [adding, setAdding] = useState(false);
    const move = (from: number, to: number) => { const list = [...actions]; const [a] = list.splice(from, 1); list.splice(to, 0, a); change(list); };
    return <ol className="wb-actions">
        {actions.map((a, i) => {
            const help = a.do === 'effect' ? itemOf(cat.effects, a.effect.kind) : itemOf(cat.actions, a.do);
            return <li key={i} className="wb-line"><span className="wb-glyph">{i + 1}</span>
                <Sentence parts={actionParts(cat, a, names)} problems={problems?.(i)} cues={a.do === 'effect' ? [] : undefined} pick={(_slot, v) => {
                    const next = a.do === 'effect' ? {...a, effect: merged(a.effect, v) as Effect} : merged(a, v);
                    change(actions.map((x, n) => n === i ? next : x));
                }}/>
                {help && <small className="wb-help" title={help.help + (help.actsOnWolves ? " (Acts on players' wolves.)" : '')}>?</small>}
                <span className="wb-tools"><button type="button" disabled={!i} onClick={() => move(i, i - 1)} title="Earlier" aria-label={'Move action ' + (i + 1) + ' earlier'}>↑</button>
                    <button type="button" disabled={i === actions.length - 1} onClick={() => move(i, i + 1)} title="Later" aria-label={'Move action ' + (i + 1) + ' later'}>↓</button>
                    <button type="button" className="wb-x" onClick={() => change(actions.filter((_, n) => n !== i))} title="Remove" aria-label={'Remove action ' + (i + 1)}>×</button></span>
            </li>;
        })}
        {actions.length < max && <li className="wb-add-row">{adding
            ? <ActionPicker entry={entry} close={() => setAdding(false)} pick={kind => {
                const a = newAction(cat, kind, program);
                // (Going to a phase is the last action; a new action goes before it.)
                const last = actions.length && actions[actions.length - 1].do === 'enter' && a.do !== 'enter';
                change(last ? [...actions.slice(0, -1), a, actions[actions.length - 1]] : [...actions, a]); setAdding(false);
            }}/>
            : <button type="button" className="wb-add" onClick={() => setAdding(true)}>{entry ? '+ effect' : '+ action'}</button>}</li>}
    </ol>;
}

/** A phase's entry effects, as the action list shows them (effects only). */
export function EffectList({effects, change, problems}: {effects: Effect[]; change: (e: Effect[]) => void; problems?: (i: number) => string[] | undefined}) {
    return <ActionList entry actions={effects.map(effect => ({do: 'effect', effect}) as Action)} problems={problems}
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
    const sends = t.actions.flatMap(a => a.do === 'emit' || a.do === 'schedule' ? [a.signal] : []);
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

        <h5>Actions</h5>
        <ActionList actions={t.actions} change={actions => save({...t, actions})} problems={i => problems[`${key}:actions[${i}]`]}/>

        {(sends.length > 0 || heard.length > 0) && <section className="wb-links"><h5>Signal connections</h5>
            {[...new Set([...sends, ...heard])].map(name => {
                const uses = usesOf(program, {type: 'signal', id: name}).filter(u => !(u.ref.kind === 'trigger' && u.ref.phase === at.phase && u.ref.index === at.index));
                return <p key={name}><code>{name}</code>: {uses.length ? uses.map((u, i) => <button key={i} type="button" onClick={() => select(u.ref)}>{u.where}</button>)
                    : <span className="warn">{sends.includes(name) ? 'nothing else listens for it' : 'nothing sends it'}</span>}</p>;
            })}</section>}
    </article>;
}
