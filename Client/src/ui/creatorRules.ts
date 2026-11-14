// The character creator's rules, apart from its page (frontDoor.ts), so they can be tested: the name's length, the fur
// zones (each wears the coat's colours until the player changes one of its three, and Reset gives it the coat's again),
// and the Gift's three tiers, each open or greyed with the reason (the user, 2026-11-14).
import {type Fur, type FurCoat} from './fur.ts';
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

/** The three colours a zone wears: its own, else the coat's. */
export function zoneCoat(fur: Fur, zone: string): FurCoat {
    return [...(fur.zones?.[zone] ?? fur.coat)] as FurCoat;
}

/** Whether a zone wears colours of its own (not the coat's). */
export function zoneChanged(fur: Fur, zone: string): boolean {
    const own = fur.zones?.[zone];
    return !!own && own.some((c, i) => c !== fur.coat[i]);
}

function withZones(fur: Fur, zones: Record<string, FurCoat>): Fur {
    const kept = Object.entries(zones).filter(([, coat]) => coat.some((c, i) => c !== fur.coat[i]));
    const out: Fur = {coat: [...fur.coat] as FurCoat, eyes: fur.eyes};
    if (kept.length) out.zones = Object.fromEntries(kept.map(([zone, coat]) => [zone, [...coat] as FurCoat]));
    return out;
}

/** One of a zone's three colours (0 dark, 1 mid, 2 light) changed: the zone starts from what it wears (the coat's,
 *  unless it has its own). A zone that comes back to the coat's three is the coat's again (nothing kept). */
export function setZoneColour(fur: Fur, zone: string, slot: number, colour: string): Fur {
    const next = zoneCoat(fur, zone);
    next[Math.min(2, Math.max(0, slot))] = colour;
    return withZones(fur, {...(fur.zones ?? {}), [zone]: next});
}

/** The zone's Reset: the coat's scheme again. */
export function resetZone(fur: Fur, zone: string): Fur {
    const zones = {...(fur.zones ?? {})};
    delete zones[zone];
    return withZones(fur, zones);
}

/** A Gift tier as the creator shows it: open, or greyed with why. */
export interface TierCard {
    tier: 'normal' | 'gifted' | 'quickened';
    open: boolean;
    reason: string;          // Why it is greyed ('' when open).
    progress: string[];      // What the account has toward it, line by line (a world's own standing, when it sends it).
}

const Tiers = ['normal', 'gifted', 'quickened'] as const;

/** What a world's standing asks before Quickened opens there (doc 49; standing.json's "unlocks", sent in central's rules). */
export function quickenedNeeds(unlocks: Json | null): string {
    const q = obj(unlocks, 'quickened');
    return `Gifted open, social level ${num(q, 'level', 8)}, ${num(q, 'stars', 100)} stars from ${num(q, 'givers', 30)} different wolves ` +
        `and ${num(q, 'stories', 2)} closed Stories`;
}

/** The three tiers, by the game's unlock rules (doc 49; doc 77, Q12 (a); Core/RatwGame.cpp, arrivalChoices):
 *   - on a world (a browser on a password world): Gifted and Quickened open as the account's standing there has opened
 *     them (`tiers`, the lobby's tiersView); Quickened needs Gifted too; an older server that sends none: all open.
 *   - on central (the desktop client): the tier is each world's, at the first arrival; a wolf with a Gift family arrives
 *     Gifted anywhere, so Gifted is open to every account; Quickened opens only in a world whose standing has opened it,
 *     so it is never chosen here (greyed, with what a world asks). */
export function tierCards(mode: 'world' | 'central', tiers: Json | null, unlocks: Json | null = null): TierCard[] {
    return Tiers.map(tier => {
        if (tier === 'normal') return {tier, open: true, reason: '', progress: []};
        if (mode === 'central') {
            if (tier === 'gifted') return {tier, open: true, reason: '', progress: []};
            return {tier, open: false, progress: [],
                reason: `Quickened opens in each world on its own, once your account there has ${quickenedNeeds(unlocks)}. ` +
                    'Give your wolf a Gift family: in a world that has opened it, they may arrive Quickened.'};
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
