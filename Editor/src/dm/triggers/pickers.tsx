// The pickers of the trigger workbench (Docs/Design/85, 4.3): one per kind of parameter, so nobody types an id. Residents
// and wolves by name, places and tiles on the map, items from the catalogue by category, factions, towns, markets, ships,
// harvest areas, templates and projects from the world, and the story's own phases, regions, missions, scenes, signals
// and counters. "Enter an id" stays, for what the world does not list (or a world the refs could not be read from).
import {createContext, useContext, useMemo, useState} from 'react';
import {MapView} from '../MapView';
import {surfaceFor} from '../../lib/surface';
import {allActions, countersOf, figuresOf, fillsOf, isV2, kindOfType, LIST_TYPES, operandWords, purseNames, widestFilled, REF_KIND, signalsOf, talkKeysOf, talkValuesOf, visitorsOf, type CatEvent, type CatItem, type Condition, type Operand, type Param,
    type Program, type Slot} from '../../lib/workbench';
import {useBench} from './context';
import {ConditionNode} from './TriggerView';

export type Values = Record<string, unknown>;
interface Option {id: string; name: string; hint?: string; group?: string}

function SearchList({options, value, pick, placeholder, extra, limit = 60}: {options: Option[]; value: unknown; pick: (id: string) => void;
    placeholder: string; extra?: React.ReactNode; limit?: number}) {
    const [q, setQ] = useState('');
    const words = q.toLowerCase().split(/\s+/).filter(Boolean);
    const found = options.filter(o => words.every(w => (o.name + ' ' + o.id + ' ' + (o.hint ?? '') + ' ' + (o.group ?? '')).toLowerCase().includes(w)));
    const groups = [...new Set(found.slice(0, limit).map(o => o.group ?? ''))];
    return <div className="wb-search-list">
        <input type="search" autoFocus placeholder={placeholder} value={q} onChange={e => setQ(e.target.value)} aria-label={placeholder}/>
        {extra}
        <div className="wb-options">{groups.map(g => <div key={g}>{g && <h6>{g}</h6>}
            {found.slice(0, limit).filter(o => (o.group ?? '') === g).map(o => <button key={o.id} type="button" aria-pressed={o.id === value}
                onClick={() => pick(o.id)}><span>{o.name}</span>{o.name !== o.id && <code>{o.id}</code>}{o.hint && <small>{o.hint}</small>}</button>)}</div>)}
            {found.length > limit && <p className="hint">{found.length - limit} more: type to narrow.</p>}
            {!found.length && <p className="hint">Nothing matches.</p>}</div>
    </div>;
}

function EnterId({value, pick, label = 'Enter an id'}: {value: unknown; pick: (v: string) => void; label?: string}) {
    const [text, setText] = useState(typeof value === 'string' ? value : '');
    return <form className="wb-enter" onSubmit={e => { e.preventDefault(); if (text.trim()) pick(text.trim()); }}>
        <label>{label}<input value={text} onChange={e => setText(e.target.value)} maxLength={80}/></label><button type="submit">Use</button></form>;
}

/** The options of a reference type, from the world's refs and the story itself. */
export function useOptions(type: string): Option[] {
    const {refs, program} = useBench();
    return useMemo(() => {
        if (type === 'character') return [...(refs?.wolves ?? []).map(w => ({...w, group: 'Wolves'})), ...(refs?.residents ?? []).map(r => ({...r, group: 'Residents'}))];
        if (type === 'phase') return program.phases.map(p => ({id: p.id, name: p.id, hint: p.journal.slice(0, 60)}));
        if (type === 'region') return (program.regions ?? []).map(r => ({id: r.id, name: r.id, hint: `${r.cell} (${r.x1},${r.y1})–(${r.x2},${r.y2})`}));
        if (type === 'scene') return (program.scenes ?? []).map(s => ({id: s.id, name: s.title || s.id, hint: s.visibility}));
        if (type === 'mission') return program.phases.flatMap(p => (p.missions ?? []).map(m => ({id: m.id, name: m.title || m.id, group: p.id})));
        // (Phase 6: the story's quests; who pays a coin reward, by town.)
        if (type === 'quest') return (program.quests ?? []).map(q => ({id: q.id, name: q.title || q.id, hint: q.summary}));
        if (type === 'purse') return Object.entries(purseNames(refs)).map(([id, name]) => ({id, name, group: id.startsWith('till:') ? 'Tills' : 'Town treasuries'}));
        const kind = REF_KIND[type];
        const list = kind ? (refs?.[kind] as Option[] | undefined) ?? [] : [];
        if (type === 'item') return (refs?.items ?? []).map(i => ({id: i.id, name: i.name, group: i.category}));
        if (type === 'resident') return (refs?.residents ?? []).map(r => ({id: r.id, name: r.name, hint: r.work || r.home}));
        if (type === 'place') return (refs?.places ?? []).map(p => ({id: p.id, name: p.name, hint: p.kind + (p.town ? ' · ' + p.town : '')}));
        return list;
    }, [type, refs, program]);
}

