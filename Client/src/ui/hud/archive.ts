// The archive and the journal (Docs/Design/54-gathering-places.md, 7). The sorting sheet: six records to put in order
// by their clues (▲ ▼ to move one), handed in for the keeper to check (the page never knows the answer). The journal:
// the lore read at archives, the bestiary, the herbarium and the places been.
import {arr, isObject, num, str, type Json} from '../../game/json.ts';
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

export function renderJournal(panel: HTMLElement, s: GameState) {
    const j = s.journalView;
    el('div', 'label gold', panel, 'THE JOURNAL');
    el('h1', '', panel, 'What you have found out');
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
