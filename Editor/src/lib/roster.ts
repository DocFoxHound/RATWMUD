// The shared character roster (Data/Characters/roster.json), served by the local host.
import type {Appearance} from '../model/model.mjs';

export type Behavior = 'merchant' | 'guard' | 'civilian';
export interface Profession {
    id: string; name: string; behavior: Behavior; workLabel: string; description: string;
    hours: {start: number; end: number}; paid: boolean;
}
export interface Character {
    id: string; name: string; age: number; appearance: Appearance; voice: number;
    description: string; personality: string; traits: string[]; backstory: string; greeting: string;
    /** Profession ID → 0 never, 1 would accept, 2 good fit, 3 ideal. */
    preferences: Record<string, number>;
    status: 'active' | 'dead' | 'removed';
    /** Locked for life once an export assigns them; set only by exports. */
    profession: string;
    assignment: {world: string; slot: string} | null;
    origin: 'manual' | 'llm';
}
export interface Roster { format: 'ratw-roster'; version: 1; professions: Profession[]; characters: Character[] }
export interface SlotPreview { plan: {slot: string; character: string; name: string; new: boolean}[]; warnings: string[] }

export const PREFERENCE_LABELS = ['Never', 'Would accept', 'Good fit', 'Ideal'];
export const BEHAVIOR_LABELS: Record<Behavior, string> = {merchant: 'Shopkeeping', guard: 'Guarding', civilian: 'Working'};

const slug = (s: string) => (s.toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/^_+|_+$/g, '') || 'wolf').replace(/^[^a-z]/, 'c_$&').slice(0, 40);
export function freshCharacterId(roster: Roster, name: string) {
    const used = new Set(roster.characters.map(c => c.id)), base = slug(name);
    if (!used.has(base)) return base;
    for (let i = 2; ; i++) if (!used.has(`${base}_${i}`)) return `${base}_${i}`;
}
export function newCharacter(roster: Roster, name = 'New character'): Character {
    return {id: freshCharacterId(roster, name), name, age: 30, voice: roster.characters.length % 32,
        appearance: {species: 'timber', sex: 'female', stature: 'average', pattern: 'saddle', baseColor: 3, gradientColor: 1, markingColor: 5},
        description: '', personality: '', traits: [], backstory: '', greeting: '', preferences: {},
        status: 'active', profession: '', assignment: null, origin: 'manual'};
}