function TilePicker({value, pick}: {value: {cell?: unknown; x?: unknown; y?: unknown}; pick: (v: Values) => void}) {
    const {world, program, refs} = useBench();
    const [cell, setCell] = useState(typeof value.cell === 'string' ? value.cell : '');
    const places = useOptions('place');
    const sheet = world && [...world.cells, ...world.rooms].find(c => c.id === cell);
    const surface = useMemo(() => world && sheet ? surfaceFor(world, {kind: 'cell', id: sheet.id}) : null, [world, sheet]);
    const regions = program.regions ?? [];
    const x = typeof value.x === 'number' ? value.x : null, y = typeof value.y === 'number' ? value.y : null;
    if (!cell) return <div className="wb-tile">
        {regions.length > 0 && <div className="wb-chips"><span>The centre of a region:</span>{regions.map(r => <button key={r.id} type="button"
            onClick={() => pick({cell: r.cell, x: Math.floor((r.x1 + r.x2) / 2), y: Math.floor((r.y1 + r.y2) / 2)})}>{r.id}</button>)}</div>}
        <SearchList options={places} value={cell} pick={setCell} placeholder={refs ? 'Find a place' : 'Find a place (the world could not be read)'}/>
        <EnterId value="" pick={setCell} label="Or enter a place id"/>
    </div>;
    return <div className="wb-tile">
        <p><button type="button" onClick={() => setCell('')}>← another place</button> <strong>{sheet?.name ?? cell}</strong>
            {sheet && <small> {sheet.width} × {sheet.height} tiles · click a tile</small>}</p>
        {surface && sheet ? <div className="wb-map"><MapView showElevationControl={false} surface={surface} markers={[]} selected={null} mode="pick"
            overlay={{paths: [], tiles: x !== null && y !== null && value.cell === cell ? [{x, y, color: '#ffd277', strong: true}] : [],
                rects: regions.filter(r => r.cell === cell).map(r => ({x: r.x1, y: r.y1, w: r.x2 - r.x1 + 1, h: r.y2 - r.y1 + 1, color: '#71bdc0', strong: false, label: r.id}))}}
            onClick={place => { if (place) pick({cell, x: place.x, y: place.y}); }} onPaint={() => {}} onOpen={() => {}}/></div>
            : <p className="hint">The map of this place is not loaded; enter the tile.</p>}
        <form className="wb-enter" onSubmit={e => { e.preventDefault(); const f = new FormData(e.currentTarget);
            pick({cell, x: Number(f.get('x')), y: Number(f.get('y'))}); }}>
            <label>X<input name="x" type="number" min={0} max={(sheet?.width ?? 4096) - 1} defaultValue={x ?? 0}/></label>
            <label>Y<input name="y" type="number" min={0} max={(sheet?.height ?? 4096) - 1} defaultValue={y ?? 0}/></label><button type="submit">Use</button></form>
    </div>;
}

function NumberPicker({param, value, pick, step}: {param?: Param; value: unknown; pick: (v: number) => void; step?: string}) {
    const [text, setText] = useState(typeof value === 'number' ? String(value) : '');
    const n = Number(text);
    const ok = text !== '' && Number.isFinite(n) && (param?.min === undefined || n >= param.min) && (param?.max === undefined || n <= param.max)
        && (step === 'any' || Number.isInteger(n)) && !(param?.nonzero && n === 0);
    return <form className="wb-enter" onSubmit={e => { e.preventDefault(); if (ok) pick(n); }}>
        <label>{param?.label ?? 'A number'}{param?.min !== undefined && <small> ({+param.min.toFixed(4)} to {param.max})</small>}
            <input autoFocus type="number" step={step ?? '1'} min={param?.min} max={param?.max} value={text} onChange={e => setText(e.target.value)}/></label>
        <button type="submit" disabled={!ok}>Use</button></form>;
}

