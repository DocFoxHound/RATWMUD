// The dialogue window's state (Docs/Design/83-dialogue-trees.md; ui/hud/dialogue.ts draws it). The server runs the
// tree and sends each node ({"type": "dialogue"}); the window shows the resident's line and the choices, and sends back
// the number chosen with the words to say: the choice's default line, or the player's own (an editable field prefilled
// with the default: the user's answer, 2026-11-15). Where the node allows it, "Say something else..." sends free talk.
import {arr, bool, isObject, num, obj, str, type Json} from './json.ts';

export interface DialogueChoice {
    n: number;
    text: string;               // The default line.
    locked: boolean;
    why: string;
}
export interface Dialogue {
    session: number;
    npc: string;
    name: string;
    role: string;
    place: string;
    appearance: Json | null;
    tree: string;
    node: string;
    line: string;
    mood: string;               // The face's mood with the line (game/face.ts: FaceMoods), or '' at rest.
    speaking: number;           // Seconds the line is spoken (the face talks that long: Phase 5); 0 from an older server.
    free: boolean;              // Free talk allowed here.
    awaiting: boolean;          // A free line is being answered.
    ends: boolean;              // The last line: no choices.
    choices: DialogueChoice[];
    history: {who: 'npc' | 'you'; text: string}[];
    note: string;
    inShop: boolean;            // The shop window is open on this keeper.
    wantsShop: boolean;         // Asked for it ("Let me see your wares."): it opens as the keeper's list arrives.
    key: string;                // What it shows, to reset the drafts when it changes.
}

/** How near the wolf goes before asking (tiles): a shop's list comes within two, so a keeper's counter is nearer. */
export function dialogueReach(entry: string): number {
    return entry === 'trade' ? 1.8 : 2.5;
}

/** The giver's dialogue a wheel's work action opens (doc 83, Phase 2), where the server runs the givers' trees: "Ask for
 *  work" the keeper's list, "Ask about: ..." (offer kNNN) that job, "Ask for missions" the official's board, "Deliver
 *  ..." and "Hand over the letter" the recipient's hand-in. Null for any other action (it is sent as it was). */
export function workEntry(action: string): string | null {
    if (action === 'ask for work') return 'work';
    if (action === 'ask for missions') return 'missions';
    const offer = /^offer (\S+)$/.exec(action);
    if (offer) return `job:${offer[1]}`;
    const deliver = /^(?:deliver|hand over) (\S+)$/.exec(action);
    if (deliver) return `deliver:${deliver[1]}`;
    return null;
}

/** The role's dialogue a wheel's action opens (doc 83, Phase 3), where the server runs the roles' trees: Apprentice a
 *  master's trade, Report and Pay fine the watch's crime and fine. Null for any other action. (Talk itself opens the
 *  `talk` entry for anyone: the server answers it with the tree of the resident's role, by their work, or the hub.) */
export function roleEntry(action: string): string | null {
    return ({apprentice: 'learn', report: 'report', 'pay fine': 'fine'} as Record<string, string>)[action] ?? null;
}

/** What the player is doing in the window: the choice picked out, their own words for each choice (drafts), the free
 *  line, and when the current line began (it is written out quickly). */
export interface DialogueUi {
    selected: number;           // An index into the choices.
    drafts: Map<number, string>;
    free: string;
    lineAt: number;
    whole: boolean;
    focus: '' | 'edit' | 'free';
}

export function newDialogueUi(): DialogueUi {
    return {selected: 0, drafts: new Map(), free: '', lineAt: 0, whole: false, focus: ''};
}

