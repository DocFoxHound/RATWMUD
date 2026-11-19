import {useState} from 'react';
import type {Project} from '../model/model.mjs';
import type {Npcs} from './api';

export function StoryPicker({world, roster, resident, place, onResident, onPlace, disabled = false}: {
    world: Project | null; roster: Npcs | null; resident: string; place: string;
    onResident: (id: string) => void; onPlace: (id: string) => void; disabled?: boolean;
}) {
    const [findWolf, setFindWolf] = useState(''), [findPlace, setFindPlace] = useState('');
    const matches = (p: {id: string; name: string}, query: string) => (p.name + ' ' + p.id).toLowerCase().includes(query.toLowerCase().trim());
    const locations = [...(world?.cells ?? []), ...(world?.rooms ?? [])].sort((a,b) => a.name.localeCompare(b.name));
    const people = [...new Map([...(world?.people ?? []), ...(roster?.people ?? []), ...(roster?.holders ?? [])].map(p => [p.id, p])).values()]
        .sort((a,b) => a.name.localeCompare(b.name));
    const selected = locations.find(p => p.id === place), wolf = people.find(p => p.id === resident);
    return <div className="story-pickers">
        <label>Find a resident<input type="search" value={findWolf} disabled={disabled} onChange={e => setFindWolf(e.target.value)} /></label>
        <label>Resident<select aria-label="Resident" value={resident} disabled={disabled} onChange={e => onResident(e.target.value)}>
            <option value="">Choose a resident</option>
            {resident && !wolf && <option value={resident}>{resident} · outside this roster</option>}
            {people.filter(p => p.id === resident || matches(p, findWolf)).map(p => <option value={p.id} key={p.id}>{p.name} · {p.id}{p.home ? '' : ' · unplaced'}</option>)}
        </select></label>
        <label>Find a location<input type="search" value={findPlace} disabled={disabled} onChange={e => setFindPlace(e.target.value)} /></label>
        <label>Location<select aria-label="Location" value={place} disabled={disabled} onChange={e => onPlace(e.target.value)}>
            <option value="">Choose a location</option>
            {place && !selected && <option value={place}>{place} · outside this atlas</option>}
            {locations.filter(p => p.id === place || matches(p, findPlace)).map(p => <option value={p.id} key={p.id}>{p.name} · {p.id}</option>)}
        </select></label>
        {wolf && roster?.dead.includes(wolf.id) && <small>This resident is recorded as dead.</small>}
        {wolf && <small>{wolf.home ? 'Home · ' + (locations.find(p => p.id === wolf.home?.cell)?.name ?? wolf.home.cell) : 'This resident has no home placement.'}</small>}
        {selected && <small>{selected.name} · {selected.outdoors ? 'Outdoors' : 'Indoors'} · level {selected.z}{selected.description && ' · ' + selected.description}</small>}
    </div>;
}