function TextPicker({param, value, pick}: {param?: Param; value: unknown; pick: (v: string) => void}) {
    const {program} = useBench();
    const [text, setText] = useState(typeof value === 'string' ? value : '');
    const max = param?.max ?? 400;
    // (Doc 86, C1b: in a version 2 story, words marked "fills" may hold {name} or {name.of}; checked at their widest.)
    const fills = !!param?.fills && isV2(program);
    const vars = fills ? (program.variables ?? []).filter(v => !LIST_TYPES[v.type] && v.type !== 'timer') : [];
    const used = fills ? fillsOf(text) : [];
    const widest = fills && used.length ? widestFilled(text, program) : text.length;
    return <form className="wb-enter wb-text" onSubmit={e => { e.preventDefault(); if (text.trim() && text.length <= max) pick(text); }}>
        <label>{param?.label ?? 'Words'}<textarea autoFocus rows={2} maxLength={max} value={text} onChange={e => setText(e.target.value)}/>
            <small className={widest > max * 0.9 ? 'warn' : ''}>{text.length} / {max}{used.length > 0 && ` · up to ${widest} once filled`}</small></label>
        {fills && vars.length > 0 && <div className="wb-chips"><span>Fill in:</span>{vars.map(v => {
            const own = v.scope === 'player' || v.scope === 'party';
            const token = '{' + v.id + (own ? '.of' : '') + '}';
            return <button key={v.id} type="button" title={own ? "the acting (or looped) wolf's own" : undefined} onClick={() => setText(text + token)}>
                <u>{token}</u></button>;
        })}</div>}
        {used.length > 0 && <p className="hint">Filled as it is sent: {used.map(f => <u key={f.name + f.of}>{f.name}{f.of ? '.of' : ''}</u>).reduce<React.ReactNode[]>((a, x, i) => i ? [...a, ', ', x] : [x], [])}
            {' '}(a wolf, a resident, a place, an item or a faction by name; write {'{{'} and {'}}'} for braces).</p>}
        <button type="submit" disabled={!text.trim() || widest > max}>Use</button></form>;
}

function NamePicker({names, value, pick, noun}: {names: string[]; value: unknown; pick: (v: string) => void; noun: string}) {
    const [text, setText] = useState('');
    const ok = /^[A-Za-z0-9_][A-Za-z0-9_.:-]{0,79}$/.test(text);
    return <div className="wb-search-list">
        <div className="wb-options">{names.map(n => <button key={n} type="button" aria-pressed={n === value} onClick={() => pick(n)}><span>{n}</span></button>)}
            {!names.length && <p className="hint">This story has no {noun} yet.</p>}</div>
        <form className="wb-enter" onSubmit={e => { e.preventDefault(); if (ok) pick(text); }}>
            <label>A new {noun}<input value={text} onChange={e => setText(e.target.value)} placeholder="letters, digits, - _ . :"/></label>
            <button type="submit" disabled={!ok}>Use</button></form></div>;
}

/** Version 2: a declared variable of the types the slot takes (doc 85, 4.3). */
function VariablePicker({slot, pick}: {slot: Slot; pick: (v: string) => void}) {
    const {program, cat} = useBench();
    const types = slot.param?.of, scopes = slot.param?.scope;
    const list = (program.variables ?? []).filter(v => (!types || types.includes(v.type)) && (!scopes || scopes.includes(v.scope)) && !(scopes && v.mirror));
    return <div className="wb-search-list"><div className="wb-options">
        {list.map(v => <button key={v.id} type="button" aria-pressed={v.id === slot.value} onClick={() => pick(v.id)}><span>{v.id}</span>
            <small>{cat.variables?.types[v.type] ?? v.type} · {cat.variables?.scopes[v.scope] ?? v.scope}</small></button>)}
        {!list.length && <p className="hint">No variable of this kind yet ({(types ?? []).join(', ')}): declare one in Variables, on the right.</p>}</div></div>;
}

/** Doc 86, C1a: what may stand for an id parameter (story_triggers.OPERAND_VARS / OPERAND_FIELDS): a variable of its kind
 *  (or a text), the event's field that holds one. */
const OPERAND_VARS: Record<string, string[]> = {wolf: ['wolf'], resident: ['resident'], place: ['place', 'text'], item: ['item', 'text'],
    faction: ['faction', 'text'], town: ['text', 'region']};
