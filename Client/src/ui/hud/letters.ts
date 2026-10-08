// The letter case (Docs/Design/55-letters-gifts-favours.md, 2): letters delivered (sealed until opened, each with its
// signature, its scent line, when and where from), those waiting at a town's inns (SEND IT ON), and the writing sheet
// (to a wolf known by name, the text with its count, a signature of one's own names or none). REPLY answers the writer,
// or, for one who wrote anonymously, once by the same courier.
import {arr, bool, isObject, num, obj, str, type Json} from '../../game/json.ts';
import type {GameState} from '../../game/state.ts';
import {button, el} from './dom.ts';

/** What the writing sheet holds between rebuilds: to whom, the text, the signature, and the letter being answered. */
export interface LetterDraft {
    to: string;
    text: string;
    sign: string;
    replyTo: string;
    encItem?: string;
    encQuantity?: number;
    encCoins?: number;
    by?: string;
}

export const TextMost = 800;

/** How many letters a reader counts in `text` (not bytes). */
export function lettersIn(text: string): number {
    return [...text].length;
}

export function renderLetters(panel: HTMLElement, s: GameState) {
    const c = s.lettersCase;
    el('div', 'label gold', panel, 'LETTERS');
    el('h1', '', panel, 'Your letter case');
    if (!c) {
        el('p', 'muted', panel, 'Opening the case…');
        return;
    }
    const d = s.letterDraft;
    // Writing (or answering): only where a letter can be written.
    if (bool(c, 'canWrite') || d.replyTo) {
        const sheet = el('div', 'letter-sheet', panel);
        el('div', 'label muted', sheet, d.replyTo ? 'YOUR ANSWER' : 'WRITE A LETTER');
        if (!d.replyTo) {
            const to = el('input', 'profile-input', sheet) as HTMLInputElement;
            to.placeholder = 'To: the name you know them by';
            to.maxLength = 32;
            to.value = d.to;
            to.dataset.field = 'letter-to';
            to.addEventListener('input', () => { d.to = to.value; });
        }
        const text = el('textarea', 'profile-input letter-text', sheet) as HTMLTextAreaElement;
        text.placeholder = 'Your letter…';
        text.value = d.text;
        text.dataset.field = 'letter-text';
        const count = el('span', 'muted small', sheet, `${lettersIn(d.text)} / ${TextMost}`);
        text.addEventListener('input', () => {
            d.text = text.value;
            count.textContent = `${lettersIn(d.text)} / ${TextMost}`;
            count.classList.toggle('warn', lettersIn(d.text) > TextMost);
        });
        const row = el('div', 'sheet-actions', sheet);
        const sign = el('select', 'profile-input', row) as HTMLSelectElement;
        sign.dataset.field = 'letter-sign';
        const mine = obj(obj(s.snapshot, 'self'), 'names');
        const names = [str(mine, 'name'), ...arr(mine, 'aliases').filter((n): n is string => typeof n === 'string')].filter(n => n);
        for (const [value, label] of [['', 'Unsigned'], ...names.map(n => [n, `Signed: ${n}`])] as const) {
            const o = el('option', '', sign, label) as HTMLOptionElement;
            o.value = value;
        }
        sign.value = d.sign;
        sign.addEventListener('change', () => { d.sign = sign.value; });
        // An enclosure (doc 55, 3): one small thing (a pound in all) and up to 50p.
        if (!d.replyTo) {
            const enc = el('select', 'profile-input', row) as HTMLSelectElement;
            enc.dataset.field = 'letter-enclose';
            el('option', '', enc, 'Nothing enclosed').setAttribute('value', '');
            for (const i of arr(s.snapshot, 'inventory').filter(isObject).filter(i => num(i, 'weight') > 0 && num(i, 'weight') <= 1)) {
                const o = el('option', '', enc, `Enclose: ${str(i, 'name')}`) as HTMLOptionElement;
                o.value = str(i, 'id');
            }
            enc.value = d.encItem ?? '';
            enc.addEventListener('change', () => { d.encItem = enc.value; });
            const coins = el('input', 'profile-input', row) as HTMLInputElement;
            coins.type = 'number';
            coins.min = '0';
            coins.max = '50';
            coins.value = String(d.encCoins ?? 0);
            coins.title = 'Pennies enclosed (50 at most)';
            coins.addEventListener('input', () => { d.encCoins = Math.max(0, Math.min(50, Math.trunc(Number(coins.value) || 0))); });
            // Carried by a friend (doc 55, 8): a wolf you know who knows them too, paid the fee instead of the town.
            const by = el('input', 'profile-input', row) as HTMLInputElement;
            by.placeholder = 'Carried by a friend (optional)';
            by.dataset.field = 'letter-by';
            by.value = d.by ?? '';
            by.addEventListener('input', () => { d.by = by.value; });
        }
        button(d.replyTo ? 'SEND THE ANSWER' : 'SEND', 'primary', row, () => {
            if (d.replyTo) s.sendLetter('reply', {id: d.replyTo, text: d.text, sign: d.sign});
            else s.sendLetter('write', {to: d.to, text: d.text, sign: d.sign, ...(d.by ? {by: d.by} : {}),
                ...(d.encItem || d.encCoins ? {enclose: {item: d.encItem ?? '', quantity: d.encQuantity ?? 1, coins: d.encCoins ?? 0}} : {})});
            Object.assign(d, {to: '', text: '', sign: d.sign, replyTo: '', encItem: '', encQuantity: 1, encCoins: 0, by: ''});
            s.letterDraftVersion++;
        });
        if (d.replyTo) button('CANCEL', 'secondary', row, () => { d.replyTo = ''; s.letterDraftVersion++; });
        el('p', 'muted small', sheet, 'Sealed, carried by the town\'s courier (a penny in town, more to another). It carries your scent, unless masked.');
    } else el('p', 'muted small', panel, 'Letters are written at an inn or tavern, at a scriptorium\'s desk, or in your Chapter\'s own place.');
    // A pact (doc 55, 8): terms one sets down naming another; both seal it; nothing enforces it.
    const pact = el('details', 'letter-sheet', panel);
    el('summary', 'label muted', pact, 'SET DOWN A PACT');
    const withWho = el('input', 'profile-input', pact) as HTMLInputElement;
    withWho.placeholder = 'With: the name you know them by';
    withWho.dataset.field = 'pact-with';
    const terms = el('textarea', 'profile-input', pact) as HTMLTextAreaElement;
    terms.placeholder = 'The terms (400 letters at most)';
    terms.dataset.field = 'pact-text';
    button('SET IT DOWN AND SEAL IT', 'secondary', pact, () => s.send({type: 'pact', verb: 'write', with: withWho.value, text: terms.value}));
    // Waiting at inns.
    for (const w of arr(c, 'waiting').filter(isObject)) {
        const row = el('div', 'story-row letter-waiting', panel);
        el('span', '', row, `${num(w, 'count')} ${num(w, 'count') === 1 ? 'letter waits' : 'letters wait'} at the inns of ${str(w, 'town')}`);
        button('SEND IT ON', 'small', row, () => s.sendLetter('sendOn', {from: str(w, 'id')})).title =
            'A penny a letter, and the courier\'s time again, to bring it to the town you are in';
    }
    const letters = arr(c, 'letters').filter(isObject);
    if (!letters.length && !arr(c, 'waiting').length) el('p', 'muted', panel, 'No letters.');
    for (const l of letters) letterRow(panel, s, l);
}

