import {arr, bool, isObject, str, type Json} from '../../game/json.ts';
import type {GameState} from '../../game/state.ts';
import {ask} from '../dialog.ts';
import {button, el} from './dom.ts';

const kinds = {told: 'Storyteller decides', place: 'Reach a place', talk: 'Speak to a resident', scene: 'Share a scene',
    contract: 'Complete a contract', hunt: 'Hunt', fight: 'Win a fight', gift: 'Use a Gift', deliver: 'Deliver an item'};

function readStep(value: Json = {}) {
    const objectives = arr(value, 'objectives').filter(isObject);
    return {...value, title: str(value, 'title'), text: str(value, 'text'), objectives: (objectives.length ? objectives : [{}]).map(o =>
        ({...o, kind: str(o, 'kind', 'told'), target: str(o, 'target'), line: str(o, 'line')}))};
}
function draft(value: Json = {}, id = '') {
    const steps = arr(value, 'steps').filter(isObject);
    return {id, title: str(value, 'title'), premise: str(value, 'premise'), chapter: bool(value, 'chapter'),
        cast: arr(value, 'cast').filter(isObject).map(c => ({name: str(c, 'name'), looks: str(c, 'looks')})),
        steps: steps.length ? steps.map(readStep) : [readStep()]};
}
function session(character: string) {
    return {character, draft: draft(), open: false, dirty: false, preview: -1, request: '', note: ''};
}
const sessions = new WeakMap<GameState, ReturnType<typeof session>>();
function editor(s: GameState) {
    let edit = sessions.get(s);
    if (!edit || edit.character !== s.selfId) { edit = session(s.selfId); sessions.set(s, edit); }
    return edit;
}
export async function openTale(s: GameState, id: string, redraw: () => void) {
    const edit = editor(s);
    if (edit.request) return;
    if (edit.dirty && !await ask('Discard your unsaved changes?', {ok: 'Discard changes'})) return;
    if (id) {
        edit.open = true; edit.note = 'Opening your draft…';
        const request = waitForReply(s, edit, redraw);
        s.send({type:'storyteller', verb:'edit', tale:id, request});
    } else Object.assign(edit, session(s.selfId), {open:true});
    redraw();
}
function waitForReply(s: GameState, edit: ReturnType<typeof session>, redraw: () => void) {
    const request = Math.random().toString(36).slice(2) + Date.now().toString(36);
    edit.request = request;
    window.setTimeout(() => {
        if (edit.request === request && str(s.taleSaveResult, 'request') !== request) {
            edit.request = ''; edit.note = 'The game has not answered. Check Your tales before trying again.'; redraw();
        }
    }, 15000);
    return request;
}
export function renderTaleEditor(panel: HTMLElement, s: GameState, redraw: () => void) {
    const edit = editor(s), reply = s.taleSaveResult;
    let justOpened = false;
    if (edit.request && str(reply, 'request') === edit.request) {
        if (reply && str(reply, 'type') === 'tale_draft') {
            Object.assign(edit, session(s.selfId), {draft: draft(reply, str(reply, 'tale')), open: true});
            justOpened = true;
        } else {
            if (bool(reply, 'ok')) { edit.draft.id = str(reply, 'tale'); edit.dirty = false; }
            edit.note = str(reply, 'message'); edit.request = '';
        }
    }
    const d = edit.draft;
    const form = el('details', 'letter-sheet', panel);
    form.open = edit.open;
    form.addEventListener('toggle', () => { if (form.isConnected) edit.open = form.open; });
    el('summary', 'label muted', form, d.id ? 'EDIT YOUR DRAFT' : 'WRITE A NEW TALE');
    const fields = el('fieldset', 'tale-fields', form);
    fields.disabled = !!edit.request;
    function field(parent: HTMLElement, label: string, value: string, update: (v: string) => void, key: string, long = false) {
        const wrap = el('label', 'tale-field', parent, label);
        const input = long ? el('textarea', 'profile-input', wrap) : el('input', 'profile-input', wrap);
        input.dataset.field = key; input.value = value;
        const commit = () => { update(input.value); edit.dirty = true; };
        input.addEventListener('input', commit);
        input.addEventListener('change', commit);
    }
    function change(fn: () => void) { fn(); edit.dirty = true; edit.open = true; redraw(); }
    field(fields, 'Title', d.title, v => { d.title = v; }, 'tale-title');
    field(fields, 'Premise', d.premise, v => { d.premise = v; }, 'premise', true);
    d.steps.forEach((step, i) => {
        const sheet = el('section', 'letter', fields);
        el('h3', 'label', sheet, 'Step ' + (i + 1));
        field(sheet, 'Journal heading', step.title, v => { step.title = v; }, 'step-' + i + '-title');
        field(sheet, 'What the player learns', step.text, v => { step.text = v; }, 'step-' + i + '-text');
        step.objectives.forEach((o, n) => {
            const key = 'objective-' + i + '-' + n;
            const label = el('label', 'tale-field', sheet, 'Objective ' + (n + 1));
            const sel = el('select', '', label);
            for (const [k, words] of Object.entries(kinds)) el('option', '', sel, words).value = k;
            sel.value = o.kind;
            sel.onchange = () => { o.kind = sel.value; edit.dirty = true; };
            field(sheet, 'Objective text', o.line, v => { o.line = v; }, key + '-line');
            field(sheet, 'Target ID (when applicable)', o.target, v => { o.target = v; }, key + '-target');
            if (step.objectives.length > 1) button('Remove objective ' + (n + 1), 'small', sheet, () => change(() => step.objectives.splice(n, 1)));
        });
        if (step.objectives.length < 3) button('Add objective', 'small', sheet, () => change(() => step.objectives.push(readStep().objectives[0])));
        if (d.steps.length > 1) button('Remove step', 'small', sheet, () => change(() => { d.steps.splice(i, 1); edit.preview = -1; }));
        button('Preview as player', 'small', sheet, () => { edit.preview = i; edit.open = true; redraw(); });
        if (edit.preview === i) {
            const preview = el('div', 'letter', sheet);
            el('div', 'label muted', preview, 'Player’s journal');
            const heading = el('strong', '', preview, step.title);
            const text = el('p', '', preview, step.text);
            const lines = step.objectives.map(o => el('div', 'small', preview, '· ' + (o.line || step.title)));
            sheet.addEventListener('input', () => {
                heading.textContent = step.title; text.textContent = step.text;
                step.objectives.forEach((o, index) => { lines[index].textContent = '· ' + (o.line || step.title); });
            });
        }
    });
    if (d.steps.length < 10) button('ADD A STEP', 'small', fields, () => change(() => d.steps.push(readStep())));
    d.cast.forEach((c, i) => {
        field(fields, 'Story character ' + (i + 1), c.name, v => { c.name = v; }, 'cast-' + i);
        field(fields, 'Appearance', c.looks, v => { c.looks = v; }, 'looks-' + i);
        button('Remove character', 'small', fields, () => change(() => d.cast.splice(i, 1)));
    });
    if (d.cast.length < 6) button('ADD A STORY CHARACTER', 'small', fields, () => change(() => d.cast.push({name: '', looks: ''})));
    button(d.id ? 'SAVE CHANGES' : 'SAVE THE DRAFT', 'primary', fields, () => {
        if (edit.request) return;
        const request = waitForReply(s, edit, redraw);
        edit.note = 'Waiting for the game…'; edit.open = true; fields.disabled = true;
        s.send({type: 'storyteller', verb: 'save', request, tale: d.id, title: d.title, premise: d.premise,
            chapter: d.chapter, cast: d.cast, steps: d.steps.map(step => ({...step, objectives: step.objectives.map(o => ({...o, line: o.line || step.title}))}))});
        redraw();
    });
    if (d.id) button('Write another tale', 'small', fields, () => void openTale(s, '', redraw));
    for (const b of fields.querySelectorAll('button')) b.tabIndex = 0;
    if (justOpened) fields.querySelector('input')?.focus();
    if (edit.note) el('p', 'small', form, edit.note).setAttribute('role', 'status');
}
