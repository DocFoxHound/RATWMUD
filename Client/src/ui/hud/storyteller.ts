// Player storytellers (Docs/Design/58-player-storytellers.md): the STORYTELLER page (one's tales: begin, invite, admit,
// tick, narrate, voice a story character, roll, send for a visitor, give a prize, post a call, end; and a new tale's
// editor), applying from the character sheet, and the credits screen at a tale's end or a world story's milestone.
import type {GameState} from '../../game/state.ts';
import {arr, bool, isObject, num, obj, str, type Json} from '../../game/json.ts';
import {button, el} from './dom.ts';

const Kinds: [string, string][] = [['told', 'You judge it done'], ['place', 'Be at a place'], ['talk', 'Talk to a resident'],
    ['scene', 'A scene together'], ['contract', 'A job done'], ['hunt', 'A kill'], ['fight', 'A fight won'], ['gift', 'A Gift used'], ['deliver', 'An item given']];

interface DraftStep { title: string; text: string; kind: string; target: string; line: string }
interface Draft { title: string; premise: string; chapter: boolean; steps: DraftStep[]; cast: {name: string; looks: string}[] }
const draft: Draft = {title: '', premise: '', chapter: false, steps: [{title: '', text: '', kind: 'told', target: '', line: ''}], cast: []};

const input = (parent: HTMLElement, value: string, placeholder: string, set: (v: string) => void, field = '') => {
    const i = el('input', 'profile-input', parent) as HTMLInputElement;
    i.value = value;
    i.placeholder = placeholder;
    if (field) i.dataset.field = field;
    i.addEventListener('change', () => set(i.value));
    return i;
};

/** The command a storyteller types in the composer (/narrate, /npc, /roll, /tick), as what the server takes; null if not one. */
export function storyCommand(text: string): Record<string, unknown> | null {
    const narrate = /^\/narrate\s+([\s\S]+)$/i.exec(text);
    if (narrate) return {type: 'storyteller', verb: 'narrate', text: narrate[1].trim()};
    const npc = /^\/npc\s+([^:]+):\s*([\s\S]+)$/i.exec(text);
    if (npc) return {type: 'storyteller', verb: 'npc', name: npc[1].trim(), text: npc[2].trim().replace(/^"(.*)"$/, '$1')};
    const roll = /^\/roll\s+(\S+)(?:\s+for\s+(.+))?$/i.exec(text);
    if (roll) return {type: 'storyteller', verb: 'roll', dice: roll[1], for: roll[2]?.trim() ?? ''};
    const tick = /^\/tick(?:\s+(\d+)\.(\d+))?$/i.exec(text);
    if (tick) return {type: 'storyteller', verb: 'tick', ...(tick[1] ? {step: Number(tick[1]) - 1, objective: Number(tick[2]) - 1} : {objective: 0})};
    return null;
}

