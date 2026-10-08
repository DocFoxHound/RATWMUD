// Festivals (Docs/Design/54-gathering-places.md, 6): a festival soon (in how many days), or today's programme with each
// contest's state and winner, ENTER while a contest is open (a penny), and what one does now: the race's next mark,
// PULL on the tug's beat, HOWL in one's turn and CHEER another, STAR a teller at the storytelling.
import type {GameState} from '../../game/state.ts';
import {arr, bool, isObject, num, obj, str} from '../../game/json.ts';
import {button, el, setText, show} from './dom.ts';

const Contests = ['race', 'tug', 'howl', 'tourney', 'hunt', 'story'];

/** A programme line: "13:00 Races · under way", with the winner when there is one. */
export function programmeLine(p: Record<string, unknown>): string {
    const state = str(p, 'state'), winner = str(p, 'winner');
    return `${String(num(p, 'hour')).padStart(2, '0')}:00 ${str(p, 'what')}` + (winner ? ` · ${winner === 'none' ? 'no winner' : `won by ${winner}`}` : state ? ` · ${state}` : '');
}

export class FestivalPanel {
    readonly root: HTMLElement;
    private s: GameState;
    private title: HTMLElement;
    private body: HTMLElement;
    private key = '';

    constructor(parent: HTMLElement, state: GameState) {
        this.s = state;
        this.root = el('section', 'panel festival', parent);
        const head = el('div', 'panel-head', this.root);
        this.title = el('span', 'label gold', head, 'FESTIVAL');
        this.body = el('div', '', this.root);
    }

    update() {
        const f = obj(obj(this.s.snapshot, 'self'), 'festival');
        show(this.root, !!f);
        if (!f) return;
        const now = obj(f, 'now');
        // (The tug's beat changes five times a second: drawn every time it does.)
        const key = JSON.stringify(f);
        if (key === this.key) return;
        this.key = key;
        this.body.replaceChildren();
        const send = (fields: Record<string, unknown>) => this.s.send({type: 'festival', ...fields});
        const days = num(f, 'inDays');
        setText(this.title, `${str(f, 'name').toUpperCase()}${days ? ` · IN ${days} ${days === 1 ? 'DAY' : 'DAYS'}` : ''}`);
        if (days) {
            el('div', 'muted small', this.body, 'A festival is coming: the programme is on the board by the square.');
            return;
        }
        if (now) {
            const kind = str(now, 'kind');
            const box = el('div', 'story-row', this.body);
            if (kind === 'race')
                el('span', 'small gold', box, `RACE · mark ${num(now, 'mark')} of ${num(now, 'marks')} at ${Math.floor(num(now, 'x'))}, ${Math.floor(num(now, 'y'))} · ${num(now, 'seconds')}s`);
            else if (kind === 'tug') {
                el('span', 'small gold', box, `TUG · your team ${num(now, 'team') + 1} · rope ${num(now, 'marker') > 0 ? '◂' : num(now, 'marker') < 0 ? '▸' : '·'} ${Math.abs(num(now, 'marker')).toFixed(1)}`);
                const pull = button('PULL', 'small', box, () => send({verb: 'pull'}));
                pull.style.outline = num(now, 'beatIn') < 0.35 || num(now, 'beatIn') > 0.85 ? '2px solid currentColor' : '';
            } else if (kind === 'howl') {
                el('span', 'small gold', box, `HOWLING · ${str(now, 'whose')}${bool(now, 'mine') ? ': your turn!' : ''}`);
                if (bool(now, 'mine')) button('HOWL', 'small', box, () => send({verb: 'howl'}));
                if (!bool(now, 'cheered') && bool(f, 'here'))
                    for (const h of arr(now, 'field').filter(isObject))
                        button(`CHEER ${str(h, 'name')}`, 'small', this.body, () => send({verb: 'cheer', target: str(h, 'id')}));
            } else if (kind === 'story') {
                el('span', 'small gold', box, str(now, 'whose') ? `STORYTELLING · ${str(now, 'whose')} at the middle` : 'STORYTELLING · star the best tale');
                if (!bool(now, 'starred') && bool(f, 'here'))
                    for (const h of arr(now, 'field').filter(isObject))
                        button(`STAR ${str(h, 'name')}`, 'small', this.body, () => send({verb: 'star', target: str(h, 'id')}));
            }
        }
        for (const p of arr(f, 'programme').filter(isObject)) {
            const row = el('div', 'story-row', this.body);
            el('span', 'small', row, programmeLine(p));
            if (Contests.includes(str(p, 'kind')) && str(p, 'state') === 'open' && !bool(p, 'entered'))
                button('ENTER · 1p', 'small', row, () => send({verb: 'enter', contest: str(p, 'kind')}));
            else if (bool(p, 'entered') && str(p, 'state') === 'open') el('span', 'muted small', row, 'entered');
        }
        el('div', 'muted small', this.body, bool(f, 'here') ? 'At the square: the feast, and rest that builds all day.' : 'The festival is at the square.');
    }
}
