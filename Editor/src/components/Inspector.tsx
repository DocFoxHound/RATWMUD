import * as M from '../model/model.mjs';
import type {AnyCell, Person, Place, PlaceRef, Project, Role, Room} from '../model/model.mjs';
import {COATS, PATTERNS, ROLES, SEXES, SPECIES, STATURES} from '../model/model.mjs';
import {commit, focusPlace, getState, openView, setState, useStore} from '../lib/store';
import {formatHour, ROLE_INFO, VOICE_COLORS} from '../lib/glyphs';
import {isRoom, placeLabel} from '../lib/surface';
import {Hint, NumberField, Row, Section, SelectField, Slider, TextField, Toggle} from './fields';
import {api} from '../lib/api';
import {BEHAVIOR_LABELS} from '../lib/roster';
import {setWorkspace, toast, useTool} from '../lib/store';
import {deleteCell, newRoute} from '../lib/commands';
import {Portrait} from './Portrait';

export function Inspector() {
    const selection = useStore(s => s.selection);
    const project = useStore(s => s.project);
    let body;
    const workspace = useStore(s => s.workspace);
    const view = useStore(s => s.view);
    const openRoom = view.kind === 'cell' ? project.rooms.find(r => r.id === view.id) : undefined;
    if (!selection) body = workspace === 'people' ? <PeopleOverview project={project} />
        : workspace === 'interiors' ? (openRoom ? <CellInspector project={project} cell={openRoom} pinned /> : <InteriorsOverview project={project} />)
        : <WorldInspector project={project} />;
    else if (selection.kind === 'cells') {
        const cells = selection.ids.map(id => M.getCell(project, id)).filter(Boolean) as AnyCell[];
        body = cells.length === 1 ? <CellInspector project={project} cell={cells[0]} /> : <MultiCellInspector cells={cells} />;
    } else if (selection.kind === 'person') {
        const person = project.people.find(p => p.id === selection.id);
        body = person ? <PersonInspector project={project} person={person} /> : null;
    } else if (selection.kind === 'slot') {
        const slot = project.slots.find(s => s.id === selection.id);
        body = slot ? <SlotInspector project={project} slot={slot} /> : null;
    } else if (selection.kind === 'link') {
        const link = project.links.find(l => l.id === selection.id);
        body = link ? <LinkInspector project={project} link={link} /> : null;
    } else if (selection.kind === 'route') {
        const route = project.routes.find(r => r.id === selection.id);
        body = route ? <RouteInspector project={project} route={route} /> : null;
    } else if (selection.kind === 'spawn' || selection.kind === 'herb') body = <MarkerInspector project={project} kind={selection.kind} />;
    return <aside className="inspector" aria-label="Inspector">{body}</aside>;
}

function Header({eyebrow, title, onClose, children}: {eyebrow: string; title: string; onClose?: () => void; children?: React.ReactNode}) {
    return (
        <header className="inspector-head">
            <div><small>{eyebrow}</small><h2>{title}</h2></div>
            {children}
            {onClose && <button className="icon" onClick={onClose} title="Back to world settings (Esc)" aria-label="Close">×</button>}
        </header>
    );
}
const close = () => setState({selection: null});

function PickButton({refTo, label, place}: {refTo: PlaceRef; label: string; place: Place | null}) {
    const pending = useStore(s => s.pending);
    const active = pending?.kind === 'place' && JSON.stringify(pending.ref) === JSON.stringify(refTo);
    return (
        <div className="place-row">
            <div><small>{label}</small><span>{placeLabel(getState().project, place)}</span></div>
            <button className={active ? 'active' : ''} onClick={() => setState({pending: active ? null : {kind: 'place', ref: refTo, label: label.toLowerCase()}})}>{active ? 'Cancel' : 'Pick'}</button>
            <button onClick={() => focusPlace(place)} disabled={!place} title="Show on map">Go</button>
        </div>
    );
}

// --------------------------------------------------------------------------- World