export function readDialogue(e: Json): Dialogue {
    const choices = arr(e, 'choices').filter(isObject).slice(0, 9).map(c => ({
        n: Math.trunc(num(c, 'n')), text: str(c, 'text').slice(0, 200), locked: bool(c, 'locked'), why: str(c, 'why').slice(0, 120)}));
    const history = arr(e, 'history').filter(isObject).slice(-4).map(h => ({
        who: str(h, 'who') === 'you' ? 'you' as const : 'npc' as const, text: str(h, 'text').slice(0, 400)}));
    const d: Dialogue = {
        session: Math.trunc(num(e, 'session')), npc: str(e, 'npc'), name: str(e, 'name', 'Someone').slice(0, 60),
        role: str(e, 'role').slice(0, 60), place: str(e, 'place').slice(0, 60), appearance: obj(e, 'appearance'),
        tree: str(e, 'tree'), node: str(e, 'node'), line: str(e, 'line').slice(0, 600), mood: str(e, 'mood').slice(0, 20),
        speaking: Math.min(12, Math.max(0, num(e, 'speaking'))),
        free: bool(e, 'free'), awaiting: bool(e, 'awaiting'), ends: bool(e, 'ends'), choices, history,
        note: str(e, 'note').slice(0, 160), inShop: false, wantsShop: false, key: ''};
    d.key = JSON.stringify([d.session, d.node, d.line, choices.map(c => c.text)]);
    return d;
}

/** The words a choice says: the player's own, if they wrote any, else its default line. */
export function wordsFor(d: Dialogue, ui: DialogueUi, n: number): string {
    const choice = d.choices.find(c => c.n === n);
    if (!choice) return '';
    const draft = (ui.drafts.get(n) ?? '').replace(/\s+/g, ' ').trim();
    return draft || choice.text;
}

/** The message for a choice, or null if it can't be chosen (missing, or locked): its own words only where the player
 *  changed them. */
export function chooseMessage(d: Dialogue, ui: DialogueUi, n: number): Record<string, unknown> | null {
    const choice = d.choices.find(c => c.n === n);
    if (!choice || choice.locked) return null;
    const words = wordsFor(d, ui, n).slice(0, 300);
    return {type: 'dialogue_choose', session: d.session, n, ...(words !== choice.text ? {text: words} : {})};
}

/** Free talk, where the node allows it and nothing is being answered. */
export function sayMessage(d: Dialogue, text: string): Record<string, unknown> | null {
    const words = text.replace(/\s+/g, ' ').trim().slice(0, 300);
    if (!d.free || !words || d.awaiting) return null;
    return {type: 'dialogue_say', session: d.session, text: words};
}

/** Up and Down through the choices, past locked ones, round the ends. */
export function moveSelection(d: Dialogue, ui: DialogueUi, by: number) {
    const count = d.choices.length;
    if (!count) return;
    for (let step = 1; step <= count; ++step) {
        const at = ((ui.selected + by * step) % count + count) % count;
        if (!d.choices[at].locked) {
            ui.selected = at;
            return;
        }
    }
}

/** What a key does with the window open and no field being typed in. */
export type DialogueKey = {kind: 'choose'; n: number} | {kind: 'up' | 'down' | 'send' | 'bye' | 'edit' | 'free' | 'whole'} | null;
export function dialogueKey(code: string, d: Dialogue): DialogueKey {
    const digit = /^(?:Digit|Numpad)([1-9])$/.exec(code);
    if (digit) return d.choices.some(c => c.n === +digit[1]) ? {kind: 'choose', n: +digit[1]} : null;
    if (code === 'ArrowUp') return {kind: 'up'};
    if (code === 'ArrowDown') return {kind: 'down'};
    if (code === 'Enter' || code === 'NumpadEnter') return {kind: 'send'};
    if (code === 'Escape') return {kind: 'bye'};
    if (code === 'Tab') return d.choices.length ? {kind: 'edit'} : d.free ? {kind: 'free'} : null;
    if (code === 'Space') return {kind: 'whole'};
    return null;
}

/** How much of the line shows, written out at about sixty characters a second (all of it once shown whole). */
export function revealed(line: string, ui: DialogueUi, now: number, reduced = false): string {
    if (ui.whole || reduced) return line;
    const n = Math.max(0, Math.floor((now - ui.lineAt) * 60));
    return n >= line.length ? line : line.slice(0, n);
}
