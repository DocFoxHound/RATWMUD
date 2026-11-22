// The pickers of the trigger workbench (Docs/Design/85, 4.3): one per kind of parameter, so nobody types an id. Residents
// and wolves by name, places and tiles on the map, items from the catalogue by category, factions, towns, markets, ships,
// harvest areas, templates and projects from the world, and the story's own phases, regions, missions, scenes, signals
// and counters. "Enter an id" stays, for what the world does not list (or a world the refs could not be read from).
import {useMemo, useState} from 'react';
import {MapView} from '../MapView';
import {surfaceFor} from '../../lib/surface';
import {countersOf, REF_KIND, signalsOf, type CatEvent, type CatItem, type Param, type Slot} from '../../lib/workbench';
import {useBench} from './context';

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
    const [text, setText] = useState(typeof value === 'string' ? value : '');
    const max = param?.max ?? 400;
    return <form className="wb-enter wb-text" onSubmit={e => { e.preventDefault(); if (text.trim() && text.length <= max) pick(text); }}>
        <label>{param?.label ?? 'Words'}<textarea autoFocus rows={2} maxLength={max} value={text} onChange={e => setText(e.target.value)}/>
            <small className={text.length > max * 0.9 ? 'warn' : ''}>{text.length} / {max}</small></label>
        <button type="submit" disabled={!text.trim()}>Use</button></form>;
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

/** The picker for one slot of a sentence. `pick` gets the new values of the slot's keys (undefined: clear a filter). */
export function Picker({slot, pick, close, clearable, cues}: {slot: Slot; pick: (v: Values) => void; close: () => void; clearable?: boolean; cues?: string[]}) {
    const {program} = useBench();
    const key = slot.keys[0];
    const one = (v: unknown) => pick({[key]: v});
    const type = slot.type;
    const options = useOptions(type === 'wolf' ? 'wolf' : type);
    let body: React.ReactNode;
    if (type === 'tile') body = <TilePicker value={(slot.value ?? {}) as Values} pick={pick}/>;
    else if (type === 'choice' && slot.param?.values) body = <div className="wb-chips">{slot.param.values.map(v => <button key={v} type="button"
        aria-pressed={slot.value === v} onClick={() => one(v)}>{slot.param?.words?.[v] ?? v.replaceAll('_', ' ')}</button>)}</div>;
    else if (type === 'bool') body = <div className="wb-chips">{[true, false].map(v => <button key={String(v)} type="button" aria-pressed={slot.value === v || (!v && !slot.value)}
        onClick={() => one(v ? true : undefined)}>{v ? 'Yes' : 'No'}</button>)}</div>;
    else if (type === 'int' || type === 'days' || type === 'number') body = <NumberPicker param={slot.param} value={slot.value} pick={one} step={type === 'days' ? 'any' : undefined}/>;
    else if (type === 'value') body = <><div className="wb-chips">{(slot.param?.or ?? []).map(v => <button key={v} type="button" aria-pressed={slot.value === v}
        onClick={() => one(v)}>{slot.param?.words?.[v] ?? v}</button>)}</div><NumberPicker param={slot.param} value={slot.value} pick={one}/></>;
    else if (type === 'text') body = <TextPicker param={slot.param} value={slot.value} pick={one}/>;
    else if (type === 'signal') body = <NamePicker names={signalsOf(program)} value={slot.value} pick={one} noun="signal"/>;
    else if (type === 'counter') body = <NamePicker names={[...new Set(countersOf(program).map(c => c.key))]} value={slot.value} pick={one} noun="counter"/>;
    else if (type === 'cue') body = <NamePicker names={cues ?? []} value={slot.value} pick={one} noun="cue"/>;
    else if (type === 'missions') {
        const chosen = new Set(Array.isArray(slot.value) ? slot.value as string[] : []);
        body = <div className="wb-options">{options.map(o => <label key={o.id}><input type="checkbox" checked={chosen.has(o.id)} onChange={e => {
            const next = new Set(chosen); if (e.target.checked) next.add(o.id); else next.delete(o.id); pick({missions: [...next]});
        }}/>{o.name}</label>)}{!options.length && <p className="hint">This phase has no missions.</p>}</div>;
    } else {
        const extra = type === 'wolf' ? <div className="wb-chips"><button type="button" aria-pressed={slot.value === '$actor'} onClick={() => one('$actor')}>the acting wolf</button>
            {slot.param?.world && <button type="button" aria-pressed={slot.value === 'world'} onClick={() => one('world')}>everyone (a public scene)</button>}</div> : undefined;
        body = <><SearchList options={options} value={slot.value} pick={one} placeholder={'Find ' + (slot.label || type)} extra={extra}/>
            {!['phase', 'region', 'scene', 'mission'].includes(type) && <EnterId value={slot.value} pick={one}/>}</>;
    }
    return <div className="wb-picker" role="dialog" aria-label={'Choose ' + (slot.label || type)} onKeyDown={e => { if (e.key === 'Escape') { e.stopPropagation(); close(); } }}>
        <header><strong>Choose {slot.label || type}</strong>{clearable && slot.value !== undefined && <button type="button" onClick={() => pick({[key]: undefined})}>Any (no filter)</button>}
            <button type="button" className="wb-close" onClick={close} aria-label="Close">×</button></header>
        {body}</div>;
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
export function ActionPicker({pick, close, entry = false, phaseOnly}: {pick: (kind: string) => void; close: () => void; entry?: boolean; phaseOnly?: boolean}) {
    const {cat} = useBench();
    const [q, setQ] = useState('');
    const words = q.toLowerCase().split(/\s+/).filter(Boolean);
    const match = (i: CatItem) => words.every(w => (i.id + ' ' + i.title + ' ' + i.help).toLowerCase().includes(w));
    const actions = entry ? [] : cat.actions.filter(a => a.id !== 'effect' && match(a));
    const effects = cat.effects.filter(e => (entry || !e.entryOnly) && match(e));
    void phaseOnly;
    return <div className="wb-picker wb-events" role="dialog" aria-label={entry ? 'Add an effect' : 'Add an action'}>
        <header><strong>{entry ? 'When this phase opens…' : 'Add an action'}</strong><button type="button" className="wb-close" onClick={close} aria-label="Close">×</button></header>
        <input type="search" autoFocus placeholder="Find an action (signal, deed, walk…)" value={q} onChange={e => setQ(e.target.value)} aria-label="Find an action"/>
        <div className="wb-options">{cat.actionGroups.map(g => {
            const here = [...actions.filter(a => a.group === g.id).map(a => ({id: a.id, item: a})), ...effects.filter(e => e.group === g.id).map(e => ({id: 'effect:' + e.id, item: e}))];
            return here.length ? <div key={g.id}><h6>{g.label}</h6>{here.map(({id, item}) => <button key={id} type="button" className="wb-action-option" onClick={() => pick(id)}
                title={item.help}><span>{item.title}</span><small>{item.help}</small></button>)}</div> : null;
        })}</div></div>;
}