function WorldInspector({project}: {project: Project}) {
    const counts = {
        merchant: project.people.filter(p => p.role === 'merchant').length,
        guard: project.people.filter(p => p.role === 'guard').length,
        civilian: project.people.filter(p => p.role === 'civilian').length,
    };
    const bounds = M.cellBounds(project);
    return (<>
        <Header eyebrow="WORLD SETTINGS" title={project.name} />
        <Section title="Atlas">
            <TextField label="World name" value={project.name} max={120} onCommit={v => commit('Renamed the world.', p => { p.name = v; M.normalizeProject(p, true); }, true)} />
            <div className="stats">
                <div><b>{project.cells.reduce((n, c) => n + c.width * c.height, 0).toLocaleString()}</b><span>world tiles</span></div>
                <div><b>{project.cells.length}</b><span>world cells</span></div>
                <div><b>{project.rooms.length}</b><span>interiors</span></div>
                <div><b>{project.links.length}</b><span>connections</span></div>
            </div>
            {bounds && <p className="meta">Cells span {bounds.x}, {bounds.y} to {bounds.x + bounds.width - 1}, {bounds.y + bounds.height - 1} in world tiles.</p>}
            <Hint>The world is the ground its cells hold, and it has no edge: place a new cell anywhere and it snaps beside its
                neighbours. Delete a cell and its ground goes with it. Where no cell is, there is no ground.</Hint>
            <div className="button-grid">
                <button onClick={() => useTool('cell')}>▢ New cell (C)</button>
            </div>
        </Section>
        <Section title="Markers">
            <PickButton refTo={{kind: 'spawn'}} label="Player spawn" place={project.spawn} />
            <PickButton refTo={{kind: 'herbPatch'}} label="Herb patch · resource" place={project.herbPatch} />
            <Hint>The herb patch is a resource, not terrain: the one spot where players choose Gather to collect herbs. Its shared stock regrows each day (faster in spring). The game supports one per world for now.</Hint>
            {project.herbPatch && <button className="link-button" onClick={() => commit('Removed the herb patch.', p => M.setHerbPatch(p, null))}>Remove herb patch</button>}
        </Section>
        <Section title="Politics" open={false}>
            <Hint>{project.factions.length} factions and {project.chapters.length} Chapters. Claims and Chapter sites are set per cell.</Hint>
            <button onClick={() => setState({dialog: 'politics'})}>Edit factions & Chapters…</button>
        </Section>
        <Section title="Interiors" open={false}>
            <Hint>{project.rooms.length} interiors. Rooms, shops and other insides are drawn in the Interiors workspace; the Building tool (U) here adds a building with its interior already connected.</Hint>
            <button onClick={() => setWorkspace('interiors')}>Open Interiors (Alt+2)</button>
        </Section>
        <Section title="People" open={false}>
            <Hint>{counts.merchant + counts.guard + counts.civilian} named NPCs and {project.slots.length} profession slots. Residents, jobs, patrols and the economy are edited in the People workspace.</Hint>
            <button onClick={() => setWorkspace('people')}>Open People (Alt+3)</button>
        </Section>
    </>);
}

function InteriorsOverview({project}: {project: Project}) {
    return (<>
        <Header eyebrow="INTERIORS" title={`Inside ${project.name}`} />
        <Section title="No interiors yet">
            <Hint>Interiors are detached rooms: the inside of a shop, a house, a cave. Each is its own map, joined to the world by doors, passages or stairs.</Hint>
            <button className="primary" onClick={() => setState({dialog: 'room'})}>+ New blank interior…</button>
            <button onClick={() => useTool('building')}>⌂ Place a building on the Map…</button>
        </Section>
    </>);
}

function PeopleOverview({project}: {project: Project}) {
    const e = project.economy;
    const setE = (k: keyof M.Economy) => (v: number) => commit('Updated the economy.', p => M.setEconomy(p, {[k]: Math.round(v)}), true);
    const counts = {
        merchant: project.people.filter(p => p.role === 'merchant').length,
        guard: project.people.filter(p => p.role === 'guard').length,
        civilian: project.people.filter(p => p.role === 'civilian').length,
    };
    return (<>
        <Header eyebrow="PEOPLE" title={`Who lives in ${project.name}`} />
        <Section title="Population">
            <div className="stats">
                {(Object.keys(counts) as Role[]).map(r => <div key={r}><b style={{color: ROLE_INFO[r].color}}>{counts[r]}</b><span>{ROLE_INFO[r].label.toLowerCase()}s</span></div>)}
                <div><b style={{color: '#b6a3cf'}}>{project.slots.length}</b><span>profession slots</span></div>
            </div>
            <div className="button-grid">
                {ROLES.map(r => <button key={r} onClick={() => { useTool('person'); setState({role: r}); }}>+ {ROLE_INFO[r].label}</button>)}
            </div>
            <button onClick={() => useTool('slot')}>◇ Place profession slots…</button>
            <Hint><b>Named NPCs</b> are characters you design for one place. <b>Profession slots</b> are jobs; each export fills them from the shared Characters roster, permanently. Every resident needs a home (where they sleep), a work place and an evening place.</Hint>
        </Section>
        <Section title={`Patrol routes (${project.routes.length})`}>
            <Hint>A patrol route is an ordered loop of posts. A guard on watch stands at one post and moves on to the next every 15 game minutes (about 2½ real minutes); several guards on one route are spread out. Guards without a route hold their work post.</Hint>
            <button onClick={newRoute}>+ New patrol route</button>
        </Section>
        <Section title="Town economy">
            <Hint>The treasury pays wages to guards and working civilians. Shopkeepers restock from the town stores and pay market dues back, so money is never created from nothing. A daily carter refills the stores with goods.</Hint>
            <Row><NumberField label="Treasury (silver)" value={e.treasury} min={0} max={1000000} onCommit={setE('treasury')} />
                <NumberField label="Stored meals" value={e.storeMeals} min={0} max={10000} onCommit={setE('storeMeals')} /></Row>
            <Row><NumberField label="Stored herbs" value={e.storeHerbs} min={0} max={10000} onCommit={setE('storeHerbs')} />
                <NumberField label="Daily meal delivery" value={e.dailyMeals} min={0} max={1000} onCommit={setE('dailyMeals')} /></Row>
            <NumberField label="Daily herb delivery" value={e.dailyHerbs} min={0} max={1000} onCommit={setE('dailyHerbs')} />
        </Section>
    </>);
}

