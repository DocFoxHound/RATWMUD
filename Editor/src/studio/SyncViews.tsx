// What Atlas's Update from live and the DM's Import from dev both show (doc 66, section 4): changes grouped by table,
// and the conflicts side by side (base, dev, live), field by field, with a choice for each (lib/sync.ts builds the
// engine's choices from them).
import {useState} from 'react';
import {buildChoices, defaultPicks, differingFields, fieldSide, goneText, groupByLabel, middleOf, placeText, rowChangeText, sideChangeText,
    summaryRows, valueText, type AreaRef, type GonePlace, type PlaceChoice, type Picks, type Side, type Summary, type SyncItem} from '../lib/sync';

const SHOWN = 200;              // Rows listed per table before "…and N more" (DEV's terrain alone has thousands).

/** A value cell: long values (terrain glyphs) by their size and digest, others in full on hover. */
function Value({value}: {value: unknown}) {
    const text = valueText(value);
    return <span className="sync-value" title={text}>{text}</span>;
}

/** Changes one side made, grouped by table label, each group collapsible; a row opens to its fields. */
export function ChangeGroups({items, side, empty}: {items: SyncItem[]; side: Side; empty: string}) {
    if (!items.length) return <p className="meta">{empty}</p>;
    return <div className="sync-groups">{groupByLabel(items).map(group => <details key={group.label} className="sync-group">
        <summary><b>{group.label}</b> <span className="meta">{group.items.length} row{group.items.length === 1 ? '' : 's'} · {group.part === 'world' ? 'the map' : 'live data'}</span></summary>
        <ul>{group.items.slice(0, SHOWN).map(item => <li key={item.id}>
            <details><summary>{item.name} <span className="meta">{sideChangeText(item, side)}</span></summary>
                <FieldTable item={item} />
            </details></li>)}
            {group.items.length > SHOWN && <li className="meta">…and {group.items.length - SHOWN} more</li>}</ul>
    </details>)}</div>;
}

function FieldTable({item, pick, whole, onField, labels}: {item: SyncItem; pick?: Picks[string]; whole?: Side; onField?: (field: string, side: Side) => void;
        labels?: Record<Side, string>}) {
    if (!item.fields.length) return <p className="meta">No fields differ.</p>;
    return <table className="dm-table sync-fields"><thead><tr><th>Field</th><th>Base</th><th>Dev</th><th>Live</th>
        <th>{onField ? 'Keep' : 'Changed by'}</th></tr></thead>
        <tbody>{item.fields.map(f => {
            const side = whole ?? fieldSide(f, pick);
            const differs = f.changed !== 'same';
            return <tr key={f.field} className={f.changed === 'conflict' ? 'sync-both' : ''}>
                <th>{f.field}{f.owner === 'game' && <span className="sync-owner" title="The live game's: its value live is the truth, kept unless you choose dev's">game</span>}</th>
                <td><Value value={f.base} /></td>
                <td className={side === 'dev' ? 'sync-kept' : ''}><Value value={f.dev} /></td>
                <td className={side === 'live' ? 'sync-kept' : ''}><Value value={f.live} /></td>
                <td>{onField && differs && labels ? <div className="segmented">{(['dev', 'live'] as const).map(s =>
                    <button key={s} className={side === s ? 'on' : ''} aria-pressed={side === s} onClick={() => onField(f.field, s)}
                        title={f.changed === 'conflict' ? 'Both changed this field' : f.changed === s ? `Only ${s} changed it` : ''}>{labels[s]}</button>)}</div>
                    : <span className="meta">{f.changed === 'conflict' ? 'both' : f.changed === 'same' ? 'same now' : f.changed ?? '—'}</span>}</td>
            </tr>;
        })}</tbody></table>;
}

/** A place the new map hasn't, and its decision: dev's place (when dev has one), unset, or a new place on the new map
 *  (one of its cells or interiors, at a tile). */
