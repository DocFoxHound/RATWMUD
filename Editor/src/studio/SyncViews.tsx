// What Atlas's Update from live and the DM's Import from dev both show (doc 66, section 4): changes grouped by table,
// and the conflicts side by side (base, dev, live), field by field, with a choice for each (lib/sync.ts builds the
// engine's choices from them).
import {useState} from 'react';
import {differingFields, fieldSide, groupByLabel, rowChangeText, sideChangeText, summaryRows, valueText, type Picks, type Side,
    type Summary, type SyncItem} from '../lib/sync';

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
                <th>{f.field}</th><td><Value value={f.base} /></td>
                <td className={side === 'dev' ? 'sync-kept' : ''}><Value value={f.dev} /></td>
                <td className={side === 'live' ? 'sync-kept' : ''}><Value value={f.live} /></td>
                <td>{onField && differs && labels ? <div className="segmented">{(['dev', 'live'] as const).map(s =>
                    <button key={s} className={side === s ? 'on' : ''} aria-pressed={side === s} onClick={() => onField(f.field, s)}
                        title={f.changed === 'conflict' ? 'Both changed this field' : f.changed === s ? `Only ${s} changed it` : ''}>{labels[s]}</button>)}</div>
                    : <span className="meta">{f.changed === 'conflict' ? 'both' : f.changed === 'same' ? 'same now' : f.changed ?? '—'}</span>}</td>
            </tr>;
        })}</tbody></table>;
}

/** The conflicts, each with its choice: the whole row from dev or live, or (rows both sides have) field by field.
 *  `labels` name the choices for the screen ("Keep dev's" / "Take live's" in Atlas; "Import dev's" / "Keep live's" in
 *  the DM). */
export function ConflictList({conflicts, picks, onPicks, labels, disabled = false}: {conflicts: SyncItem[]; picks: Picks;
        onPicks: (picks: Picks) => void; labels: Record<Side, string>; disabled?: boolean}) {
    const [open, setOpen] = useState<Record<string, boolean>>({});
    if (!conflicts.length) return <p className="meta">No conflicts: nothing was changed on both sides.</p>;
    const set = (id: string, pick: Picks[string]) => onPicks({...picks, [id]: pick});
    return <div className="sync-conflicts">
        <div className="sync-bulk"><span className="meta">All conflicts:</span>
            <button disabled={disabled} onClick={() => onPicks(Object.fromEntries(conflicts.map(c => [c.id, {whole: 'dev' as const}])))}>{labels.dev}</button>
            <button disabled={disabled} onClick={() => onPicks(Object.fromEntries(conflicts.map(c => [c.id, {whole: 'live' as const}])))}>{labels.live}</button></div>
        {conflicts.map(item => {
            const pick = picks[item.id];
            const byField = !!pick?.byField && item.fieldChoices;
            const shown = open[item.id] ?? byField;
            const both = differingFields(item).filter(f => f.changed === 'conflict' || f.changed === null).length;
            return <section key={item.id} className={`sync-conflict${pick?.whole || byField ? ' chosen' : ''}`}>
                <header>
                    <div><b>{item.name}</b> <span className="meta">{item.label} · {rowChangeText(item)}{both ? ` · ${both} field${both === 1 ? '' : 's'} changed on both` : ''}</span></div>
                    <div className="segmented" role="radiogroup" aria-label={`Choice for ${item.name}`}>
                        {(['dev', 'live'] as const).map(s => <button key={s} role="radio" disabled={disabled} aria-checked={!byField && pick?.whole === s}
                            className={!byField && pick?.whole === s ? 'on' : ''} onClick={() => set(item.id, {...pick, whole: s, byField: false})}>{labels[s]}</button>)}
                        {item.fieldChoices && <button role="radio" disabled={disabled} aria-checked={byField} className={byField ? 'on' : ''}
                            onClick={() => { set(item.id, {...pick, byField: true}); setOpen({...open, [item.id]: true}); }}>Field by field</button>}
                    </div>
                    <button className="icon" aria-expanded={shown} onClick={() => setOpen({...open, [item.id]: !shown})}
                        title={shown ? 'Hide the fields' : 'Show base, dev and live side by side'}>{shown ? '▾' : '▸'}</button>
                </header>
                {shown && <FieldTable item={item} pick={byField ? pick : undefined} whole={byField ? undefined : pick?.whole} labels={labels}
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