// --------------------------------------------------------------------------- Cells and rooms

function MultiCellInspector({cells}: {cells: AnyCell[]}) {
    return (<>
        <Header eyebrow={`${cells.length} CELLS SELECTED`} title="Multiple cells" onClose={close} />
        <Section title="Combine">
            <Hint>Merging needs a filled rectangle with matching level, weather, lighting and territory. The first selected cell's name survives.</Hint>
            <button className="primary wide" onClick={() => { const merged = commit('Merged cells.', p => M.mergeCells(p, cells.map(c => c.id))); if (merged) setState({selection: {kind: 'cells', ids: [merged.id]}}); }}>Merge selected cells</button>
        </Section>
        <Section title="Territory"><TerritoryEditor ids={cells.map(c => c.id)} territory={cells[0].territory} /></Section>
    </>);
}

/** `pinned`: shown as the Interiors workspace's page for the open room, so there is nothing to close. */
function CellInspector({project, cell, pinned = false}: {project: Project; cell: AnyCell; pinned?: boolean}) {
    const room = isRoom(cell) ? cell as Room : null;
    const update = (fields: Partial<Room>, label = 'Updated cell details.') => commit(label, p => M.updateCell(p, cell.id, fields), true);
    const inside = project.people.filter(p => [p.home, p.work, p.evening].some(x => x.cell === cell.id));
    const doors = project.links.filter(l => l.a.cell === cell.id || l.b.cell === cell.id);
    const viewing = useStore(s => s.view.kind === 'cell' && s.view.id === cell.id);
    return (<>
        <Header eyebrow={room ? 'INTERIOR' : 'WORLD CELL'} title={cell.name} onClose={pinned ? undefined : close}>
            {!viewing && <button className="primary" onClick={() => openView({kind: 'cell', id: cell.id})}>Open ↗</button>}
        </Header>
        <Section title="Details">
            <TextField label="Name" value={cell.name} max={120} onCommit={v => update({name: v})} />
            <TextField label="Scene description" multiline value={cell.description} max={4000} placeholder="What a wolf notices on arrival…" onCommit={v => update({description: v})} />
            <Row>
                <NumberField label="Level (Z)" value={cell.z} min={-16} max={16} onCommit={v => update({z: v})} />
                <SelectField label="Weather" value={cell.weather} onChange={v => update({weather: v})}
                    options={M.WEATHERS.map(w => ({value: w, label: w[0].toUpperCase() + w.slice(1)}))} />
            </Row>
            <Toggle label="Outdoors" hint="Exposed to weather, wind and sky light" checked={cell.outdoors} onChange={v => update({outdoors: v})} />
            <p className="meta">{cell.width} × {cell.height} tiles · id <code>{cell.id}</code></p>
        </Section>
        <Section title="Lighting" open={!cell.outdoors}>
            <Slider label="Lamps & firelight" value={cell.lighting.artificial} format={v => `${Math.round(v * 100)}%`} onCommit={v => update({lighting: {...cell.lighting, artificial: v}})} />
            <Slider label="Daylight through windows" value={cell.lighting.daylightAccess} format={v => `${Math.round(v * 100)}%`} onCommit={v => update({lighting: {...cell.lighting, daylightAccess: v}})} />
            <SelectField label="Light tone" value={cell.lighting.tone} onChange={v => update({lighting: {...cell.lighting, tone: v}})}
                options={[{value: 'warm', label: 'Warm · firelight'}, {value: 'neutral', label: 'Neutral'}, {value: 'cool', label: 'Cool'}]} />
            <Hint>Both at 0% makes a dark cellar. Outdoor cells keep these values but use the sky in play.</Hint>
        </Section>
        {room && <Section title="Interior size & overview" open={pinned}>
            <RoomSize room={room} />
            <Row>
                <NumberField label="Overview X" value={room.worldX} step={1} onCommit={v => update({worldX: v})} />
                <NumberField label="Overview Y" value={room.worldY} step={1} onCommit={v => update({worldY: v})} />
            </Row>
            <Hint>Resizing keeps the top-left corner fixed; new space is floor, walled along the new edge. Overview position only affects where the room appears on the in-game world map.</Hint>
        </Section>}
        <Section title={`Connections (${doors.length})`}>
            {doors.length ? doors.map(l => {
                const other = l.a.cell === cell.id ? l.b : l.a;
                return <button key={l.id} className="list-item" onClick={() => setState({selection: {kind: 'link', id: l.id}})}>
                    <span className="badge">{l.kind === 'stairs' ? '^' : l.kind === 'passage' ? '≡' : '+'}</span>{l.name}<small>→ {M.getCell(project, other.cell)?.name}</small></button>;
            }) : <Hint>No doors yet. Use the Connect tool (D) to link this place to another.</Hint>}
            <button onClick={() => setState({tool: 'door', pending: {kind: 'link', a: null, linkKind: 'door'}})}>+ Connect with a door…</button>
        </Section>
        <Section title={`Residents here (${inside.length})`}>
            {inside.map(p => <button key={p.id} className="list-item" onClick={() => setState({selection: {kind: 'person', id: p.id}})}>
                <span className="badge" style={{color: ROLE_INFO[p.role].color}}>{ROLE_INFO[p.role].icon}</span>{p.name}
                <small>{[p.home.cell === cell.id && 'lives', p.work.cell === cell.id && 'works', p.evening.cell === cell.id && 'evenings'].filter(Boolean).join(' · ')}</small></button>)}
            <Hint>To move someone in, select them and use Pick next to Home or Work. Or right-click a tile here to add a new resident.</Hint>
        </Section>
        <Section title="Territory" open={false}><TerritoryEditor ids={[cell.id]} territory={cell.territory} /></Section>
        {room && <Section title="Danger zone" open={false}>
            <button className="danger wide" onClick={() => {
                if (window.confirm(`Delete ${cell.name} and every door leading to it?`) && commit('Deleted interior.', p => M.removeRoom(p, cell.id)))
                    setState(s => ({selection: null, view: s.view.kind === 'cell' && s.view.id === cell.id ? {kind: 'world'} : s.view}));
            }}>Delete this interior</button>
        </Section>}
        {!room && <Section title="Danger zone" open={false}>
            <button className="danger wide" onClick={() => deleteCell(cell.id)}>Delete this cell</button>
        </Section>}
        {!room && <Section title="Split" open={false}>
            <Hint>Split this cell into two along its width or height. Terrain never moves.</Hint>
            <button onClick={() => setState({dialog: 'split'})}>Split cell…</button>
        </Section>}
    </>);
}

