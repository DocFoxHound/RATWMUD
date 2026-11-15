// The wheel a right-click opens (ui/hud/hud.ts draws it): two rings, the user's notes of 2026-11-15. The first ring is
// the kinds of thing one can do with this wolf, door or ground (Trade, Social, Work...), only those with something in
// them; choosing one opens its actions. A kind with a single action does it at once, and a wheel with a single kind
// shows its actions straight away. Every entry reads as plain words: an action that carries an id ("take k460") takes
// the server's words for it, and the same thing twice is offered once.
import type {GameState} from './state.ts';
import {GroundTarget} from './state.ts';
import {Commands} from './commands.ts';
import {arr, isObject, obj, str} from './json.ts';

/** The kinds, in the order they go round the first ring. */
export const WheelGroups = [
    {id: 'trade', words: 'Trade'},
    {id: 'social', words: 'Social'},
    {id: 'party', words: 'Party'},
    {id: 'work', words: 'Work'},
    {id: 'items', words: 'Items'},
    {id: 'chapter', words: 'Chapter'},
    {id: 'action', words: 'Action'},
    {id: 'senses', words: 'Senses'},
    {id: 'rest', words: 'Rest'},
    {id: 'wild', words: 'Wild'},
    {id: 'look', words: 'Look'},
] as const;
export type WheelGroup = typeof WheelGroups[number]['id'];

/** Shown flat, in the order given: a list one action opened (a challenge's terms, whom to vouch for). */
export const FlatWheel = '*';

const Exact: Record<string, WheelGroup> = {
    trade: 'trade', 'pay fine': 'action', pay: 'action',
    talk: 'social', introduce: 'social', groom: 'social', 'ask to lodge': 'social', vouch: 'social',
    'tell the wardens': 'social', 'vouch to the wardens': 'social',
    invite: 'party', 'ask to join': 'party', 'follow me': 'party', 'wait here': 'party', 'go home': 'party', dismiss: 'party',
    'ask for work': 'work', apprentice: 'work', 'lend a paw': 'work', 'ask to lend a paw': 'work', 'ask for missions': 'work',
    give: 'items', lend: 'items',
    'invite to chapter': 'chapter', 'mark hostile': 'chapter', 'unmark hostile': 'chapter', 'let in': 'chapter',
    'no longer let in': 'chapter', 'propose a treaty': 'chapter', 'ask to be recognised as a House': 'chapter',
    'ask about our standing': 'chapter', 'tell of an expulsion': 'chapter', 'settle at the Hold': 'chapter',
    'ask to swear to the Chapter': 'chapter',
    steal: 'action', attack: 'action', challenge: 'action', tend: 'action', report: 'action', 'ask to spar': 'action',
    inspect: 'look',
};
// The ground's commands (commands.ts), by kind.
const Ground: Record<string, WheelGroup> = {
    listen: 'senses', look: 'senses', smell: 'senses', track: 'senses',
    wait: 'rest', sit: 'rest', lay: 'rest', stand: 'rest', groom: 'rest',
    forage: 'wild', hunt: 'wild', giveup: 'wild',
    howl: 'social', perform: 'social',
    practise: 'work', board: 'work', archive: 'work', copy: 'work',
};

/** The kind an action belongs to (anything unknown is an Action: a door's open, knock and lock among them). */
export function groupOf(action: string, ground = false): WheelGroup {
    if (ground) return Ground[action] ?? 'action';
    if (Exact[action]) return Exact[action];
    if (/^hire for /.test(action)) return 'party';
    if (/^(take|deliver) /.test(action) || action.startsWith('trouble:') || /^help /.test(action)) return 'work';
    if (/^introduce as /.test(action)) return 'social';
    if (/^(pay for a report|give a tithe)/.test(action)) return 'chapter';
    if (action.startsWith('challenge:') || action.startsWith('vouch:') || action.startsWith('wardens-')) return 'action';
    return 'action';
}

const MenuWords: Record<string, string> = {
    'challenge:yield': 'Duel until one yields', 'challenge:blood': 'Duel to first blood', 'challenge:spar': 'Spar (bruises only)',
    'challenge:death': 'Fight until one goes down', trade: 'Buy and sell', vouch: 'Vouch for…', introduce: 'Introduce yourself',
};

