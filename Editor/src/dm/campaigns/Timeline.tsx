// A campaign's timeline (doc 85, 5.3): phase visits, triggers fired and orders with their receipts, newest first, with
// who, where and when; [Show] opens the world event behind a firing.
import {useState} from 'react';
import {ago, causeText, filterTimeline, nameOf, type TimelineFilter, type TimelineItem, type WorldEvent} from '../../lib/campaigns';

type Place = {cell: string; x: number; y: number};
const FILTERS: {id: TimelineFilter; label: string}[] = [{id: 'all', label: 'All'}, {id: 'phase', label: 'Phases'}, {id: 'trigger', label: 'Triggers'}, {id: 'order', label: 'Orders'}];
const when = (i: TimelineItem, now: string) => <span className="camp-when">{'day' in i && typeof i.day === 'number' ? `day ${i.day.toFixed(1)} · ` : ''}{i.at ? `${new Date(i.at).toLocaleTimeString([], {hour: '2-digit', minute: '2-digit'})} · ${ago(i.at, now)}` : ''}</span>;
const placeText = (p: {cell?: string; x?: number | null; y?: number | null}, names: Record<string, string>) =>
    p.cell ? `${nameOf(names, p.cell)}${p.x != null && p.y != null ? ` (${p.x}, ${p.y})` : ''}` : '';

export function TimelineRow({item: i, names, events, now, onMap}: {item: TimelineItem; names: Record<string, string>; events: Record<string, WorldEvent>; now: string;
    onMap?: (p: Place) => void}) {
    const [open, setOpen] = useState(false);
    if (i.type === 'phase') return <li className="phase">{when(i, now)}<span>▶ entered <b>{i.phase}</b> (visit {i.visit}) ← {causeText(i.cause, names)}</span>
        {!!i.missions?.length && <small>{i.missions.map(m => `${m.title}: ${m.ready ? 'done' : `${m.completed} / ${m.required}`}`).join(' · ')}</small>}</li>;
    if (i.type === 'order') return <li className={`order ${i.status}`}>{when(i, now)}
        <span>→ order #{i.id} <b>{i.kind}</b>{i.target && !i.target.startsWith('world-') ? ' → ' + nameOf(names, i.target) : ''} · <span className={`camp-order ${i.status}`}>{i.status}</span>
            {i.doneAt && i.status !== 'queued' ? ` ${new Date(i.doneAt).toLocaleTimeString()}` : ''}</span>
        {i.result && <small>{i.result}</small>}</li>;
    const event = i.event != null ? events[String(i.event)] : undefined;
    const place = i.cell ? {cell: i.cell, x: i.x ?? 0, y: i.y ?? 0} : event?.cell ? {cell: event.cell, x: event.x ?? 0, y: event.y ?? 0} : null;
    return <li className="trigger">{when(i, now)}
        <span>⚑ <b>{i.trigger}</b> {i.scope === 'campaign' ? '(campaign) ' : ''}in {i.phase}{i.actor ? ` · ${nameOf(names, i.actor)}` : ''}
            {place ? ` · ${placeText(place, names)}` : ''} ← {i.signal ? `signal ${i.signal}` : i.on}</span>
        <small>{i.actions.join(' → ')}{event && <> · event #{event.id} <button className="link-button" onClick={() => setOpen(o => !o)}>{open ? 'Hide' : 'Show'}</button></>}
            {place && onMap && <> · <button className="link-button" onClick={() => onMap(place)}>On the map</button></>}</small>
        {open && event && <dl className="camp-event">
            <dt>kind</dt><dd>{event.kind}</dd>
            {event.actor && <><dt>actor</dt><dd>{nameOf(names, event.actor)}</dd></>}
            {event.target && <><dt>target</dt><dd>{nameOf(names, event.target)}</dd></>}
            {event.cell && <><dt>where</dt><dd>{placeText(event, names)}</dd></>}
            {event.item && <><dt>item</dt><dd>{event.item}</dd></>}
            {!!event.quantity && <><dt>quantity</dt><dd>{event.quantity}</dd></>}
            {!!event.coins && <><dt>coins</dt><dd>{event.coins}</dd></>}
            {event.detail && <><dt>detail</dt><dd>{event.detail}</dd></>}
            <dt>when</dt><dd>day {event.day.toFixed(2)} · {new Date(event.at).toLocaleString()}</dd>
        </dl>}</li>;
}

export function Timeline({items, events, names, now, earlier, onMap, triggers, cohort}: {items: TimelineItem[]; events: Record<string, WorldEvent>;
    names: Record<string, string>; now: string; earlier?: () => void; onMap: (p: Place) => void; triggers: string[]; cohort: string[]}) {
    const [kind, setKind] = useState<TimelineFilter>('all'), [wolf, setWolf] = useState(''), [trigger, setTrigger] = useState('');
    const shown = filterTimeline(items, kind, wolf, trigger);
    return <div className="camp-timeline-tab">
        <div className="camp-filters">
            {FILTERS.map(f => <button key={f.id} className={kind === f.id ? 'on' : ''} onClick={() => setKind(f.id)}>{f.label}</button>)}
            <label>wolf <select value={wolf} onChange={e => setWolf(e.target.value)}><option value="">any</option>
                {[...cohort].sort((a, b) => nameOf(names, a).localeCompare(nameOf(names, b))).map(id => <option key={id} value={id}>{nameOf(names, id)}</option>)}</select></label>
            <label>trigger <select value={trigger} onChange={e => setTrigger(e.target.value)}><option value="">any</option>
                {triggers.sort().map(t => <option key={t}>{t}</option>)}</select></label>
        </div>
        {shown.length ? <ol className="camp-timeline">{shown.map(i => <TimelineRow key={i.key} item={i} names={names} events={events} now={now} onMap={onMap} />)}</ol>
            : <p className="hint">Nothing {items.length ? 'matches' : 'has happened yet'}.</p>}
        {earlier && <button onClick={earlier}>Earlier…</button>}
    </div>;
}