export function renderStoryteller(panel: HTMLElement, s: GameState, redraw: () => void) {
    const st = obj(obj(s.snapshot, 'self'), 'storyteller');
    const send = (verb: string, extra: Record<string, unknown> = {}) => s.send({type: 'storyteller', verb, ...extra});
    el('div', 'label gold', panel, 'STORYTELLER');
    el('h1', '', panel, 'Your tales');
    if (str(st, 'state') !== 'approved') {
        el('p', 'muted', panel, 'Only storytellers the Dungeon Masters have approved tell tales. Apply from your character sheet.');
        return;
    }
    el('p', 'muted small', panel, `In the box where you write: ${arr(st, 'commands').map(String).join(' · ')}`);
    const wolves = [...s.entities.values()].filter(e => e.kind !== 'npc' && !e.self);
    for (const t of arr(st, 'tales').filter(isObject)) {
        const id = str(t, 'id'), state = str(t, 'state');
        const box = el('div', 'letter', panel);
        el('div', 'label', box, `${str(t, 'title').toUpperCase()} · ${state} · step ${Math.min(num(t, 'step') + 1, num(t, 'steps'))} of ${num(t, 'steps')}`);
        const row = el('div', 'story-row', box);
        if (state === 'draft' || state === 'paused') button('BEGIN', 'small', row, () => send('start', {tale: id}));
        if (state !== 'running') continue;
        button('END · DONE', 'small', row, () => { if (window.confirm('End the tale, done?')) send('end', {tale: id, how: 'done'}); });
        button('END · FAILED', 'small', row, () => { if (window.confirm('End the tale, failed?')) send('end', {tale: id, how: 'failed'}); });
        button('POST A CALL', 'small', row, () => send('call', {tale: id})).title = 'On this town\'s board: strangers may ask to join; you admit them';
        arr(t, 'objectives').filter(isObject).forEach((o, k) => {
            const r = el('div', 'story-row', box);
            el('span', 'small', r, `${bool(o, 'done') ? '✓' : '·'} ${str(o, 'line')}`);
            if (!bool(o, 'done')) button('TICK', 'small', r, () => send('tick', {tale: id, step: num(t, 'step'), objective: k}));
        });
        for (const a of arr(t, 'asked').filter(isObject)) {
            const r = el('div', 'story-row', box);
            el('span', 'small', r, `${str(a, 'name')} asks to join`);
            button('ADMIT', 'small', r, () => send('admit', {tale: id, who: str(a, 'id')}));
        }
        // Invite a wolf in sight; narrate; a story character's line; dice; a visitor; a prize.
        const invite = el('div', 'story-row', box);
        const who = el('select', '', invite) as HTMLSelectElement;
        for (const w of wolves) (el('option', '', who, w.name) as HTMLOptionElement).value = w.id;
        button('INVITE', 'small', invite, () => { if (who.value) send('invite', {tale: id, who: who.value}); });
        const words = el('textarea', 'profile-input', box) as HTMLTextAreaElement;
        words.placeholder = 'Narration (1,000 letters), or a story character\'s line';
        words.dataset.field = 'narration';
        const say = el('div', 'story-row', box);
        button('NARRATE', 'small', say, () => { if (words.value.trim()) send('narrate', {tale: id, text: words.value.trim()}); words.value = ''; });
        const cast = el('select', '', say) as HTMLSelectElement;
        for (const c of arr(t, 'cast').map(String)) (el('option', '', cast, c) as HTMLOptionElement).value = c;
        button('SAY AS', 'small', say, () => { if (cast.value && words.value.trim()) send('npc', {tale: id, name: cast.value, text: words.value.trim()}); words.value = ''; });
        const dice = el('div', 'story-row', box);
        const roll = input(dice, '2d6', 'Dice', () => {});
        button('ROLL', 'small', dice, () => send('roll', {tale: id, dice: roll.value}));
        const visitors = arr(st, 'visitors').filter(isObject);
        if (visitors.length) {
            const v = el('select', '', dice) as HTMLSelectElement;
            for (const x of visitors) (el('option', '', v, str(x, 'name')) as HTMLOptionElement).value = str(x, 'id');
            button('SEND FOR', 'small', dice, () => send('visitor', {tale: id, visitor: v.value, minutes: 20}));
        }
        for (const v of arr(st, 'onStage').filter(isObject))
            button(`SEND ${str(v, 'name').toUpperCase()} AWAY`, 'small', dice, () => send('dismiss', {tale: id, visitor: str(v, 'id')}));
        const prize = el('div', 'story-row', box);
        const to = el('select', '', prize) as HTMLSelectElement;
        for (const w of wolves) (el('option', '', to, w.name) as HTMLOptionElement).value = w.id;
        const coins = input(prize, '1', 'Pennies', () => {});
        coins.type = 'number';
        coins.style.width = '4em';
        button('GIVE A PRIZE', 'small', prize, () => send('prize', {tale: id, to: to.value, coins: Math.trunc(Number(coins.value))}));
    }
    // A new tale.
    const form = el('details', 'letter-sheet', panel);
    el('summary', 'label muted', form, 'WRITE A NEW TALE');
    input(form, draft.title, 'Title (60 letters)', v => { draft.title = v; }, 'tale-title');
    const premise = el('textarea', 'profile-input', form) as HTMLTextAreaElement;
    premise.value = draft.premise;
    premise.placeholder = 'Premise (500 letters)';
    premise.addEventListener('change', () => { draft.premise = premise.value; });
    draft.steps.forEach((step, i) => {
        el('div', 'label muted', form, `STEP ${i + 1}`);
        input(form, step.title, 'Its title', v => { step.title = v; }, `step-${i}-title`);
        input(form, step.text, 'What it asks', v => { step.text = v; }, `step-${i}-text`);
        const kind = el('select', '', form) as HTMLSelectElement;
        for (const [k, words] of Kinds) (el('option', '', kind, words) as HTMLOptionElement).value = k;
        kind.value = step.kind;
        kind.addEventListener('change', () => { step.kind = kind.value; });
        input(form, step.target, 'Where or whom (a place or resident id), if it asks', v => { step.target = v; });
    });
    const more = el('div', 'story-row', form);
    if (draft.steps.length < 10) button('ADD A STEP', 'small', more, () => { draft.steps.push({title: '', text: '', kind: 'told', target: '', line: ''}); redraw(); });
    if (draft.cast.length < 6) button('ADD A STORY CHARACTER', 'small', more, () => { draft.cast.push({name: '', looks: ''}); redraw(); });
    draft.cast.forEach((c, i) => {
        input(form, c.name, `Story character ${i + 1}: a name (never a real wolf's)`, v => { c.name = v; });
        input(form, c.looks, 'How they look', v => { c.looks = v; });
    });
    button('SAVE THE DRAFT', 'primary', form, () => send('save', {
        title: draft.title, premise: draft.premise, chapter: draft.chapter,
        steps: draft.steps.map(st => ({title: st.title, text: st.text, objectives: [{kind: st.kind, target: st.target, line: st.title}]})),
        cast: draft.cast,
    }));
}