function RoomSize({room}: {room: Room}) {
    return <Row>
        <NumberField label="Width" value={room.width} min={4} max={256} onCommit={v => commit('Resized interior.', p => M.resizeRoom(p, room.id, v, room.height))} />
        <NumberField label="Height" value={room.height} min={4} max={256} onCommit={v => commit('Resized interior.', p => M.resizeRoom(p, room.id, room.width, v))} />
    </Row>;
}

function TerritoryEditor({ids, territory}: {ids: string[]; territory: M.Territory}) {
    const project = useStore(s => s.project);
    const set = (t: M.Territory) => commit('Updated territory.', p => M.setTerritory(p, ids, t), true);
    return (<>
        <TextField label="Region ID" value={territory.region} max={48} hint="lowercase letters, digits, _ or -" onCommit={v => set({...territory, region: v})} />
        <SelectField label="Chapter site" value={territory.chapter} onChange={v => set({...territory, chapter: v})}
            options={[{value: '', label: 'None'}, ...project.chapters.map(c => ({value: c.id, label: c.name}))]} />
        <div className="field"><span>Faction claims</span>
            {project.factions.length ? project.factions.map(f => <Toggle key={f.id} label={f.name} checked={territory.claims.includes(f.id)}
                onChange={on => set({...territory, claims: on ? [...territory.claims, f.id] : territory.claims.filter(c => c !== f.id)})} />)
                : <Hint>No factions yet. Add some under World → Factions & Chapters.</Hint>}
        </div>
    </>);
}

// --------------------------------------------------------------------------- People

