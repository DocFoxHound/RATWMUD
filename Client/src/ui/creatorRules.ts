// The character creator's rules, apart from its page (frontDoor.ts), so they can be tested: the name's length, the
// fur layers & legacy zones (each inherits from the coat/claws until changed, and Reset restores it), and the Gift's
// three tiers, each open or greyed with the reason (the user, 2026-11-14).
import {baseLayerColour, coatThree, layerColour, type Fur, type FurCoat3} from './fur.ts';
import {arr, isObject, num, obj, str, type Json} from '../game/json.ts';

/** A new wolf's name is 2 to 24 characters (Core/RatwIdentity.h, identity::NewNameMost; central keeps it too). Counted
 *  as the server counts them: UTF-16 code units, a string's length. */
export const NameLeast = 2;
export const NameMost = 24;

/** '' when a name (already trimmed) may be asked for; otherwise why not. */
export function nameProblem(name: string, most = NameMost): string {
    if (name.length < NameLeast || name.length > most) return `Choose a name of ${NameLeast} to ${most} characters.`;
    return '';
}

/** Clone a Fur cleanly, omitting empty zones/layers maps. */
function cleanFur(fur: Fur): Fur {
    const out: Fur = {coat: [...fur.coat] as Fur['coat'], eyes: fur.eyes};
    if (fur.claws) out.claws = fur.claws;
    if (fur.zones && Object.keys(fur.zones).length) {
        out.zones = Object.fromEntries(Object.entries(fur.zones).map(([z, c]) => [z, [...c] as FurCoat3]));
    }
    if (fur.layers && Object.keys(fur.layers).length) {
        out.layers = {...fur.layers};
    }
    return out;
}

// ---------------------------------------------------------------------------------------------- Per-layer markings (45 doll layers)

/** Whether a doll layer wears a colour override of its own (different from what it inherits from the coat/claws). */
export function layerChanged(fur: Fur, layerId: string): boolean {
    const own = fur.layers?.[layerId];
    return !!own && own !== baseLayerColour(fur, layerId);
}

/** Change one doll layer's colour. Setting it back to its inherited coat/claws colour drops the override. */
export function setLayerColour(fur: Fur, layerId: string, colour: string): Fur {
    const layers = {...(fur.layers ?? {})};
    if (colour === baseLayerColour(fur, layerId)) delete layers[layerId];
    else layers[layerId] = colour;
    return cleanFur({...fur, layers});
}

/** Reset one doll layer back to its inherited coat/claws colour. */
export function resetLayer(fur: Fur, layerId: string): Fur {
    if (!fur.layers || !(layerId in fur.layers)) return cleanFur(fur);
    const layers = {...fur.layers};
    delete layers[layerId];
    return cleanFur({...fur, layers});
}

/** Reset every layer override in a list of layer ids (e.g. an entire body group or all layers). */
export function resetLayers(fur: Fur, layerIds?: readonly string[]): Fur {
    if (!fur.layers) return cleanFur(fur);
    if (!layerIds) return cleanFur({...fur, layers: undefined});
    const layers = {...fur.layers};
    for (const id of layerIds) delete layers[id];
    return cleanFur({...fur, layers});
}

/** Read a doll layer's current colour id (override if set, else inherited). */
export function currentLayerColour(fur: Fur, layerId: string): string {
    return layerColour(fur, layerId);
}

// ---------------------------------------------------------------------------------------------- Legacy zone helpers (kept for older tests/compatibility)

/** The three colours a legacy zone wears: its own, else the coat's 3-shade summary. */
export function zoneCoat(fur: Fur, zone: string): FurCoat3 {
    const base = coatThree(fur.coat);
    return [...(fur.zones?.[zone] ?? base)] as FurCoat3;
}

/** Whether a legacy zone wears colours of its own (not the coat's). */
export function zoneChanged(fur: Fur, zone: string): boolean {
    const own = fur.zones?.[zone];
    const base = coatThree(fur.coat);
    return !!own && own.some((c, i) => c !== base[i]);
}

function withZones(fur: Fur, zones: Record<string, FurCoat3>): Fur {
    const base = coatThree(fur.coat);
    const kept = Object.entries(zones).filter(([, coat]) => coat.some((c, i) => c !== base[i]));
    return cleanFur({...fur, zones: kept.length ? Object.fromEntries(kept) : undefined});
}

/** One of a legacy zone's three colours (0 dark, 1 mid, 2 light) changed. */
export function setZoneColour(fur: Fur, zone: string, slot: number, colour: string): Fur {
    const next = zoneCoat(fur, zone);
    next[Math.min(2, Math.max(0, slot))] = colour;
    return withZones(fur, {...(fur.zones ?? {}), [zone]: next});
}

/** A legacy zone's Reset: back to the coat's scheme. */
export function resetZone(fur: Fur, zone: string): Fur {
    const zones = {...(fur.zones ?? {})};
    delete zones[zone];
    return withZones(fur, zones);
}

// ---------------------------------------------------------------------------------------------- Gift tiers

/** A Gift tier as the creator shows it: open, or greyed with why. */
export interface TierCard {
    tier: 'normal' | 'gifted' | 'quickened';
    open: boolean;
    reason: string;          // Why it is greyed ('' when open).
    progress: string[];      // What the account has toward it, line by line (a world's own standing, when it sends it).
}

const Tiers = ['normal', 'gifted', 'quickened'] as const;

/** What an official world's standing asks before Quickened opens for the account (doc 49; standing.json's "unlocks",
 *  sent in central's rules). */
export function quickenedNeeds(unlocks: Json | null): string {
    const q = obj(unlocks, 'quickened');
    return `Gifted open, social level ${num(q, 'level', 8)}, ${num(q, 'stars', 100)} stars from ${num(q, 'givers', 30)} different wolves ` +
        `and ${num(q, 'stories', 2)} closed Stories`;
}

/** The three tiers, by the game's unlock rules (doc 49; doc 77, Q12 (a); Core/RatwGame.cpp, arrivalChoices). */
export function tierCards(mode: 'world' | 'central', tiers: Json | null, unlocks: Json | null = null, quickened = false): TierCard[] {
    return Tiers.map(tier => {
        if (tier === 'normal') return {tier, open: true, reason: '', progress: []};
        if (mode === 'central') {
            if (tier === 'gifted' || quickened) return {tier, open: true, reason: '', progress: []};
            return {tier, open: false, progress: [],
                reason: `Quickened opens for your whole account, in every world, once it has ${quickenedNeeds(unlocks)} ` +
                    'in an official world. Then you can make Quickened wolves here; until then, a wolf with a Gift family is Gifted.'};
        }
        const t = obj(tiers, tier);
        const lines = (x: Json | null) => arr(x, 'progress').filter(isObject).map(p => str(p, 'label')).filter(Boolean);
        const giftedShut = obj(tiers, 'gifted')?.open === false;
        if (t?.open === false || (tier === 'quickened' && giftedShut)) {
            const blocker = t?.open === false ? t : obj(tiers, 'gifted');
            const word = tier === 'gifted' ? 'Gifted' : 'Quickened';
            return {tier, open: false, progress: lines(blocker),
                reason: str(blocker, 'message', `${word} isn't open to your account yet.`)};
        }
        return {tier, open: true, reason: '', progress: []};
    });
}
