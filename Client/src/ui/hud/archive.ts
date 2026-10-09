// The archive and the journal (Docs/Design/54-gathering-places.md, 7). The sorting sheet: six records to put in order
// by their clues (▲ ▼ to move one), handed in for the keeper to check (the page never knows the answer). The journal:
// the lore read at archives, the bestiary, the herbarium and the places been.
import {arr, bool, isObject, num, obj, str, type Json} from '../../game/json.ts';
import type {GameState} from '../../game/state.ts';
import {button, el} from './dom.ts';

/** Moves the record at `i` one place up (-1) or down (+1); the same order if it can't. */
export function moveRecord(order: readonly string[], i: number, by: -1 | 1): string[] {
    const out = [...order];
    const j = i + by;
    if (i < 0 || i >= out.length || j < 0 || j >= out.length) return out;
    [out[i], out[j]] = [out[j], out[i]];
    return out;
}

let shownTask: Json | null = null;
let order: string[] = [];

export function renderArchive(panel: HTMLElement, s: GameState) {
    const task = s.archiveTask;
    el('div', 'label gold', panel, 'ARCHIVE WORK');
    el('h1', '', panel, 'Records to sort');
    if (!task) {
        el('p', 'muted', panel, 'Ask a keeper of records for work.');
        return;
    }
    const records = arr(task, 'records').filter(isObject);
    if (task !== shownTask) {
        shownTask = task;
        order = records.map(r => str(r, 'id'));
    }
    const words = new Map(records.map(r => [str(r, 'id'), str(r, 'text')]));
    el('p', '', panel, `${str(task, 'rule')} (${num(task, 'tries', 3)} ${num(task, 'tries', 3) === 1 ? 'try' : 'tries'} left)`);
    const list = el('div', '', panel);
    const draw = () => {
        list.replaceChildren();
        order.forEach((id, i) => {
            const row = el('div', 'story-row', list);
            el('span', 'muted small', row, `${i + 1}.`);
            el('span', '', row, words.get(id) ?? id);
            button('▲', 'small', row, () => { order = moveRecord(order, i, -1); draw(); }).disabled = i === 0;
            button('▼', 'small', row, () => { order = moveRecord(order, i, 1); draw(); }).disabled = i === order.length - 1;
        });
    };
    draw();
    const actions = el('div', 'sheet-actions', panel);
    button('HAND THEM IN', 'primary', actions, () => s.send({type: 'archive', verb: 'sort', order}));
    button('HAND THE WORK BACK', 'secondary', actions, () => { s.send({type: 'archive', verb: 'leave'}); s.archiveTask = null; });
}

/** A storyline in the journal (doc 58, 2): its steps (✓ done, ● current with its objectives and marker, ○ later). */
export function renderStoryline(box: HTMLElement, s: GameState, t: Json) {
    const send = (verb: string, extra: Record<string, unknown> = {}) => s.send({type: 'storyline', verb, storyline: str(t, 'id'), ...extra});
    const head = el('div', 'letter-head', box);
    el('span', 'label', head, str(t, 'title').toUpperCase());
    el('span', 'muted small', head, ` · ${str(t, 'source')}${str(t, 'state') !== 'running' ? ` · ${str(t, 'state')}` : ''}`);
    if (str(t, 'premise')) el('p', 'small', box, str(t, 'premise'));
    const steps = arr(t, 'steps').filter(isObject);
    steps.forEach((st, i) => {
        const state = str(st, 'state');
        el('div', state === 'current' ? 'small' : 'muted small', box, `${state === 'done' ? '✓' : state === 'current' ? '●' : '○'} ${str(st, 'title')}`);
        if (state !== 'current') return;
        if (str(st, 'text')) el('p', 'small', box, str(st, 'text'));
        if (str(st, 'marker')) el('div', 'gold small', box, `◆ ${str(st, 'marker')}`);
        if (bool(st, 'distinct')) el('div', 'muted small', box, 'Each part by a different wolf.');
        arr(st, 'objectives').filter(isObject).forEach((o, k) => {
            const row = el('div', 'story-row', box);
            el('span', 'small', row, `${bool(o, 'done') ? '✓' : '·'} ${str(o, 'line')}${str(o, 'by') ? ` (${str(o, 'by')})` : ''}` +
                `${!bool(o, 'done') && str(o, 'taken') ? ` · ${str(o, 'taken')} ${str(o, 'taken') === 'you' ? 'are' : 'is'} on it` : ''}` +
                `${str(o, 'suits') ? ` · suits a ${str(o, 'suits')}` : ''}`);
            if (!bool(o, 'done') && arr(t, 'participants').length > 1 && str(o, 'taken') !== 'you')
                button('TAKE', 'small', row, () => send('take', {step: i, objective: k}));
        });
    });
    const people = arr(t, 'participants').filter(isObject);
    if (people.length) el('div', 'muted small', box, people.map(p => `${str(p, 'name')} (${str(p, 'where')})`).join(', '));
    if (bool(t, 'invited')) {
        // Invited into a tale (doc 58, 6): opting in is the wolf's own.
        const row = el('div', 'story-row', box);
        button('JOIN', 'small', row, () => s.send({type: 'storyteller', verb: 'accept', tale: str(t, 'id')}));
        button('NOT NOW', 'small', row, () => s.send({type: 'storyteller', verb: 'decline', tale: str(t, 'id')}));
        return;
    }
    if (bool(t, 'asked')) {
        el('div', 'muted small', box, 'You asked to join; its storyteller will say.');
        return;
    }
    const open = str(t, 'state') === 'running' || str(t, 'state') === 'paused';
    if (!open) return;
    const actions = el('div', 'story-row', box);
    button(bool(t, 'tracked') ? 'UNTRACK' : 'TRACK', 'small', actions, () => send('track'));
    if (!bool(t, 'mine')) button(str(t, 'kind') === 'tale' ? 'LEAVE' : 'ABANDON', 'small', actions, () => {
        if (window.confirm(str(t, 'kind') === 'tale' ? 'Leave this story?' : 'Put this story aside for good?')) send(str(t, 'kind') === 'tale' ? 'leave' : 'abandon');
    });
}