const upperFirst = (text: string) => text ? text[0].toUpperCase() + text.slice(1) : text;
/** An id in an action ("k460", "m12", "npc_ash"): never shown as words. */
const IdLike = /(^|\s)([a-z]{1,4}\d+|[a-z]+_[a-z0-9_]+)(\s|$)/;

/** "Tell the Wardens of Ash" / "Vouch for Ash to the Wardens" (doc 53, 4), by the names the wolf knows them by. */
function wardensWords(s: GameState, action: string): string {
    const [verb, id] = action.slice('wardens-'.length).split(':');
    const seen = arr(obj(s.snapshot, 'self'), 'witnessed').filter(isObject).find(w => str(w, 'id') === id);
    const name = seen ? str(seen, 'name') : 'them';
    return verb === 'tell' ? `Tell the Wardens of ${name}` : `Vouch for ${name} to the Wardens`;
}

/** An action in plain words, as the wheel shows it. */
export function actionWords(s: GameState, action: string): string {
    if (s.contextTarget === GroundTarget) return Commands.find(c => c.name === action)?.wheel ?? upperFirst(action);
    const given = s.entities.get(s.contextTarget)?.words?.[action];
    if (given) return given;
    if (action.startsWith('vouch:')) return `Vouch for ${s.entities.get(action.slice(6))?.name ?? 'them'}`;
    if (action.startsWith('wardens-')) return wardensWords(s, action);
    if (action.startsWith('trouble:')) return action.slice(action.indexOf('|') + 1);   // (As the server words it: doc 57.)
    if (MenuWords[action]) return MenuWords[action];
    // An id the server gave no words for (an older server): the verb alone, never the id.
    if (action.startsWith('take ')) return 'Take on work';
    if (action.startsWith('deliver ')) return 'Deliver';
    if (IdLike.test(action)) return upperFirst(action.split(' ')[0]);
    return upperFirst(action);
}

/** The actions on the wheel now, each once: one already shown in the same words goes, and "Ask for work" goes beside
 *  the work itself (each piece of it is on the wheel, by name). */
export function wheelActions(s: GameState): string[] {
    const all = [...new Set(s.contextActions)];
    const work = all.some(a => a.startsWith('take '));
    const seen = new Set<string>();
    return all.filter(a => {
        if (work && a === 'ask for work') return false;
        const words = actionWords(s, a).toLowerCase();
        if (seen.has(words)) return false;
        seen.add(words);
        return true;
    });
}

export interface WheelEntry {
    kind: 'group' | 'action';
    id: string;                 // The group's id, or the action.
    words: string;
    hint: string;               // A group's one action in words, or how many it holds.
    actions: string[];          // A group's actions (an action's: itself).
}

/** The kinds on the first ring: those with something in them, in WheelGroups' order. */
export function wheelGroups(s: GameState): WheelEntry[] {
    const ground = s.contextTarget === GroundTarget;
    const by = new Map<string, string[]>();
    for (const a of wheelActions(s)) {
        const g = groupOf(a, ground);
        by.set(g, [...by.get(g) ?? [], a]);
    }
    return WheelGroups.filter(g => by.has(g.id)).map(g => {
        const actions = by.get(g.id)!;
        return {kind: 'group', id: g.id, words: g.words, actions,
            hint: actions.length === 1 ? actionWords(s, actions[0]) : `${actions.length} choices`};
    });
}

/** What the wheel shows now: its kinds, or the chosen kind's actions (or a flat list an action opened). */
export function wheelEntries(s: GameState): WheelEntry[] {
    const action = (a: string): WheelEntry => ({kind: 'action', id: a, words: actionWords(s, a), hint: '', actions: [a]});
    if (s.contextCategory === FlatWheel) return [...new Set(s.contextActions)].map(action);
    const groups = wheelGroups(s);
    if (groups.length <= 1) return (groups[0]?.actions ?? []).map(action);
    if (!s.contextCategory) return groups;
    const chosen = groups.find(g => g.id === s.contextCategory);
    return chosen ? chosen.actions.map(action) : groups;
}

/** Whether the wheel shows one kind's actions (Esc or the middle goes back to the kinds). */
export function wheelInGroup(s: GameState): boolean {
    return !!s.contextCategory && s.contextCategory !== FlatWheel && wheelGroups(s).length > 1;
}
