// The workbench's tree (Docs/Design/85, 4.2): the campaign triggers, then each phase with its triggers by folder (numbered
// in the order they are tried), its Exits and its missions; then the story's regions, scenes, rehearsal cases and
// settings. Drag a trigger to reorder it, or into another folder or phase; right-click for duplicate, copy, cut,
// paste, rename and delete. Shift and Ctrl pick several. After a rehearsal each trigger shows how often it fired.
import {useState} from 'react';
import {buildTree, eventOf, exitAt, folderEnd, moveTrigger, renameFolder, sameRef, traceKey, triggerAt, type Branch, type Ref} from '../../lib/workbench';
import {useBench} from './context';

export interface TreeActions {
    duplicate: (refs: Ref[]) => void; copy: (refs: Ref[], text?: boolean) => void; cut: (refs: Ref[]) => void; paste: (at: Ref | {scope: number | null; folder?: string | null}) => void;
    remove: (refs: Ref[]) => void; rename: (ref: Ref) => void; template: (refs: Ref[]) => void; newTrigger: (scope: number | null, folder?: string | null) => void;
    newPhase: () => void; newExit: (phase: number) => void; canPaste: boolean;
}

const refKey = (r: Ref) => JSON.stringify(r);

export function TriggerTree({selected, picked, select, actions, counts, trace, problemRefs}: {selected: Ref | null; picked: Ref[];
    select: (r: Ref, how?: 'add' | 'range') => void; actions: TreeActions; counts?: Record<string, number> | null; trace?: Set<string> | null;
    problemRefs: Set<string>}) {
    const {program, cat, edit} = useBench();
    const tree = buildTree(program);
    const [closed, setClosed] = useState<Set<string>>(new Set());
    const [menu, setMenu] = useState<{x: number; y: number; ref: Ref} | null>(null);
    const [drop, setDrop] = useState<string | null>(null);
    const toggle = (k: string) => setClosed(c => { const n = new Set(c); if (n.has(k)) n.delete(k); else n.add(k); return n; });
    const isPicked = (r: Ref) => picked.some(p => sameRef(p, r));
    const targets = (r: Ref) => isPicked(r) && picked.length > 1 ? picked : [r];

    function onDrop(e: React.DragEvent, scope: number | null, index: number, folder: string | null | undefined) {
        e.preventDefault(); setDrop(null);
        let from: Ref;
        try { from = JSON.parse(e.dataTransfer.getData('application/x-ratw-trigger')); } catch { return; }
        if (from.kind !== 'trigger') return;
        const {program: next, ref} = moveTrigger(program, from, {phase: scope, index, folder});
        edit(next); select(ref);
    }
    const dropProps = (key: string, scope: number | null, index: () => number, folder: string | null | undefined) => ({
        onDragOver: (e: React.DragEvent) => { if (e.dataTransfer.types.includes('application/x-ratw-trigger')) { e.preventDefault(); setDrop(key); } },
        onDragLeave: () => setDrop(d => d === key ? null : d),
        onDrop: (e: React.DragEvent) => onDrop(e, scope, index(), folder),
    });

    function Row({r, order, folder}: {r: Ref & {kind: 'trigger'}; order: number; folder: string | null}) {
        const t = triggerAt(program, r)!;
        const k = refKey(r);
        const count = counts?.[traceKey(program, r)];
        const event = t.on === 'story signal' ? '↯ ' + t.signal : t.on === 'clock' ? '⏱ ' + t.afterDays + ' d' : eventOf(cat, t.on)?.title ?? t.on;
        return <li className={'wb-node' + (sameRef(selected, r) ? ' selected' : '') + (isPicked(r) ? ' picked' : '') + (drop === k ? ' drop' : '')
            + (trace && !trace.has(traceKey(program, r)) ? ' dim' : '') + (problemRefs.has(k) ? ' bad' : '')}
            draggable onDragStart={e => { e.dataTransfer.setData('application/x-ratw-trigger', JSON.stringify(r)); e.dataTransfer.effectAllowed = 'move'; }}
            {...dropProps(k, r.phase, () => r.index, folder)}
            onClick={e => select(r, e.shiftKey ? 'range' : e.ctrlKey || e.metaKey ? 'add' : undefined)}
            onContextMenu={e => { e.preventDefault(); if (!isPicked(r)) select(r); setMenu({x: e.clientX, y: e.clientY, ref: r}); }}
            title={t.note ?? undefined} aria-selected={sameRef(selected, r)} role="treeitem">
            <span className="wb-order">{order}</span><span className="wb-name">{t.id}</span><small>{event}</small>
            {count !== undefined && <b className="wb-count" title={`fired ${count}× in the rehearsal`}>×{count}</b>}
            {t.note && <span className="wb-noted" aria-hidden>✎</span>}
        </li>;
    }

    function Scope({b}: {b: Branch}) {
        const k = 'b:' + b.scope;
        const open = !closed.has(k);
        const phaseRef: Ref | null = b.scope === null ? null : {kind: 'phase', phase: b.scope};
        return <li className="wb-branch">
            <div className={'wb-branch-head' + (phaseRef && sameRef(selected, phaseRef) ? ' selected' : '') + (drop === k ? ' drop' : '')}
                {...dropProps(k, b.scope, () => folderEnd(program, b.scope, null), null)}>
                <button type="button" className="wb-fold" onClick={() => toggle(k)} aria-expanded={open}>{open ? '▾' : '▸'}</button>
                <button type="button" className="wb-branch-name" onClick={() => phaseRef ? select(phaseRef) : toggle(k)}>
                    {b.scope === null ? b.label : <>Phase: {b.label}</>}{b.opening && <small className="wb-tag">opening</small>}{b.ending && <small className="wb-tag">ending</small>}</button>
                <button type="button" className="wb-plus" title="New trigger here" aria-label={'New trigger in ' + b.label} onClick={() => actions.newTrigger(b.scope)}>+</button>
            </div>
            {open && <ul role="group">
                {b.folders.map(f => {
                    const fk = k + ':f:' + f.name;
                    const fopen = !closed.has(fk);
                    return <li key={f.name} className="wb-folder">
                        <div className={'wb-folder-head' + (drop === fk ? ' drop' : '')} {...dropProps(fk, b.scope, () => folderEnd(program, b.scope, f.name), f.name)}
                            onDoubleClick={() => { const name = window.prompt('Rename the folder (empty: take the triggers out of it)', f.name);
                                if (name !== null) edit(renameFolder(program, b.scope, f.name, name.trim() || null)); }}>
                            <button type="button" className="wb-fold" onClick={() => toggle(fk)} aria-expanded={fopen}>{fopen ? '▾' : '▸'}</button>
                            <span>📁 {f.name}</span><small>{f.items.length}</small>
                            <button type="button" className="wb-plus" title="New trigger in this folder" aria-label={'New trigger in folder ' + f.name} onClick={() => actions.newTrigger(b.scope, f.name)}>+</button></div>
                        {fopen && <ul role="group">{f.items.map(i => <Row key={refKey(i.ref)} r={i.ref as Ref & {kind: 'trigger'}} order={i.order} folder={f.name}/>)}</ul>}
                    </li>;
                })}
                {b.loose.map(i => <Row key={refKey(i.ref)} r={i.ref as Ref & {kind: 'trigger'}} order={i.order} folder={null}/>)}
                {b.scope !== null && <li className="wb-folder">
                    <div className="wb-folder-head"><button type="button" className="wb-fold" onClick={() => toggle(k + ':exits')}>{closed.has(k + ':exits') ? '▸' : '▾'}</button>
                        <span>Exits</span><small>{b.exits.length}</small>
                        <button type="button" className="wb-plus" title="New exit" aria-label={'New exit in ' + b.label} onClick={() => actions.newExit(b.scope!)}>+</button></div>
                    {!closed.has(k + ':exits') && <ul role="group">{b.exits.map(r => {
                        const x = exitAt(program, r)!;
                        return <li key={refKey(r)} role="treeitem" className={'wb-node exit' + (sameRef(selected, r) ? ' selected' : '') + (problemRefs.has(refKey(r)) ? ' bad' : '')} onClick={() => select(r)}>
                            <span className="wb-order">→</span><span className="wb-name">{x.to}</span><small>{eventOf(cat, x.on)?.title ?? x.on}</small></li>;
                    })}</ul>}</li>}
                {b.scope !== null && <li role="treeitem" className={'wb-node aux' + (sameRef(selected, {kind: 'missions', phase: b.scope}) ? ' selected' : '')}
                    onClick={() => select({kind: 'missions', phase: b.scope!})}><span className="wb-order">☰</span><span className="wb-name">Missions</span><small>{b.missions}</small></li>}
                {b.scope !== null && <li role="treeitem" className={'wb-node aux' + (sameRef(selected, {kind: 'entry', phase: b.scope}) ? ' selected' : '')}
                    onClick={() => select({kind: 'entry', phase: b.scope!})}><span className="wb-order">✦</span><span className="wb-name">When it opens</span><small>{b.entry}</small></li>}
            </ul>}
        </li>;
    }

    const aux: [Ref, string, string][] = [[{kind: 'settings'}, 'Campaign settings', ''], [{kind: 'regions'}, 'Map regions', String(program.regions?.length ?? 0)],
        [{kind: 'scenes'}, 'Embedded scenes', String(program.scenes?.length ?? 0)], [{kind: 'tests'}, 'Rehearsal cases', String(program.tests.length)]];
    return <nav className="wb-tree" aria-label="Triggers" onClick={() => setMenu(null)}>
        <ul role="tree">{tree.map(b => <Scope key={String(b.scope)} b={b}/>)}</ul>
        <ul className="wb-aux">{aux.map(([r, label, n]) => <li key={label} role="treeitem" className={'wb-node aux' + (sameRef(selected, r) ? ' selected' : '') + (problemRefs.has(refKey(r)) ? ' bad' : '')}
            onClick={() => select(r)}><span className="wb-name">{label}</span><small>{n}</small></li>)}</ul>
        <div className="wb-tree-tools">
            <button type="button" onClick={() => actions.newPhase()}>+ Phase</button>
            <button type="button" onClick={() => { const name = window.prompt('A name for the new folder'); if (name?.trim()) actions.newTrigger(selected?.kind === 'trigger' || selected?.kind === 'phase' ? (selected.phase ?? null) : null, name.trim()); }}>+ Folder</button>
            <button type="button" disabled={!actions.canPaste} onClick={() => actions.paste(selected ?? {scope: null})}>Paste</button>
        </div>
        {menu && <div className="wb-menu" style={{left: menu.x, top: menu.y}} role="menu" onClick={e => e.stopPropagation()}>
            {([['Duplicate', 'Ctrl+D', () => actions.duplicate(targets(menu.ref))], ['Copy', 'Ctrl+C', () => actions.copy(targets(menu.ref))],
                ['Cut', 'Ctrl+X', () => actions.cut(targets(menu.ref))], ['Paste after', 'Ctrl+V', () => actions.paste(menu.ref)],
                ['Copy as text', '', () => actions.copy(targets(menu.ref), true)], ['Rename…', 'F2', () => actions.rename(menu.ref)],
                ['Save as a template…', '', () => actions.template(targets(menu.ref))], ['Delete', 'Del', () => actions.remove(targets(menu.ref))]] as [string, string, () => void][])
                .map(([label, keys, run]) => <button key={label} type="button" role="menuitem" disabled={label === 'Paste after' && !actions.canPaste}
                    onClick={() => { run(); setMenu(null); }}><span>{label}</span><small>{keys}</small></button>)}
        </div>}
    </nav>;
}
