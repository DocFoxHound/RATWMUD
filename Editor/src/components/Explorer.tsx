import {useState, type ReactNode} from 'react';
import {ROLE_INFO} from '../lib/glyphs';
import {focusPlace, openView, setState, useStore, useTool} from '../lib/store';
import {newRoute} from '../lib/commands';
import * as M from '../model/model.mjs';
import type {Role} from '../model/model.mjs';

function Group({title, count, children, action, initiallyOpen = true}: {title: string; count: number; children: ReactNode; action?: ReactNode; initiallyOpen?: boolean}) {
    const [open, setOpen] = useState(initiallyOpen);
    return (
        <section className="tree-group">
            <header>
                <button className="tree-toggle" onClick={() => setOpen(!open)} aria-expanded={open}>
                    <span className={open ? 'chev open' : 'chev'}>▸</span>{title}<em>{count}</em>
                </button>
                {action}
            </header>
            {open && <div className="tree-items">{children}</div>}
        </section>
    );
}

export function Explorer() {
    const project = useStore(s => s.project);
    const view = useStore(s => s.view);
    const workspace = useStore(s => s.workspace);
    const people_ = workspace === 'people', interiors_ = workspace === 'interiors';
    const openRoom = view.kind === 'cell' && project.rooms.some(r => r.id === view.id) ? view.id : null;
    const selection = useStore(s => s.selection);
    const [query, setQuery] = useState('');
    const q = query.trim().toLowerCase();
    const match = (...text: string[]) => !q || text.some(t => t.toLowerCase().includes(q));
    const viewing = (id: string) => view.kind === 'cell' && view.id === id;
    const selected = (kind: string, id?: string) => selection?.kind === kind && (!id || ('id' in selection && selection.id === id)
        || (selection.kind === 'cells' && selection.ids.includes(id)));

    const cells = project.cells.filter(c => match(c.name, c.id));
    const rooms = project.rooms.filter(c => match(c.name, c.id));
    const people = project.people.filter(p => match(p.name, p.id, p.role, p.workLabel));
    const roster = useStore(s => s.roster);
    const preview = useStore(s => s.preview);
    const slots = project.slots.filter(s => match(s.name, s.id, s.profession));
    const holder = (slotId: string) => preview?.plan.find(e => e.slot === slotId)?.name;
    const routes = project.routes.filter(r => match(r.name, r.id));
    const links = project.links.filter(l => match(l.name, l.id) && (!interiors_ || l.a.cell === openRoom || l.b.cell === openRoom));
    const doorsOf = (id: string) => project.links.filter(l => l.a.cell === id || l.b.cell === id).length;

    const place = (id: string) => { setState({selection: {kind: 'cells', ids: [id]}}); openView({kind: 'cell', id}); };
    const item = (key: string, active: boolean, onClick: () => void, icon: ReactNode, label: string, detail?: string, iconColor?: string) =>
        <button key={key} className={active ? 'tree-item active' : 'tree-item'} onClick={onClick}>
            <span className="tree-icon" style={iconColor ? {color: iconColor} : undefined}>{icon}</span>
            <span className="tree-label">{label}</span>{detail && <small>{detail}</small>}
        </button>;

    return (
        <nav className="explorer" aria-label="World explorer">
            <div className="explorer-search">
                <input value={query} onChange={e => setQuery(e.target.value)} placeholder="Search places, people…" aria-label="Search the world" />
            </div>
            <div className="explorer-scroll">
                {!interiors_ && <>
                {item('world', view.kind === 'world', () => { openView({kind: 'world'}); setState({selection: null}); }, '◇', 'World overview', `${project.cells.length} cells`)}
                <Group title="World cells" count={cells.length} initiallyOpen={!people_} key={`cells-${workspace}`}>
                    {cells.map(c => item(c.id, viewing(c.id) || selected('cells', c.id), () => place(c.id), '▦', c.name, `${c.width}×${c.height}`))}
                </Group>
                </>}
                <Group title="Interiors" count={rooms.length} initiallyOpen={interiors_} key={`rooms-${workspace}`} action={<button className="icon" title="New interior" onClick={() => setState({dialog: 'room'})}>+</button>}>
                    {rooms.map(c => item(c.id, viewing(c.id) || selected('cells', c.id), () => place(c.id), '▣', c.name,
                        interiors_ ? `${c.width}×${c.height} · ${doorsOf(c.id)} door${doorsOf(c.id) === 1 ? '' : 's'}` : `${c.width}×${c.height}`))}
                    {!rooms.length && <p className="tree-empty">Use + for a blank room, or the Building tool (U) on the Map for a building with its interior.</p>}
                </Group>
                {people_ && <>
                <Group title="Named NPCs" count={people.length} action={<button className="icon" title="Place a named NPC" onClick={() => useTool('person')}>+</button>}>
                    {(['merchant', 'guard', 'civilian'] as Role[]).map(role => people.filter(p => p.role === role).map(p =>
                        item(p.id, selected('person', p.id), () => { setState({selection: {kind: 'person', id: p.id}}); focusPlace(p.work); },
                            ROLE_INFO[role].icon, p.name, p.workLabel, ROLE_INFO[role].color)))}
                    {!people.length && <p className="tree-empty">No residents yet. Press P and click the map.</p>}
                </Group>
                <Group title="Profession slots" count={slots.length} action={<button className="icon" title="Place profession slots" onClick={() => useTool('slot')}>+</button>}>
                    {slots.map(s => item(s.id, selected('slot', s.id), () => { setState({selection: {kind: 'slot', id: s.id}}); focusPlace(s.work); },
                        '◇', s.name, holder(s.id) ?? roster?.professions.find(p => p.id === s.profession)?.name ?? s.profession, '#b6a3cf'))}
                    {!slots.length && <p className="tree-empty">Jobs filled from the character roster. Press J and click the map.</p>}
                </Group>
                <Group title="Patrol routes" count={routes.length} action={<button className="icon" title="New patrol route" onClick={newRoute}>+</button>}>
                    {routes.map(r => item(r.id, selected('route', r.id), () => { setState({selection: {kind: 'route', id: r.id}}); focusPlace(r.posts[0] ?? null); },
                        '⟲', r.name, `${r.posts.length} posts`))}
                </Group>
                </>}
                {!people_ && <>
                {(!interiors_ || openRoom) && <Group title={interiors_ ? 'Doors in this interior' : 'Connections'} count={links.length} key={`links-${workspace}`}>
                    {links.map(l => {
                        const here = interiors_ && l.b.cell === openRoom ? l.b : l.a, there = here === l.a ? l.b : l.a;
                        return item(l.id, selected('link', l.id), () => { setState({selection: {kind: 'link', id: l.id}}); focusPlace(here); },
                            l.kind === 'stairs' ? '^' : l.kind === 'passage' ? '≡' : '+', l.name, interiors_ ? `→ ${M.getCell(project, there.cell)?.name ?? there.cell}` : undefined);
                    })}
                    {interiors_ && openRoom && !links.length && <p className="tree-empty">No way in yet. Use Connect (D) to add a door to a street or another room.</p>}
                </Group>}
                {!interiors_ && <Group title="Markers" count={2}>
                    {item('spawn', selected('spawn'), () => { setState({selection: {kind: 'spawn'}}); focusPlace(project.spawn); }, '★', 'Player spawn', project.spawn ? undefined : 'not set', '#e6c481')}
                    {item('herb', selected('herb'), () => { setState({selection: {kind: 'herb'}}); focusPlace(project.herbPatch); }, '❦', 'Herb patch', project.herbPatch ? 'resource' : 'none', '#8fb56f')}
                </Group>}
                </>}
            </div>
        </nav>
    );
}
