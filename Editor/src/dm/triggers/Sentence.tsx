// A sentence of the workbench (Docs/Design/85, 4.2): words and underlined slots; a click on a slot opens the picker for its
// type in place. A slot that fails a check is red, with the problem as its tooltip.
import {useState} from 'react';
import type {Part, Slot} from '../../lib/workbench';
import {Picker, type Values} from './pickers';

export function Sentence({parts, pick, problems, clearable, cues, readOnly = false, lead}: {parts: Part[]; pick?: (slot: Slot, v: Values) => void;
    problems?: string[]; clearable?: (slot: Slot) => boolean; cues?: string[]; readOnly?: boolean; lead?: React.ReactNode}) {
    const [open, setOpen] = useState<string | null>(null);
    const slot = open !== null ? parts.find((p): p is Slot => 'slot' in p && p.slot === open) : undefined;
    const bad = !!problems?.length;
    return <div className={'wb-sentence' + (bad ? ' bad' : '')} title={bad ? problems!.join('\n') : undefined}>
        <p>{lead}{parts.map((p, i) => 'text' in p ? <span key={i}>{p.text}</span>
            : <button key={i} type="button" disabled={readOnly || !pick} className={'wb-slot' + (p.empty ? ' empty' : '') + (open === p.slot ? ' open' : '')}
                onClick={() => setOpen(open === p.slot ? null : p.slot)} title={p.label}>{p.shown || p.label}</button>)}</p>
        {slot && pick && <Picker slot={slot} cues={cues} clearable={clearable?.(slot)} close={() => setOpen(null)}
            pick={v => { pick(slot, v); if (slot.type !== 'missions') setOpen(null); }}/>}
    </div>;
}