const OPERAND_FIELDS: Record<string, string[]> = {wolf: ['actor', 'target'], resident: ['target', 'item'], place: ['cell', 'target'], item: ['item'],
    faction: ['target', 'detail'], town: ['actor', 'target', 'detail']};

/** Doc 86, C6: inside a quest's "Only for wolves who…": an offer is not an event, so the values offered are the story's own and
 *  the wolf's (the event's actor is the wolf tested); no other event field, no loop value, no share, not who is online. */
export const OfferValues = createContext(false);

/** Version 2: a value (doc 85, 3.4): a number, on or off, a text, a variable, the event's field, a list's count, a timer's
 *  days, the loop's value, or a variable plus or minus a number. `kind` narrows it: number, switch or text. */
function OperandPicker({slot, pick}: {slot: Slot; pick: (v: Operand) => void}) {
    const {program} = useBench();
    const offer = useContext(OfferValues);
    const kind = slot.param?.kind ?? '';
    const idKind = slot.param?.idKind && OPERAND_VARS[slot.param.idKind] ? slot.param.idKind : '';
    const vars = (program.variables ?? []).filter(v => !LIST_TYPES[v.type] && v.type !== 'timer'
        && (idKind ? OPERAND_VARS[idKind].includes(v.type) && !v.mirror : !kind || kindOfType(v.type) === kind));
    const lists = (program.variables ?? []).filter(v => LIST_TYPES[v.type]);
    const timers = (program.variables ?? []).filter(v => v.type === 'timer');
    const wolfLists = (program.variables ?? []).filter(v => v.type === 'wolves' && ['story', 'world'].includes(v.scope));
    const [text, setText] = useState(typeof slot.value === 'string' ? slot.value : '');
    const [num, setNum] = useState(typeof slot.value === 'number' ? String(slot.value) : '');
    const fields = (idKind ? OPERAND_FIELDS[idKind] : kind === 'number' ? ['quantity', 'coins'] : kind === 'text' ? ['actor', 'target', 'cell', 'item', 'detail'] : kind === 'switch' ? []
        : ['actor', 'target', 'cell', 'item', 'detail', 'quantity', 'coins']).filter(f => !offer || f === 'actor');
    const n = Number(num);
    return <div className="wb-operand">
        <p className="hint">Now: {operandWords(slot.value as Operand, {}) || 'nothing'}</p>
        {(!kind || kind === 'number') && <form className="wb-enter" onSubmit={e => { e.preventDefault(); if (num !== '' && Number.isFinite(n)) pick(n); }}>
            <label>A number<input autoFocus type="number" step="any" value={num} onChange={e => setNum(e.target.value)}/></label><button type="submit" disabled={num === '' || !Number.isFinite(n)}>Use</button></form>}
        {(!kind || kind === 'switch') && <div className="wb-chips"><span>On or off:</span><button type="button" onClick={() => pick(true)}>on</button><button type="button" onClick={() => pick(false)}>off</button></div>}
        {(!kind || kind === 'text') && <form className="wb-enter" onSubmit={e => { e.preventDefault(); pick(text); }}>
            <label>A text<input value={text} maxLength={200} onChange={e => setText(e.target.value)}/></label><button type="submit">Use</button></form>}
        {vars.length > 0 && <div className="wb-chips"><span>A variable:</span>{vars.map(v => <button key={v.id} type="button" onClick={() => pick({var: v.id})}>{v.id}</button>)}
            {vars.some(v => v.scope === 'player' || v.scope === 'party') && <small className="hint">(a wolf's own: the acting wolf's)</small>}</div>}
        {fields.length > 0 && <div className="wb-chips"><span>{offer ? 'The wolf:' : "The event's:"}</span>{fields.map(f => <button key={f} type="button" onClick={() => pick({field: f})}>{offer ? 'the one it is offered to' : f}</button>)}</div>}
        {(!kind || kind === 'number') && lists.length > 0 && <div className="wb-chips"><span>How many in:</span>{lists.map(v => <button key={v.id} type="button" onClick={() => pick({count: v.id})}>{v.id}</button>)}</div>}
        {(!kind || kind === 'number') && timers.length > 0 && <div className="wb-chips"><span>Days since:</span>{timers.map(v => <button key={v.id} type="button" onClick={() => pick({since: v.id})}>{v.id}</button>)}</div>}
        {(!kind || kind === 'text') && !offer && <div className="wb-chips"><span>Inside For each:</span><button type="button" onClick={() => pick({each: true})}>the looped value</button></div>}
        {(!kind || kind === 'number') && isV2(program) && <div className="wb-chips"><span>The story's wolves (doc 86):</span>
            {(['all', 'active', 'online'] as const).filter(a => !offer || a !== 'online').map(a => <button key={a} type="button" onClick={() => pick({audience: a})}>how many {a === 'all' ? 'it has' : `are ${a}`}</button>)}
            {num !== '' && Number.isFinite(n) && <button type="button" onClick={() => pick({max: [n, {times: [{audience: 'active'}, 2]}]})}>{n}, or 2 for each active wolf</button>}</div>}
        {(!kind || kind === 'number') && isV2(program) && <div className="wb-chips"><span>Over every wolf (doc 86, tally):</span>
            <button type="button" onClick={() => pick({tally: {}})}>how many wolves the story has</button>
            {(program.variables ?? []).filter(v => v.type === 'number' && v.scope === 'player' && !v.mirror).map(v =>
                <button key={v.id} type="button" onClick={() => pick({tally: {sum: v.id}})}>every wolf's {v.id}, added up</button>)}
            <small className="hint">(only some wolves, or over a list: edited in Source)</small></div>}
        {(!kind || kind === 'number') && !offer && (program.quests ?? []).some(q => q.credit === 'share') && <div className="wb-chips"><span>In a reward by share:</span>
            <button type="button" onClick={() => pick({share: true})}>the wolf's share</button></div>}
        {(!kind || kind === 'number') && wolfLists.length > 0 && <div className="wb-chips"><span>Of the active wolves:</span>
            {wolfLists.map(v => <span key={v.id}><button type="button" onClick={() => pick({percent: {list: v.id}, of: 'active'})}>the share in {v.id}</button>
                <button type="button" onClick={() => pick({among: {list: v.id}, of: 'active'})}>how many in {v.id}</button></span>)}
            <small className="hint">(the share of the wolves who…, how many of the wolves…: a test for each wolf is edited in Source)</small></div>}
        {(!kind || kind === 'number') && vars.some(v => v.type === 'number') && num !== '' && Number.isFinite(n) && <div className="wb-chips"><span>Or a number variable and {n}:</span>
            {vars.filter(v => v.type === 'number').map(v => <span key={v.id}><button type="button" onClick={() => pick({plus: [{var: v.id}, n]})}>{v.id} + {n}</button>
                <button type="button" onClick={() => pick({minus: [{var: v.id}, n]})}>{v.id} − {n}</button>
                <button type="button" onClick={() => pick({max: [{var: v.id}, n]})}>larger of {v.id}, {n}</button></span>)}</div>}
    </div>;
}

/** Version 2: a trigger of this story (Run, Enable, Disable, "has fired"), by phase. */
function TriggerPicker({slot, pick}: {slot: Slot; pick: (v: string) => void}) {
    const {program} = useBench();
    const options = [...(program.triggers ?? []).map(t => ({id: t.id, name: t.id, group: 'Campaign'})),
        ...program.phases.flatMap(ph => (ph.triggers ?? []).map(t => ({id: t.id, name: t.id, group: 'Phase ' + ph.id, hint: t.note})))];
    return <SearchList options={options} value={slot.value} pick={pick} placeholder="Find a trigger"/>;
}

/** Phase 6: the story choices (npc.offer) a program names, and those its triggers listen for. */
export function offersOf(p: Program): string[] {
    const out = new Set<string>();
    const triggers = [...(p.triggers ?? []), ...p.phases.flatMap(ph => ph.triggers ?? [])];
    for (const t of triggers) {
        if (t.on === 'dialogue choice' && t.match?.detail) out.add(t.match.detail);
        for (const {a} of allActions(t.actions)) if (a.do === 'effect' && a.effect.kind === 'npc.offer' && typeof a.effect.id === 'string' && a.effect.id) out.add(a.effect.id);
    }
    return [...out].sort();
}

/** The picker for one slot of a sentence. `pick` gets the new values of the slot's keys (undefined: clear a filter). */
export function Picker({slot, pick, close, clearable, cues}: {slot: Slot; pick: (v: Values) => void; close: () => void; clearable?: boolean; cues?: string[]}) {
    const {program} = useBench();
    const key = slot.keys[0];
    const one = (v: unknown) => pick({[key]: v});
    const type = slot.type;
    const options = useOptions(type === 'wolf' ? 'wolf' : type);
    let body: React.ReactNode;
    if (type === 'tile') body = <TilePicker value={(slot.value ?? {}) as Values} pick={pick}/>;
    else if (type === 'variable') body = <VariablePicker slot={slot} pick={one}/>;
    else if (type === 'operand') body = <OperandPicker slot={slot} pick={one}/>;
    else if (type === 'trigger') body = <TriggerPicker slot={slot} pick={one}/>;
    // (Doc 86, R1: a test for each wolf, the wolf as $each; the world's facts are greyed out, it is counted outside an event.)
    else if (type === 'condition') body = <div className="wb-each-test"><p className="hint">Tested once for each wolf: read a wolf's own values "of the looped wolf".</p>
        <ConditionNode node={(slot.value as Condition | undefined) ?? {test: 'compare', left: {var: (program.variables ?? []).find(v => v.scope === 'player' && v.type === 'number')?.id ?? 'progress', of: '$each'}, op: '>=', right: 1}}
            change={c => one(c)} depth={1} phase={null} each/></div>;
    else if (type === 'whose') body = <div className="wb-chips">{(slot.param?.values ?? []).map(v => <button key={v} type="button" aria-pressed={(slot.value ?? '$actor') === v}
        onClick={() => one(v === '$actor' ? undefined : v)}>{slot.param?.words?.[v] ?? v}</button>)}</div>;
    else if (type === 'choice' && slot.param?.values && slot.param.optional) body = <div className="wb-chips"><button type="button" aria-pressed={slot.value === undefined}
        onClick={() => one(undefined)}>{slot.param.words?.[''] ?? 'none'}</button>{slot.param.values.map(v => <button key={v} type="button"
        aria-pressed={slot.value === v} onClick={() => one(v)}>{v}</button>)}</div>;
    else if (type === 'choice' && slot.param?.values) body = <div className="wb-chips">{slot.param.values.map(v => <button key={v} type="button"
        aria-pressed={slot.value === v} onClick={() => one(v)}>{slot.param?.words?.[v] ?? v.replaceAll('_', ' ')}</button>)}</div>;
    else if (type === 'bool') body = <div className="wb-chips">{[true, false].map(v => <button key={String(v)} type="button" aria-pressed={slot.value === v || (!v && !slot.value)}
        // (A switch's "is" must say off as false; elsewhere "no" is the field left out.)
        onClick={() => one(v ? true : slot.param?.key === 'is' ? false : undefined)}>{slot.param?.words?.[String(v)] || (v ? 'Yes' : 'No')}</button>)}</div>;
    else if (type === 'int' || type === 'days' || type === 'number') body = <NumberPicker param={slot.param} value={slot.value} pick={one} step={type === 'days' ? 'any' : undefined}/>;
    else if (type === 'value') body = <><div className="wb-chips">{(slot.param?.or ?? []).map(v => <button key={v} type="button" aria-pressed={slot.value === v}
        onClick={() => one(v)}>{slot.param?.words?.[v] ?? v}</button>)}</div><NumberPicker param={slot.param} value={slot.value} pick={one}/></>;
    else if (type === 'text') body = <TextPicker param={slot.param} value={slot.value} pick={one}/>;
    else if (type === 'signal') body = <NamePicker names={signalsOf(program)} value={slot.value} pick={one} noun="signal"/>;
    else if (type === 'counter') body = <NamePicker names={[...new Set(countersOf(program).map(c => c.key))]} value={slot.value} pick={one} noun="counter"/>;
    else if (type === 'cue') body = <NamePicker names={cues ?? []} value={slot.value} pick={one} noun="cue"/>;
    else if (type === 'offer') body = <NamePicker names={offersOf(program)} value={slot.value} pick={one} noun="story choice"/>;
    // (Doc 86, Phase 1b: a talk memory's key and value, small letters, digits and _; a figure of the realm's id.)
    else if (type === 'talkkey') body = <NamePicker names={talkKeysOf(program)} value={slot.value} pick={v => one(v.toLowerCase())} noun="memory key (nh_stage)"/>;
    else if (type === 'talkvalue') body = <NamePicker names={talkValuesOf(program)} value={slot.value} pick={v => one(v.toLowerCase())} noun="value (reckoning)"/>;
    else if (type === 'figure') body = <NamePicker names={figuresOf(program)} value={slot.value} pick={v => one(v.toLowerCase())} noun="figure's id"/>;
    // (Phase 9: a stranger the story brings, by the id it gives them.)
    else if (type === 'visitor') body = <NamePicker names={visitorsOf(program)} value={slot.value} pick={one} noun="stranger's id"/>;
    else if (type === 'objective') {
        const q = (program.quests ?? []).find(x => x.id === slot.param?.quest);
        body = q ? <SearchList options={q.objectives.map(o => ({id: o.id, name: o.label, hint: (o.optional ? 'optional' : '') + (o.hidden ? ' hidden' : '')}))}
            value={slot.value} pick={one} placeholder="Find an objective"/> : <p className="hint">Choose the quest first.</p>;
    }
    else if (type === 'missions') {
        const chosen = new Set(Array.isArray(slot.value) ? slot.value as string[] : []);
        body = <div className="wb-options">{options.map(o => <label key={o.id}><input type="checkbox" checked={chosen.has(o.id)} onChange={e => {
            const next = new Set(chosen); if (e.target.checked) next.add(o.id); else next.delete(o.id); pick({missions: [...next]});
        }}/>{o.name}</label>)}{!options.length && <p className="hint">This phase has no missions.</p>}</div>;
    } else {
        const extra = type === 'wolf' ? <div className="wb-chips"><button type="button" aria-pressed={slot.value === '$actor'} onClick={() => one('$actor')}>the acting wolf</button>
            {isV2(program) && <button type="button" aria-pressed={slot.value === '$each'} onClick={() => one('$each')} title="Inside For each">the looped wolf</button>}
            {slot.param?.world && <button type="button" aria-pressed={slot.value === 'world'} onClick={() => one('world')}>everyone (a public scene)</button>}</div>
            : slot.param?.all ? <div className="wb-chips"><button type="button" aria-pressed={slot.value === 'all'} onClick={() => one('all')}>every market</button></div> : undefined;
        const here = slot.param?.optional ? <div className="wb-chips"><button type="button" aria-pressed={slot.value === undefined} onClick={() => one(undefined)}>{slot.param.words?.[''] ?? 'none'}</button></div>
            // (Doc 85, Phase 8: a place that may be where the acting wolf is: "the weather where the acting wolf is".)
            : slot.param?.or?.length ? <div className="wb-chips">{slot.param.or.map(v => <button key={v} type="button" aria-pressed={(slot.value ?? slot.param?.default) === v}
                onClick={() => one(v === slot.param?.default ? undefined : v)}>{slot.param?.words?.[v] ?? v}</button>)}</div> : undefined;
        body = <><SearchList options={options} value={slot.value} pick={one} placeholder={'Find ' + (slot.label || type)} extra={extra && here ? <>{extra}{here}</> : extra ?? here}/>
            {!['phase', 'region', 'scene', 'mission', 'quest'].includes(type) && <EnterId value={slot.value} pick={one}
                label={type === 'purse' ? 'Or a purse: town:<town>, till:job:<resident>, chapter:<id>' : undefined}/>}</>;
    }
    // (Doc 86, C1a: a parameter marked operand may be a value worked out as the trigger fires: a variable of its kind, the
    // event's field, the loop's value, arithmetic.)
    const valued = !!slot.param?.operand && isV2(program) && type !== 'operand';
    return <div className="wb-picker" role="dialog" aria-label={'Choose ' + (slot.label || type)} onKeyDown={e => { if (e.key === 'Escape') { e.stopPropagation(); close(); } }}>
        <header><strong>Choose {slot.label || type}</strong>{clearable && slot.value !== undefined && <button type="button" onClick={() => pick({[key]: undefined})}>Any (no filter)</button>}
            <button type="button" className="wb-close" onClick={close} aria-label="Close">×</button></header>
        {valued && slot.value !== null && typeof slot.value === 'object' && <p className="hint">Now a value: {operandWords(slot.value as Operand, {})}</p>}
        {body}
        {valued && <details className="wb-or-value"><summary>…or a value</summary>
            <OperandPicker slot={{...slot, param: {...slot.param!, kind: slot.param!.type === 'int' ? 'number' : 'text',
                idKind: slot.param!.type === 'int' ? undefined : slot.param!.type}}} pick={one}/></details>}</div>;
}

/** "Choose an event": the catalogue's events by group, each with its help and what its fields hold (4.3). */
export function EventPicker({value, pick, close, exits = false, filter}: {value?: string; pick: (id: string) => void; close: () => void; exits?: boolean;
    filter?: (e: CatEvent) => boolean}) {
    const {cat} = useBench();
    const [q, setQ] = useState('');
    const [open, setOpen] = useState(value ?? '');
    const words = q.toLowerCase().split(/\s+/).filter(Boolean);
    const list = cat.events.filter(e => (exits ? e.special !== 'signal' : !e.exitOnly) && (!filter || filter(e))
        && words.every(w => (e.id + ' ' + e.title + ' ' + e.help + ' ' + Object.values(e.fields).map(f => f.label).join(' ')).toLowerCase().includes(w)));
    return <div className="wb-picker wb-events" role="dialog" aria-label="Choose an event">
        <header><strong>Choose an event</strong><button type="button" className="wb-close" onClick={close} aria-label="Close">×</button></header>
        <input type="search" autoFocus placeholder="Find an event (gift, region, festival…)" value={q} onChange={e => setQ(e.target.value)} aria-label="Find an event"/>
        <div className="wb-options">{cat.groups.map(g => {
            const here = list.filter(e => e.group === g.id);
            return here.length ? <div key={g.id}><h6>{g.label}</h6>{here.map(e => <div key={e.id} className={'wb-event' + (open === e.id ? ' open' : '')}>
                <button type="button" aria-pressed={e.id === value} onClick={() => pick(e.id)} onMouseEnter={() => setOpen(e.id)} onFocus={() => setOpen(e.id)}>
                    <span>⚑ {e.title}</span><code>{e.id}</code></button>
                {open === e.id && <div className="wb-event-help"><p>{e.help}</p>
                    {Object.keys(e.fields).length > 0 && <dl>{Object.entries(e.fields).map(([k, f]) => <div key={k}><dt>{k}</dt><dd>{f.label}</dd></div>)}</dl>}
                    {e.tile && <small>Carries the tile where it happened.</small>}{e.notes && <small> {e.notes}</small>}</div>}</div>)}</div> : null;
        })}{!list.length && <p className="hint">No event matches.</p>}</div></div>;
}

/** "Add an action": counters, signals, the flow, and every world effect, by group, each with its help. */
export function ActionPicker({pick, close, entry = false, phaseOnly, rewards = false}: {pick: (kind: string) => void; close: () => void; entry?: boolean; phaseOnly?: boolean;
    /** A quest's rewards (Phase 6): the world effects only, which the game is sent for each wolf with credit. */
    rewards?: boolean}) {
    const {cat, program} = useBench();
    const [q, setQ] = useState('');
    const words = q.toLowerCase().split(/\s+/).filter(Boolean);
    const match = (i: CatItem) => words.every(w => (i.id + ' ' + i.title + ' ' + i.help).toLowerCase().includes(w));
    const v2 = isV2(program);
    const actions = entry || rewards ? [] : cat.actions.filter(a => a.id !== 'effect' && (v2 ? !a.v1 : !a.v2) && match(a));
    // (Phase 6: a version 2 story's own effects (quests, who takes part, story choices) are a trigger's actions, never a phase's entry
    // effects or a reward.)
    const effects = cat.effects.filter(e => (entry || !e.entryOnly) && (v2 || !e.v2) && !((entry || rewards) && e.story) && match(e));
    void phaseOnly;
    return <div className="wb-picker wb-events" role="dialog" aria-label={entry ? 'Add an effect' : 'Add an action'}>
        <header><strong>{rewards ? 'Add a reward' : entry ? 'When this phase opens…' : 'Add an action'}</strong><button type="button" className="wb-close" onClick={close} aria-label="Close">×</button></header>
        <input type="search" autoFocus placeholder="Find an action (signal, deed, walk…)" value={q} onChange={e => setQ(e.target.value)} aria-label="Find an action"/>
        <div className="wb-options">{cat.actionGroups.map(g => {
            const here = [...actions.filter(a => a.group === g.id).map(a => ({id: a.id, item: a})), ...effects.filter(e => e.group === g.id).map(e => ({id: 'effect:' + e.id, item: e}))];
            return here.length ? <div key={g.id}><h6>{g.label}</h6>{here.map(({id, item}) => <button key={id} type="button" className="wb-action-option" onClick={() => pick(id)}
                title={item.help}><span>{item.title}</span><small>{item.help}</small></button>)}</div> : null;
        })}</div></div>;
}