function letterRow(panel: HTMLElement, s: GameState, l: Json) {
    const box = el('div', `letter${bool(l, 'read') ? '' : ' unread'}`, panel);
    const head = el('div', 'letter-head', box);
    el('span', 'label', head, str(l, 'sign') ? `From ${str(l, 'sign')}` : str(l, 'by') ? `From ${str(l, 'by')}`
        : bool(l, 'viaCourier') ? 'An answer, by the same courier' : 'Unsigned');
    el('span', 'muted small', head, ` · ${str(l, 'from')} · ${str(l, 'when')}${bool(l, 'kept') ? ' · kept' : ''}`);
    if (str(l, 'scent')) el('div', 'muted small letter-scent', box, str(l, 'scent'));
    if (bool(l, 'read')) el('p', 'letter-body', box, str(l, 'text'));
    else el('p', 'muted letter-body', box, 'Sealed.');
    if (str(l, 'enclosed')) el('div', 'label gold', box, `ENCLOSED · ${str(l, 'enclosed')}`);
    const actions = el('div', 'sheet-actions', box);
    const id = str(l, 'id');
    if (!bool(l, 'read')) button('BREAK THE SEAL', 'primary', actions, () => s.sendLetter('read', {id}));
    else {
        if (str(l, 'enclosed')) button('TAKE', 'primary', actions, () => s.sendLetter('take', {id}));
        if (bool(l, 'pact')) {
            // A pact (doc 55, 8): between whom, sealed by whom; SEAL, and ASK A WITNESS.
            el('span', 'muted small', actions, `${str(l, 'between')} · sealed by ${arr(l, 'seals').filter((x): x is string => typeof x === 'string').join(', ')}`);
            if (bool(l, 'canSeal')) button('SEAL', 'primary', actions, () => s.send({type: 'pact', verb: 'seal', id}));
            if (bool(l, 'canAskWitness')) {
                const by = el('input', 'profile-input', actions) as HTMLInputElement;
                by.placeholder = 'A witness, by name';
                button('ASK A WITNESS', 'secondary', actions, () => s.send({type: 'pact', verb: 'witness', id, by: by.value}));
            }
        }
        if (bool(l, 'invitation')) {
            const answer = num(l, 'answer');
            if (answer !== 0) el('span', 'label sage', actions, answer > 0 ? 'YOU SAID YOU WOULD COME' : 'YOU SENT WORD YOU CAN\'T');
            button('COMING', answer > 0 ? 'primary' : 'secondary', actions, () => s.sendLetter('answer', {id, yes: true}));
            button('CAN\'T COME', 'secondary', actions, () => s.sendLetter('answer', {id, yes: false}));
        }
        if (str(l, 'work')) button('TAKE IT ON', 'primary', actions, () => s.sendLetter('takeWork', {id})).title =
            'Offered to you first: yours for two days, then it goes on the board';
        if (bool(l, 'canReply')) button('REPLY', 'secondary', actions, () => {
            s.letterDraft.replyTo = id;
            s.letterDraft.text = '';
            s.letterDraftVersion++;
        });
        button(bool(l, 'kept') ? 'UNKEEP' : 'KEEP', 'secondary', actions, () => s.sendLetter('keep', {id}));
        button('BURN', 'secondary', actions, () => s.sendLetter('burn', {id})).title = 'Gone for good';
    }
}