/** The credits screen: who did what, and the stars one may give (a tale: its storyteller, once; a milestone: up to 3). */
export function renderCredits(panel: HTMLElement, s: GameState) {
    const c = s.creditsView;
    if (!c) return;
    const send = (to: string) => s.send({type: 'storyteller', verb: 'star', credits: str(c, 'id'), to});
    el('div', 'label gold', panel, str(c, 'kind') === 'tale' ? 'THE TALE ENDS' : 'A MILESTONE');
    el('h1', '', panel, str(c, 'story') ? `${str(c, 'story')} · ${str(c, 'title')}` : str(c, 'title'));
    if (str(c, 'kind') === 'tale') {
        const row = el('div', 'story-row', panel);
        el('span', '', row, `Told by ${str(c, 'tellerName')}`);
        if (bool(c, 'tellerStarred')) el('span', 'gold small', row, ' ★ starred');
        else button('★ STORYTELLER', 'small', row, () => send(str(c, 'teller'))).title = 'A star for the storyteller, tagged Storyteller';
    } else el('p', 'muted small', panel, `Up to ${num(c, 'stars')} stars, to anyone on it.`);
    for (const p of arr(c, 'people').filter(isObject)) {
        const box = el('div', 'letter', panel);
        const head = el('div', 'story-row', box);
        el('span', 'label', head, str(p, 'name'));
        if (str(c, 'kind') !== 'tale' && str(p, 'name') !== 'You') {
            if (bool(p, 'starred')) el('span', 'gold small', head, ' ★');
            else button('★', 'small', head, () => send(str(p, 'id')));
        }
        for (const l of arr(p, 'lines').map(String)) el('div', 'small', box, l);
    }
}

export function storytellerNote(st: Json | null): string {
    // The sheet's line: where one stands as a storyteller.
    const state = str(st, 'state');
    return state === 'approved' ? 'You tell stories (the STORYTELLER button).' : state === 'applied' ? 'Your application to tell stories is with the Dungeon Masters.'
        : state === 'refused' ? `Your application was refused${str(st, 'reason') ? `: ${str(st, 'reason')}` : '.'}`
        : state === 'revoked' ? `You no longer tell stories${str(st, 'reason') ? `: ${str(st, 'reason')}` : '.'}` : '';
}