function PlaceDecision({gone, choice, areas, onChoose, disabled}: {gone: GonePlace; choice: PlaceChoice | undefined; areas: AreaRef[];
        onChoose: (choice: PlaceChoice) => void; disabled: boolean}) {
    const picked = typeof choice === 'object' ? choice : null;
    const area = picked ? areas.find(a => a.id === picked.cell) : undefined;
    return <div className="sync-place">
        <span><b>{gone.place}</b> <span className="meta">in {gone.area ?? '?'}, not on the new map</span></span>
        <div className="segmented small" role="radiogroup" aria-label={`The ${gone.place}`}>
            {gone.dev && <button role="radio" disabled={disabled} aria-checked={choice === 'dev'} className={choice === 'dev' ? 'on' : ''}
                title={`Dev's: ${placeText(gone.dev)}`} onClick={() => onChoose('dev')}>Dev's ({placeText(gone.dev)})</button>}
            <button role="radio" disabled={disabled} aria-checked={choice === 'unset'} className={choice === 'unset' ? 'on' : ''}
                title={gone.place === 'home' ? 'Unplaced: kept, with its name, talk and memories, standing nowhere' : `No ${gone.place} place`}
                onClick={() => onChoose('unset')}>Unset</button>
            <button role="radio" disabled={disabled || !areas.length} aria-checked={!!picked} className={picked ? 'on' : ''}
                onClick={() => onChoose(picked ?? middleOf(areas[0]))}>New place</button>
        </div>
        {picked && <span className="sync-new-place">
            <select value={picked.cell} disabled={disabled} aria-label="The new map's place"
                onChange={e => { const a = areas.find(x => x.id === e.target.value); if (a) onChoose(middleOf(a)); }}>
                {areas.map(a => <option key={a.id} value={a.id}>{a.name} ({a.kind}{a.width ? `, ${a.width}×${a.height}` : ''})</option>)}
            </select>
            {(['x', 'y'] as const).map(k => <label key={k}>{k} <input type="number" min={0} max={(k === 'x' ? area?.width : area?.height) ?? undefined}
                value={picked[k]} disabled={disabled} onChange={e => onChoose({...picked, [k]: Math.max(0, Math.floor(Number(e.target.value) || 0))})} /></label>)}
        </span>}
    </div>;
}

/** The conflicts, each with its choice: the whole row from dev or live, or (rows both sides have) field by field.
 *  `labels` name the choices for the screen ("Keep dev's" / "Take live's" in Atlas; "Import dev's" / "Keep live's" in
 *  the DM). A conflict over the live game's fields starts on live's (its default, the game's truth: lib/sync.ts
 *  defaultPicks); an NPC whose place a new map hasn't (`gone`, the import's `areas` to pick a new one from) needs a
 *  decision for each such place. */
export function ConflictList({conflicts, picks, onPicks, labels, disabled = false, areas = []}: {conflicts: SyncItem[]; picks: Picks;
        onPicks: (picks: Picks) => void; labels: Record<Side, string>; disabled?: boolean; areas?: AreaRef[]}) {
    const [open, setOpen] = useState<Record<string, boolean>>({});
    if (!conflicts.length) return <p className="meta">No conflicts: nothing was changed on both sides.</p>;
    const set = (id: string, pick: Picks[string]) => onPicks({...picks, [id]: pick});
    const homeless = conflicts.filter(c => c.gone?.length);
    // (Live's place gone, or dev without the row: those keep their own decision.)
    const all = (side: Side) => onPicks({...picks, ...Object.fromEntries(conflicts
        .filter(c => !c.gone?.length || (side === 'dev' && c.dev !== 'absent')).map(c => [c.id, {whole: side}]))});
    return <div className="sync-conflicts">
        <div className="sync-bulk"><span className="meta">All conflicts:</span>
            <button disabled={disabled} onClick={() => all('dev')}>{labels.dev}</button>
            <button disabled={disabled} onClick={() => all('live')}>{labels.live}</button>
            <button disabled={disabled} onClick={() => onPicks({...picks, ...defaultPicks(conflicts)})}
                title="Each conflict over the live game's fields back on live's (the game's truth)">The game's truth</button>
            {homeless.length > 0 && <button disabled={disabled} onClick={() => onPicks({...picks, ...Object.fromEntries(homeless.map(c => [c.id, {whole: 'unplace' as const}]))})}
                title="Each place the new map hasn't unset: they are kept, unplaced or without work">Unplace the {homeless.length} without a place</button>}</div>
        {conflicts.map(item => {
            const pick = picks[item.id];
            const gone = item.gone ?? [];
            const byField = !!pick?.byField && item.fieldChoices;
            const shown = open[item.id] ?? byField;
            const both = differingFields(item).filter(f => f.changed === 'conflict' || f.changed === null).length;
            const sides = (['dev', 'live'] as const).filter(s => !gone.length || (s === 'dev' && item.dev !== 'absent'));
            const chosen = gone.length ? buildChoices([item], picks).missing.length === 0 : !!(pick?.whole || byField);
            const defaulted = item.default != null && !gone.length;
            return <section key={item.id} className={`sync-conflict${chosen ? ' chosen' : ''}`}>
                <header>
                    <div><b>{item.name}</b> <span className="meta">{item.label} · {rowChangeText(item)}{both ? ` · ${both} field${both === 1 ? '' : 's'} changed on both` : ''}</span>
                        {defaulted && <span className="sync-owner" title="The live game's fields: live's unless you choose dev's">starts on the game's</span>}
                        {gone.length > 0 && <span className="sync-owner warn" title="The new map hasn't this place: decide for each">not on the new map: {goneText(item)}</span>}</div>
                    <div className="segmented" role="radiogroup" aria-label={`Choice for ${item.name}`}>
                        {sides.map(s => <button key={s} role="radio" disabled={disabled} aria-checked={!byField && pick?.whole === s}
                            className={!byField && pick?.whole === s ? 'on' : ''} onClick={() => set(item.id, {...pick, whole: s, byField: false})}>{labels[s]}</button>)}
                        {gone.length > 0 && <button role="radio" disabled={disabled} aria-checked={pick?.whole === 'unplace'} className={pick?.whole === 'unplace' ? 'on' : ''}
                            title="Live's, with each place the new map hasn't unset" onClick={() => set(item.id, {...pick, whole: 'unplace', byField: false})}>Unplace</button>}
                        {item.fieldChoices && <button role="radio" disabled={disabled} aria-checked={byField} className={byField ? 'on' : ''}
                            onClick={() => { set(item.id, {...pick, whole: undefined, byField: true}); setOpen({...open, [item.id]: true}); }}>Field by field</button>}
                    </div>
                    <button className="icon" aria-expanded={shown} onClick={() => setOpen({...open, [item.id]: !shown})}
                        title={shown ? 'Hide the fields' : 'Show base, dev and live side by side'}>{shown ? '▾' : '▸'}</button>
                </header>
                {gone.length > 0 && pick?.whole !== 'unplace' && pick?.whole !== 'dev' && <div className="sync-places">
                    {gone.map(g => <PlaceDecision key={g.place} gone={g} choice={pick?.places?.[g.place]} areas={areas} disabled={disabled}
                        onChoose={c => set(item.id, {...pick, whole: undefined, places: {...pick?.places, [g.place]: c}})} />)}</div>}
                {shown && <FieldTable item={item} pick={byField ? pick : undefined}
                    whole={byField || pick?.whole === 'unplace' ? undefined : pick?.whole as Side | undefined} labels={labels}
                    onField={byField && !disabled ? (field, side) => set(item.id, {...pick, byField: true, fields: {...pick?.fields, [field]: side}}) : undefined} />}
            </section>;
        })}
    </div>;
}

/** A summary's lines as a small table ("NPCs: 2 added, 1 changed"). */
export function SummaryTable({summary, empty = 'Nothing changes.'}: {summary: Summary | undefined; empty?: string}) {
    const rows = summaryRows(summary);
    if (!rows.length) return <p className="meta">{empty}</p>;
    return <table className="summary"><tbody>{rows.map(([what, text]) => <tr key={what}><th>{what}</th><td>{text}</td></tr>)}</tbody></table>;
}

/** Problems from the engine (or the dev world's checks), listed. */
export function Problems({title, items}: {title: string; items: string[]}) {
    if (!title && !items.length) return null;
    return <div className="publish-errors"><b>{title}</b>
        {items.length > 0 && <ul>{items.slice(0, 20).map((e, i) => <li key={i}>{e}</li>)}{items.length > 20 && <li>…and {items.length - 20} more</li>}</ul>}</div>;
}

export const when = (at: string | null | undefined) => (at ? new Date(at).toLocaleString() : '');
