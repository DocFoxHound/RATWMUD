// A town's notice board (Docs/Design/54-gathering-places.md, 2): the work side (the town's contracts, TAKE IT ON) and
// the public side (players' notices: kind, text, what it seeks or offers, signature or none, the scent line, a
// resident's answer, TAKE DOWN for one's own), and pinning a notice (a penny to the town, seven days).
import {arr, bool, isObject, num, obj, str} from '../../game/json.ts';
import type {GameState} from '../../game/state.ts';
import {button, el} from './dom.ts';

const KindWords: Record<string, string> = {seeking: 'SEEKING', offering: 'OFFERING', event: 'EVENT', lost: 'LOST AND FOUND', other: 'NOTICE'};

export function renderBoard(panel: HTMLElement, s: GameState) {
    const b = s.boardView;
    el('div', 'label gold', panel, 'NOTICE BOARD');
    el('h1', '', panel, b ? str(b, 'town') : 'The board');
    if (!b) {
        el('p', 'muted', panel, 'Reading the board…');
        return;
    }
    const send = (verb: string, extra: Record<string, unknown> = {}) => s.send({type: 'board', verb, ...extra});
    const festival = obj(b, 'festival');
    if (festival) {
        // The festival's programme, from three days before (doc 54, 6).
        const days = num(festival, 'inDays');
        el('div', 'label gold', panel, `${str(festival, 'name').toUpperCase()} · ${days ? `IN ${days} ${days === 1 ? 'DAY' : 'DAYS'}` : 'TODAY'}`);
        el('p', 'small', panel, arr(festival, 'programme').map(String).join(' · ') + '. Sign up at the square, here, or at the inn: a penny a contest.');
    }
    el('div', 'label muted', panel, 'WORK AND STORIES');
    const work = arr(b, 'work').filter(isObject);
    if (!work.length) el('p', 'muted small', panel, 'No work posted in this town just now.');
    for (const w of work) {
        const row = el('div', 'story-row', panel);
        el('span', '', row, `${str(w, 'text')}${num(w, 'reward') ? ` · ${num(w, 'reward')}p` : ''}`);
        if (bool(w, 'canTake')) button('TAKE IT ON', 'small', row, () => send('take', {id: str(w, 'id')}));
    }
    el('div', 'label muted', panel, 'NOTICES');
    const notices = arr(b, 'notices').filter(isObject);
    if (!notices.length) el('p', 'muted small', panel, 'Nothing pinned up.');
    for (const n of notices) {
        const box = el('div', 'letter', panel);
        const head = el('div', 'letter-head', box);
        el('span', 'label', head, KindWords[str(n, 'kind')] ?? 'NOTICE');
        el('span', 'muted small', head, ` · ${str(n, 'sign') || 'unsigned'} · ${str(n, 'when')} · ${num(n, 'days').toFixed(1)} days left`);
        if (str(n, 'what')) el('div', 'muted small', box, `${str(n, 'kind') === 'offering' ? 'Offering' : 'Seeking'}: ${str(n, 'what')}`);
        el('p', 'letter-body', box, str(n, 'text'));
        if (str(n, 'scent')) el('div', 'muted small letter-scent', box, str(n, 'scent'));
        if (str(n, 'answer')) el('div', 'sage small', box, `↳ ${str(n, 'answer')}`);
        if (bool(n, 'mine')) button('TAKE DOWN', 'secondary', box, () => send('unpost', {id: str(n, 'id')}));
    }
    // Pinning a notice.
    const form = el('details', 'letter-sheet', panel);
    el('summary', 'label muted', form, 'PIN A NOTICE · 1p');
    const kind = el('select', 'profile-input', form) as HTMLSelectElement;
    kind.dataset.field = 'notice-kind';
    for (const [value, label] of [['seeking', 'Seeking'], ['offering', 'Offering'], ['event', 'An event'], ['lost', 'Lost and found'], ['other', 'Other']]) {
        const o = el('option', '', kind, label) as HTMLOptionElement;
        o.value = value;
    }
    const whatType = el('select', 'profile-input', form) as HTMLSelectElement;
    whatType.dataset.field = 'notice-what-type';
    for (const [value, label] of [['', 'Nothing in particular'], ['item', 'A good (its name)'], ['apprenticeship', 'An apprenticeship (a trade)'],
        ['room', 'A room'], ['partner', 'A hunting or work partner']]) {
        const o = el('option', '', whatType, label) as HTMLOptionElement;
        o.value = value;
    }
    const whatValue = el('input', 'profile-input', form) as HTMLInputElement;
    whatValue.placeholder = 'Which good, or which trade';
    whatValue.dataset.field = 'notice-what';
    const text = el('textarea', 'profile-input', form) as HTMLTextAreaElement;
    text.placeholder = 'Your notice (280 letters at most)';
    text.dataset.field = 'notice-text';
    button('PIN IT UP', 'primary', form, () => send('post', {kind: kind.value, whatType: whatType.value, whatValue: whatValue.value, text: text.value}));
    el('p', 'muted small', form, 'A penny to the town; it stays up seven days. It carries your scent unless you are masked. Residents answer what you seek, never what you write.');
}