function PersonInspector({project, person}: {project: Project; person: Person}) {
    const update = (fields: Partial<Person>, label = `Updated ${person.name}.`) =>
        commit(label, p => M.upsertPerson(p, {...person, ...fields}), true);
    const look = person.appearance;
    const setLook = (fields: Partial<M.Appearance>) => update({appearance: {...look, ...fields}});
    const info = ROLE_INFO[person.role];
    return (<>
        <Header eyebrow={info.label.toUpperCase()} title={person.name} onClose={close}>
            <span className="role-chip" style={{borderColor: info.color, color: info.color}}>{info.icon}</span>
        </Header>
        <Section title="Identity">
            <Row>
                <TextField label="Name" value={person.name} max={120} onCommit={v => update({name: v})} />
                <NumberField label="Age" value={person.age} min={0} max={200} onCommit={v => update({age: v})} />
            </Row>
            <SelectField label="Role" value={person.role} hint={info.blurb}
                onChange={v => update({role: v, route: v === 'merchant' ? '' : person.route})} options={ROLES.map(r => ({value: r, label: ROLE_INFO[r].label}))} />
            <TextField label="Description" multiline value={person.description} max={4000} placeholder="What others see when they inspect this wolf." onCommit={v => update({description: v})} />
            <TextField label="Greeting" value={person.greeting} max={1024} placeholder="Said when a player talks to them offline." onCommit={v => update({greeting: v})} />
        </Section>
        <Section title="Character sheet (for live dialogue)" open={false}>
            <TextField label="Personality" multiline value={person.personality} max={2000} placeholder="Temperament, values, how they speak." onCommit={v => update({personality: v})} />
            <TextField label="Backstory" multiline value={person.backstory} max={6000} placeholder="Where they came from and what they want." onCommit={v => update({backstory: v})} />
            <Hint>Sent to the live-dialogue model so the NPC stays in character. The game remembers every conversation per player.</Hint>
        </Section>
        <Section title="Daily schedule">
            <Timeline role={person.role} hours={person.hours} route={person.route} workLabel={person.workLabel} />
            <Row>
                <NumberField label={person.role === 'guard' ? 'Watch starts' : person.role === 'merchant' ? 'Shop opens' : 'Work starts'} value={person.hours.start} min={0} max={23.75} step={0.25} suffix={formatHour(person.hours.start)}
                    onCommit={v => update({hours: {...person.hours, start: v}})} />
                <NumberField label={person.role === 'guard' ? 'Watch ends' : person.role === 'merchant' ? 'Shop closes' : 'Work ends'} value={person.hours.end} min={0} max={23.75} step={0.25} suffix={formatHour(person.hours.end)}
                    onCommit={v => update({hours: {...person.hours, end: v}})} />
            </Row>
            <TextField label="Activity label" value={person.workLabel} max={40} hint="Shown in game while they work, e.g. “minding a market stall”." onCommit={v => update({workLabel: v})} />
            {person.role === 'guard' && <SelectField label="Patrol route" value={person.route} onChange={v => update({route: v})}
                hint={person.route ? 'Walks these posts in order while on watch.' : 'Holds the work place as a fixed post while on watch.'}
                options={[{value: '', label: 'Hold work post'}, ...project.routes.map(r => ({value: r.id, label: `${r.name} (${r.posts.length} posts)`}))]} />}
            {person.role === 'civilian' && <SelectField label="Travel route" value={person.route} onChange={v => update({route: v})}
                hint={person.route ? 'A traveller: walks these posts in order through working hours, and rests on the road wherever the day ends.' : 'Works at the work place.'}
                options={[{value: '', label: 'Work at the work place'}, ...project.routes.map(r => ({value: r.id, label: `${r.name} (${r.posts.length} posts)`}))]} />}
            <Toggle label="Paid by the treasury" hint="Earns small wages for time spent working" checked={person.paid} onChange={v => update({paid: v})} />
        </Section>
        <Section title="Places">
            <PickButton refTo={{kind: 'person', id: person.id, slot: 'home'}} label="Home · sleeps here" place={person.home} />
            <PickButton refTo={{kind: 'person', id: person.id, slot: 'work'}} label={person.role === 'merchant' ? 'Counter · customers stand beside it' : person.role === 'guard' ? 'Post · used without a route' : 'Work place'} place={person.work} />
            <PickButton refTo={{kind: 'person', id: person.id, slot: 'evening'}} label="Evening place" place={person.evening} />
            <Hint>On the map: W is their work place; ⌂ home and ☾ evening appear while selected. Drag any of them to move it.</Hint>
        </Section>
        <Section title="Appearance">
            <div className="portrait-row">
                <Portrait appearance={look} age={person.age} size={120} />
                <div className="coat" style={{background: `linear-gradient(135deg, ${COATS[look.baseColor][1]} 45%, ${COATS[look.gradientColor][1]})`}}>
                    <i style={{background: COATS[look.markingColor][1]}} />
                </div>
            </div>
            <Row>
                <SelectField label="Species" value={look.species} onChange={v => setLook({species: v})} options={SPECIES.map(v => ({value: v, label: v[0].toUpperCase() + v.slice(1)}))} />
                <SelectField label="Sex" value={look.sex} onChange={v => setLook({sex: v})} options={SEXES.map(v => ({value: v, label: v}))} />
            </Row>
            <Row>
                <SelectField label="Stature" value={look.stature} onChange={v => setLook({stature: v})} options={STATURES.map(v => ({value: v, label: v}))} />
                <SelectField label="Markings" value={look.pattern} onChange={v => setLook({pattern: v})} options={PATTERNS.map(v => ({value: v, label: v}))} />
            </Row>
            {(['baseColor', 'gradientColor', 'markingColor'] as const).map(k => <div className="field" key={k}>
                <span>{{baseColor: 'Coat', gradientColor: 'Coat gradient', markingColor: 'Markings'}[k]} · {COATS[look[k]][0]}</span>
                <div className="swatches">{COATS.map(([name, hex], i) => <button key={name} title={name} aria-label={name} className={look[k] === i ? 'on' : ''} style={{background: hex}} onClick={() => setLook({[k]: i})} />)}</div>
            </div>)}
            <div className="field"><span>Voice color in chat</span>
                <div className="swatches small">{VOICE_COLORS.map((hex, i) => <button key={i} title={`Voice ${i}`} aria-label={`Voice color ${i}`} className={person.voice === i ? 'on' : ''} style={{background: hex}} onClick={() => update({voice: i})} />)}</div>
            </div>
        </Section>
        <Section title="Purse & goods" open={false}>
            <Row>
                <NumberField label="Silver" value={person.purse} min={0} max={100000} onCommit={v => update({purse: v})} />
                <NumberField label="Meals" value={person.meals} min={0} max={10000} onCommit={v => update({meals: v})} />
            </Row>
            <NumberField label="Herbs" value={person.herbs} min={0} max={10000} onCommit={v => update({herbs: v})} />
            <Hint>{person.role === 'merchant' ? 'A merchant\'s goods are their shop stock.' : 'Starting money and food. Hungry residents buy meals at the nearest open shop.'}</Hint>
        </Section>
        <Section title="Danger zone" open={false}>
            <button className="danger wide" onClick={() => { if (commit(`Removed ${person.name}.`, p => M.removePerson(p, person.id))) setState({selection: null}); }}>Remove {person.name}</button>
        </Section>
    </>);
}

