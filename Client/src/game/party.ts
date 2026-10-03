// Parties (Docs/Design/32-parties-chapters-factions.md, Part 2) as the page receives them: the player's party, an
// invitation waiting, a party mate's fight calling them in. The rules are the server's (Core/RatwParty.cpp,
// Core/RatwGameParty.cpp); the page only shows them and sends the player's choices.
import {arr, bool, num, obj, str, type Json} from './json.ts';

export interface PartyMember {
    id: string;
    name: string;
    leader: boolean;
    online: boolean;
    cell: string;               // Where they are ('' when out of the world).
    place: string;
    x: number;
    y: number;
    health: number;
    downed: boolean;
    fighting: boolean;
    npc: boolean;               // A resident travelling with the party (Phase 3).
    reason: string;             // Why a resident came: "friend", "hired", "story".
    wage: number;               // Pennies a game day, if hired.
    waiting: boolean;           // Told to wait where they are.
    mine: boolean;              // This player may give them orders.
}

export interface PartyView {
    id: string;                 // '' when in no party (an invitation or a setting may still be shown).
    leader: string;
    members: PartyMember[];
    invite: {from: string; name: string; seconds: number} | null;
    pull: {name: string; seconds: number} | null;
    autoJoin: boolean;
    goal: string;               // Where the leader says the party is bound.
}

/** The party from the own wolf's snapshot entry (`self.party`), or null when there is nothing to show. */
export function readParty(self: Json | null): PartyView | null {
    const p = obj(self, 'party');
    if (!p) return null;
    const invite = obj(p, 'invite'), pull = obj(p, 'pull');
    return {
        id: str(p, 'id'),
        leader: str(p, 'leader'),
        members: arr(p, 'members').filter((m): m is Json => !!m && typeof m === 'object' && !Array.isArray(m)).map(m => ({
            id: str(m, 'id'), name: str(m, 'name'), leader: bool(m, 'leader'), online: bool(m, 'online'), cell: str(m, 'cell'),
            place: str(m, 'place'), x: num(m, 'x'), y: num(m, 'y'), health: num(m, 'health', 100), downed: bool(m, 'downed'),
            fighting: bool(m, 'fighting'), npc: bool(m, 'npc'), reason: str(m, 'reason'), wage: num(m, 'wage'),
            waiting: bool(m, 'waiting'), mine: bool(m, 'mine'),
        })),
        invite: invite ? {from: str(invite, 'from'), name: str(invite, 'name'), seconds: num(invite, 'seconds')} : null,
        pull: pull ? {name: str(pull, 'name'), seconds: num(pull, 'seconds')} : null,
        autoJoin: p.autoJoin !== false,
        goal: str(p, 'goal'),
    };
}

/** In a party (not merely invited to one). */
export function inParty(p: PartyView | null): boolean {
    return !!p && !!p.id && p.members.length > 1;
}

/** In a party with another player (party chat needs someone to hear it out of character). */
export function withPlayers(p: PartyView | null): boolean {
    return inParty(p) && p!.members.filter(m => !m.npc).length > 1;
}
