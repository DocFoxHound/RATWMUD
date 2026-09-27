import {useEffect, useId, useState, type ReactNode} from 'react';

/** Text input that commits on blur or Enter, so one edit is one undo step. */
export function TextField({label, value, onCommit, multiline = false, max, placeholder, hint}: {
    label: string; value: string; onCommit: (v: string) => void; multiline?: boolean; max?: number; placeholder?: string; hint?: string;
}) {
    const [draft, setDraft] = useState(value);
    const id = useId();
    useEffect(() => setDraft(value), [value]);
    const done = () => { if (draft !== value) onCommit(draft); };
    return (
        <label className="field" htmlFor={id}>
            <span>{label}</span>
            {multiline
                ? <textarea id={id} value={draft} rows={3} maxLength={max} placeholder={placeholder} onChange={e => setDraft(e.target.value)} onBlur={done} />
                : <input id={id} value={draft} maxLength={max} placeholder={placeholder} onChange={e => setDraft(e.target.value)} onBlur={done}
                    onKeyDown={e => { if (e.key === 'Enter') (e.target as HTMLInputElement).blur(); if (e.key === 'Escape') { setDraft(value); } }} />}
            {hint && <small>{hint}</small>}
        </label>
    );
}

export function NumberField({label, value, onCommit, min, max, step = 1, suffix}: {
    label: string; value: number; onCommit: (v: number) => void; min?: number; max?: number; step?: number; suffix?: string;
}) {
    const [draft, setDraft] = useState(String(value));
    const id = useId();
    useEffect(() => setDraft(String(value)), [value]);
    const done = () => {
        const n = Number(draft);
        if (draft.trim() === '' || !Number.isFinite(n)) { setDraft(String(value)); return; }
        const clamped = Math.min(max ?? Infinity, Math.max(min ?? -Infinity, n));
        if (clamped !== value) onCommit(clamped); else setDraft(String(value));
    };
    return (
        <label className="field" htmlFor={id}>
            <span>{label}</span>
            <div className="number-row">
                <input id={id} type="number" value={draft} min={min} max={max} step={step} onChange={e => setDraft(e.target.value)} onBlur={done}
                    onKeyDown={e => { if (e.key === 'Enter') (e.target as HTMLInputElement).blur(); }} />
                {suffix && <em>{suffix}</em>}
            </div>
        </label>
    );
}

export function SelectField<T extends string>({label, value, options, onChange, hint}: {
    label: string; value: T; options: {value: T; label: string}[]; onChange: (v: T) => void; hint?: string;
}) {
    const id = useId();
    return (
        <label className="field" htmlFor={id}>
            <span>{label}</span>
            <select id={id} value={value} onChange={e => onChange(e.target.value as T)}>
                {options.map(o => <option key={o.value} value={o.value}>{o.label}</option>)}
            </select>
            {hint && <small>{hint}</small>}
        </label>
    );
}

export function Toggle({label, checked, onChange, hint}: {label: string; checked: boolean; onChange: (v: boolean) => void; hint?: string}) {
    return (
        <label className="toggle">
            <input type="checkbox" checked={checked} onChange={e => onChange(e.target.checked)} />
            <span className="toggle-track" aria-hidden="true" />
            <span className="toggle-text">{label}{hint && <small>{hint}</small>}</span>
        </label>
    );
}

export function Slider({label, value, onCommit, min = 0, max = 1, step = 0.05, format}: {
    label: string; value: number; onCommit: (v: number) => void; min?: number; max?: number; step?: number; format?: (v: number) => string;
}) {
    const [draft, setDraft] = useState(value);
    useEffect(() => setDraft(value), [value]);
    return (
        <label className="field slider">
            <span>{label}<b>{format ? format(draft) : draft}</b></span>
            <input type="range" min={min} max={max} step={step} value={draft} onChange={e => setDraft(Number(e.target.value))}
                onPointerUp={() => draft !== value && onCommit(draft)} onKeyUp={() => draft !== value && onCommit(draft)} />
        </label>
    );
}

export function Section({title, children, actions, open = true}: {title: string; children: ReactNode; actions?: ReactNode; open?: boolean}) {
    return (
        <details className="section" open={open}>
            <summary><span>{title}</span>{actions && <span className="section-actions" onClick={e => e.preventDefault()}>{actions}</span>}</summary>
            <div className="section-body">{children}</div>
        </details>
    );
}

export function Row({children}: {children: ReactNode}) { return <div className="row">{children}</div>; }
export function Hint({children}: {children: ReactNode}) { return <p className="hint">{children}</p>; }