/** 24-hour strip mirroring the engine's routine (RatwResidents.cpp). */
function Timeline({role, hours, route, workLabel}: {role: Role; hours: {start: number; end: number}; route: string; workLabel: string}) {
    const within = (h: number, a: number, b: number) => a <= b ? h >= a && h < b : h >= a || h < b;
    const {start, end} = hours;
    const slot = (h: number): [string, string] => {
        const on = within(h, start, end);
        if (role === 'guard') return on ? ['watch', route ? 'Patrol' : 'On post'] : ['sleep', 'Rest in bed'];
        if (role === 'merchant') return on ? ['work', 'Shop open'] : h < 6 || h >= 22 ? ['sleep', 'Asleep'] : ['evening', 'At home'];
        if (route) return on ? ['work', `${workLabel} (on the road)`] : h < 6 || h >= 22 ? ['sleep', 'Camped on the road'] : ['evening', 'Resting on the road'];
        if (h < 6 || h >= 22) return ['sleep', 'Asleep'];
        if (on) return ['work', workLabel];
        return within(h, end, 22) ? ['evening', 'Evening place'] : ['home', 'Home'];
    };
    const halfHours = Array.from({length: 48}, (_, i) => slot(i / 2));
    return (
        <div className="timeline" aria-label="Daily schedule">
            <div className="bar">{halfHours.map(([kind, label], i) => <i key={i} className={kind} title={`${formatHour(i / 2)} · ${label}`} />)}</div>
            <div className="ticks"><span>00</span><span>06</span><span>12</span><span>18</span><span>24</span></div>
            <div className="legend"><span className="work">work / shop</span><span className="watch">watch</span><span className="evening">evening</span><span className="sleep">sleep</span></div>
            <Hint>Anyone hungry takes a short break to buy food while a shop is open. Game days last four real hours.</Hint>
        </div>
    );
}

// --------------------------------------------------------------------------- Profession slots