export function renderJournal(panel: HTMLElement, s: GameState) {
    const j = s.journalView;
    el('div', 'label gold', panel, 'THE JOURNAL');
    el('h1', '', panel, 'Your stories, and what you have found out');
    // Stories (doc 58, 2): under way, then done.
    const stories = arr(obj(s.snapshot, 'self'), 'journal').filter(isObject);
    const going = stories.filter(t => ['running', 'paused', 'draft'].includes(str(t, 'state')));
    el('div', 'label gold', panel, 'UNDER WAY');
    if (!going.length) el('p', 'muted small', panel, 'No stories under way. Ties, residents\' troubles and good work start them.');
    for (const t of going) renderStoryline(el('div', 'letter', panel), s, t);
    // Calls on this town's board (doc 58, 6): ask to join; its storyteller admits.
    const calls = arr(obj(obj(s.snapshot, 'self'), 'storyteller'), 'calls').filter(isObject);
    if (calls.length) {
        el('div', 'label gold', panel, 'CALLS IN THIS TOWN');
        for (const c of calls) {
            const row = el('div', 'story-row', panel);
            el('span', 'small', row, `${str(c, 'text')} (${str(c, 'by')})`);
            if (bool(c, 'asked')) el('span', 'muted small', row, ' · asked');
            else button('ASK TO JOIN', 'small', row, () => s.send({type: 'storyteller', verb: 'ask', tale: str(c, 'id')}));
        }
    }
    const done = stories.filter(t => !['running', 'paused', 'draft'].includes(str(t, 'state')));
    if (done.length) {
        el('div', 'label muted', panel, 'DONE');
        for (const t of done) el('div', 'muted small', panel, `${str(t, 'title')} · ${str(t, 'state')} · ${str(t, 'source')}`);
    }
    if (!j) {
        el('p', 'muted', panel, 'Opening the journal…');
        return;
    }
    el('div', 'label gold', panel, `LORE · SCHOLARSHIP ${num(j, 'scholarship')}`);
    const lore = arr(j, 'lore').filter(isObject);
    if (!lore.length) el('p', 'muted small', panel, 'Nothing yet. Keepers of records give work, and the records tell things.');
    for (const f of lore) {
        el('div', 'label muted', panel, `${str(f, 'topic').toUpperCase()} · ${str(f, 'town')}`);
        el('p', 'small', panel, str(f, 'text'));
    }
    const section = (title: string, rows: Json[], line: (r: Json) => string, empty: string) => {
        el('div', 'label gold', panel, title);
        if (!rows.length) el('p', 'muted small', panel, empty);
        for (const r of rows) el('div', 'small', panel, line(r));
    };
    section('BESTIARY', arr(j, 'bestiary').filter(isObject), r => `${str(r, 'name')} · ${num(r, 'count')} brought down · first ${str(r, 'first')}`,
        'Nothing brought down yet.');
    section('HERBARIUM', arr(j, 'herbarium').filter(isObject), r => `${str(r, 'name')} · ${str(r, 'where')} · first ${str(r, 'first')}`,
        'Nothing foraged yet.');
    section('PLACES', arr(j, 'places').filter(isObject), r => `${str(r, 'town')}: ${num(r, 'count')} ${num(r, 'count') === 1 ? 'place' : 'places'}`,
        'Nowhere yet.');
}

/** The chronicle (doc 56, 8): a character's life as the world remembers it, dated, oldest first. */
export function renderChronicle(panel: HTMLElement, s: GameState) {
    const c = s.chronicleView;
    el('div', 'label gold', panel, 'THE CHRONICLE');
    el('h1', '', panel, 'Your life so far');
    if (!c) {
        el('p', 'muted', panel, 'Writing it out…');
        return;
    }
    const entries = arr(c, 'entries').filter(isObject);
    if (!entries.length) el('p', 'muted', panel, 'Nothing yet: the world hasn\'t noticed you. It will.');
    for (const e of entries) el('p', 'small', panel, str(e, 'text'));
    if (c.partial) el('p', 'muted small', panel, 'Only what the world remembers lately: the older pages are elsewhere.');
}

/** While you were away (doc 56, 10): a card shown once on coming back after a break, closed with ×. */
export function renderWelcome(panel: HTMLElement, s: GameState) {
    const w = s.welcomeView;
    el('div', 'label gold', panel, 'WELCOME BACK');
    el('h1', '', panel, w && num(w, 'days') >= 1 ? `${num(w, 'days')} ${num(w, 'days') === 1 ? 'day' : 'days'} away` : 'Back after a while');
    const lines = arr(w, 'lines').map(String);
    if (!lines.length) el('p', 'muted', panel, 'All much as you left it. Folk will be glad to see you.');
    for (const l of lines) el('p', 'small', panel, l);
    el('p', 'muted small', panel, 'This stays on your character sheet until you next leave.');
}