function SlotInspector({project, slot}: {project: Project; slot: M.Slot}) {
    const roster = useStore(s => s.roster);
    const preview = useStore(s => s.preview);
    const profession = roster?.professions.find(p => p.id === slot.profession);
    const behavior = profession?.behavior ?? 'civilian';
    const update = (fields: Partial<M.Slot>, label = `Updated ${slot.name}.`) => commit(label, p => M.upsertSlot(p, {...slot, ...fields}), true);
    const entry = preview?.plan.find(e => e.slot === slot.id);
    const holder = entry && roster?.characters.find(c => c.id === entry.character);
    const refresh = async () => {
        try { setState({preview: await api.preview(getState().project)}); }
        catch (error) { toast((error as Error).message, 'error'); }
    };
    return (<>
        <Header eyebrow="PROFESSION SLOT" title={slot.name} onClose={close}><span className="role-chip" style={{borderColor: '#b6a3cf', color: '#b6a3cf'}}>◇</span></Header>
        <Section title="Filled by">
            {holder ? <div className="holder">
                <Portrait appearance={holder.appearance} age={holder.age} size={72} />
                <div><b>{holder.name}</b><small>{entry!.new ? 'Will be drawn at the next export' : 'Holds this job permanently'} · {holder.age} years</small>
                    <p>{holder.personality}</p></div>
            </div> : <Hint>{preview ? 'Nobody suitable is free. Add or generate characters who would take this profession.' : 'Check who holds or would be drawn for this job.'}</Hint>}
            {preview?.warnings.filter(w => w.includes(slot.name)).map((w, i) => <p key={i} className="hint warn">{w}</p>)}
            <div className="button-grid two">
                <button onClick={refresh}>Check roster</button>
                {holder && <button onClick={() => { setWorkspace('characters'); setState({character: holder.id}); }}>Open character ↗</button>}
            </div>
            <Hint>At export, an empty slot takes the free character who most prefers this profession. From then on they keep this job until they die or are removed, and never appear anywhere else.</Hint>
        </Section>
        <Section title="Job">
            <TextField label="Slot name" value={slot.name} max={120} hint="For you, e.g. “North gate guard”." onCommit={v => update({name: v})} />
            <SelectField label="Profession" value={slot.profession} onChange={v => update({profession: v, route: roster?.professions.find(p => p.id === v)?.behavior === 'guard' ? slot.route : ''})}
                hint={profession ? `${BEHAVIOR_LABELS[profession.behavior]} behavior. ${profession.description}` : 'Unknown profession: add it in Characters.'}
                options={[...(profession ? [] : [{value: slot.profession, label: `${slot.profession} (missing)`}]), ...(roster?.professions ?? []).map(p => ({value: p.id, label: p.name}))]} />
            <TextField label="Activity label" value={slot.workLabel} max={40} placeholder={profession?.workLabel ?? ''} hint="Leave empty to use the profession's label." onCommit={v => update({workLabel: v})} />
        </Section>
        <Section title="Daily schedule">
            <Timeline role={behavior} hours={slot.hours} route={slot.route} workLabel={slot.workLabel || profession?.workLabel || 'working'} />
            <Row>
                <NumberField label="Starts" value={slot.hours.start} min={0} max={23.75} step={0.25} suffix={formatHour(slot.hours.start)} onCommit={v => update({hours: {...slot.hours, start: v}})} />
                <NumberField label="Ends" value={slot.hours.end} min={0} max={23.75} step={0.25} suffix={formatHour(slot.hours.end)} onCommit={v => update({hours: {...slot.hours, end: v}})} />
            </Row>
            {behavior === 'guard' && <SelectField label="Patrol route" value={slot.route} onChange={v => update({route: v})}
                options={[{value: '', label: 'Hold work post'}, ...project.routes.map(r => ({value: r.id, label: `${r.name} (${r.posts.length} posts)`}))]} />}
            <Toggle label="Paid by the treasury" checked={slot.paid} onChange={v => update({paid: v})} />
        </Section>
        <Section title="Places">
            <PickButton refTo={{kind: 'slot', id: slot.id, slot: 'home'}} label="Home · sleeps here" place={slot.home} />
            <PickButton refTo={{kind: 'slot', id: slot.id, slot: 'work'}} label={behavior === 'merchant' ? 'Counter' : behavior === 'guard' ? 'Post' : 'Work place'} place={slot.work} />
            <PickButton refTo={{kind: 'slot', id: slot.id, slot: 'evening'}} label="Evening place" place={slot.evening} />
        </Section>
        <Section title="Starting purse" open={false}>
            <Row><NumberField label="Silver" value={slot.purse} min={0} max={100000} onCommit={v => update({purse: v})} />
                <NumberField label="Meals" value={slot.meals} min={0} max={10000} onCommit={v => update({meals: v})} /></Row>
            <NumberField label="Herbs" value={slot.herbs} min={0} max={10000} onCommit={v => update({herbs: v})} />
        </Section>
        <Section title="Danger zone" open={false}>
            <button className="danger wide" onClick={() => { if (commit('Removed profession slot; its character is released at the next export.', p => M.removeSlot(p, slot.id))) close(); }}>Remove slot</button>
            <Hint>The character who held it keeps their profession and can only fill another slot of the same kind.</Hint>
        </Section>
    </>);
}

// --------------------------------------------------------------------------- Links, routes, markers

function LinkInspector({project, link}: {project: Project; link: M.Link}) {
    const update = (fields: Partial<M.Link>) => commit('Updated connection.', p => M.updateLink(p, link.id, fields), true);
    const end = (side: 'a' | 'b') => {
        const place = link[side], c = M.getCell(project, place.cell);
        return <div className="place-row">
            <div><small>{side === 'a' ? 'First end' : 'Second end'}</small><span>{c?.name} · {place.x}, {place.y}</span></div>
            <button onClick={() => openView({kind: 'cell', id: place.cell})}>Go</button>
        </div>;
    };
    return (<>
        <Header eyebrow="CONNECTION" title={link.name} onClose={close} />
        <Section title="Details">
            <TextField label="Name" value={link.name} max={120} onCommit={v => update({name: v})} />
            <SelectField label="Kind" value={link.kind} onChange={v => update({kind: v})} hint={{door: 'Shows as + and opens/closes. Players click it to enter.', passage: 'An always-open gap.', stairs: 'Shows as ^; always open.'}[link.kind]}
                options={[{value: 'door', label: 'Door'}, {value: 'passage', label: 'Passage'}, {value: 'stairs', label: 'Stairs'}]} />
            {link.kind === 'door' && <Toggle label="Starts open" checked={link.open} onChange={v => update({open: v})} />}
            {end('a')}{end('b')}
            <Hint>Drag either end on the map to move it. Each end needs an open neighbouring tile to arrive on.</Hint>
        </Section>
        <Section title="Danger zone" open={false}>
            <button className="danger wide" onClick={() => { if (commit('Removed connection.', p => M.removeLink(p, link.id))) close(); }}>Remove connection</button>
        </Section>
    </>);
}

function RouteInspector({project, route}: {project: Project; route: M.Route}) {
    const save = (r: M.Route, label = 'Updated route.') => commit(label, p => M.upsertRoute(p, r), true);
    const guards = project.people.filter(p => p.route === route.id);
    const move = (i: number, d: number) => {
        const posts = [...route.posts]; [posts[i], posts[i + d]] = [posts[i + d], posts[i]]; save({...route, posts});
    };
    const tool = useStore(s => s.tool);
    return (<>
        <Header eyebrow="PATROL ROUTE" title={route.name} onClose={close} />
        <Section title="Route">
            <TextField label="Name" value={route.name} max={120} onCommit={v => save({...route, name: v})} />
            <button className={tool === 'post' ? 'primary wide' : 'wide'} onClick={() => setState({tool: tool === 'post' ? 'select' : 'post'})}>
                {tool === 'post' ? 'Done adding posts' : '+ Add posts by clicking the map'}</button>
            <Hint>Guards on this route move to the next post every 15 game minutes, spread out so they don't bunch up.</Hint>
        </Section>
        <Section title={`Posts (${route.posts.length})`}>
            <ol className="posts">{route.posts.map((post, i) => <li key={i}>
                <button className="link-button" onClick={() => focusPlace(post)}>{placeLabel(project, post)}</button>
                <span>
                    <button className="icon" disabled={i === 0} onClick={() => move(i, -1)} aria-label="Move earlier">↑</button>
                    <button className="icon" disabled={i === route.posts.length - 1} onClick={() => move(i, 1)} aria-label="Move later">↓</button>
                    <button className="icon" disabled={route.posts.length === 1} title={route.posts.length === 1 ? 'A route needs at least one post; delete the route instead' : 'Remove post'}
                        onClick={() => save({...route, posts: route.posts.filter((_, n) => n !== i)}, 'Removed post.')} aria-label="Remove post">×</button>
                </span>
            </li>)}</ol>
        </Section>
        <Section title={`Guards on this route (${guards.length})`}>
            {guards.map(g => <button key={g.id} className="list-item" onClick={() => setState({selection: {kind: 'person', id: g.id}})}><span className="badge">⛨</span>{g.name}<small>{formatHour(g.hours.start)}–{formatHour(g.hours.end)}</small></button>)}
            {project.people.filter(p => p.role === 'guard' && p.route !== route.id).length > 0 &&
                <SelectField label="Assign a guard" value="" onChange={id => { const g = project.people.find(p => p.id === id); if (g) commit(`${g.name} now walks ${route.name}.`, p => M.upsertPerson(p, {...g, route: route.id})); }}
                    options={[{value: '', label: 'Choose…'}, ...project.people.filter(p => p.role === 'guard' && p.route !== route.id).map(p => ({value: p.id, label: p.name}))]} />}
        </Section>
        <Section title="Danger zone" open={false}>
            <button className="danger wide" onClick={() => { if (commit('Removed route; its guards now hold their work posts.', p => M.removeRoute(p, route.id))) close(); }}>Delete route</button>
        </Section>
    </>);
}

function MarkerInspector({project, kind}: {project: Project; kind: 'spawn' | 'herb'}) {
    const spawn = kind === 'spawn';
    return (<>
        <Header eyebrow="MARKER" title={spawn ? 'Player spawn' : 'Herb patch'} onClose={close} />
        <Section title="Placement">
            <PickButton refTo={spawn ? {kind: 'spawn'} : {kind: 'herbPatch'}} label={spawn ? 'New characters appear here' : 'Players gather herbs within reach'} place={spawn ? project.spawn : project.herbPatch} />
            <Hint>{spawn ? 'Every world needs exactly one spawn on open ground.' : 'The patch regrows each day, faster in spring. A world may have one herb patch.'}</Hint>
        </Section>
    </>);
}
