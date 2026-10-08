// Tavern games (Docs/Design/54-gathering-places.md, 5): at a table in a common room, setting out a game (Knucklebones,
// Wolves and Deer, Liar's Bones, the last for a stake), the seats (join, ask the room, begin, leave), and the game as it
// goes: Knucklebones' rounds with TRY and BANK, Wolves and Deer's board (a piece, then where it goes), Liar's Bones'
// own bones with BID and CALL LIAR. The table's last lines below. Hidden away from tables.
import type {GameState} from '../../game/state.ts';
import {arr, bool, isObject, num, obj, str, type Maybe} from '../../game/json.ts';
import {boardRows, dieFace, knucklebonesLine, nextBid, nextRound, targetsFrom} from '../../game/tableGames.ts';
import {button, el, setText, show} from './dom.ts';

export class TablePanel {
    readonly root: HTMLElement;
    private s: GameState;
    private title: HTMLElement;
    private body: HTMLElement;
    private key = '';
    private picked = -1;                            // Wolves and Deer: the piece chosen to move.
    private bidFace = 0;                            // Liar's Bones: the face to bid on.

    constructor(parent: HTMLElement, state: GameState) {
        this.s = state;
        this.root = el('section', 'panel table', parent);
        const head = el('div', 'panel-head', this.root);
        this.title = el('span', 'label gold', head, 'AT THE TABLE');
        this.body = el('div', '', this.root);
    }

    update() {
        const table = obj(obj(this.s.snapshot, 'self'), 'table');
        show(this.root, !!table);
        if (!table) return;
        const key = JSON.stringify([table, this.picked, this.bidFace]);
        if (key === this.key) return;
        this.key = key;
        this.body.replaceChildren();
        const send = (fields: Record<string, unknown>) => this.s.send({type: 'table', ...fields});
        if (bool(table, 'offer')) {
            setText(this.title, 'A TABLE');
            el('div', 'muted small', this.body, 'Set out a game, then ask someone to play or ask the room.');
            const row = el('div', 'story-row', this.body);
            button('KNUCKLEBONES', 'small', row, () => send({verb: 'start', game: 'knucklebones'}));
            button('WOLVES AND DEER', 'small', row, () => send({verb: 'start', game: 'wolves'}));
            const liars = el('div', 'story-row', this.body);
            const stake = el('select', '', liars);
            for (let p = 0; p <= 5; ++p) el('option', '', stake, p ? `${p}p each` : 'no stake').value = String(p);
            button('LIAR\'S BONES', 'small', liars, () => send({verb: 'start', game: 'liars', stake: Number(stake.value)}));
            return;
        }
        const seats = arr(table, 'seats').filter(isObject);
        const seat = num(table, 'seat', -1), turn = num(table, 'turn', -1), begun = bool(table, 'begun');
        const myTurn = begun && seat >= 0 && seat === turn;
        setText(this.title, `${str(table, 'name').toUpperCase()}${num(table, 'stake') > 0 ? ` · ${num(table, 'stake')}p EACH` : ''}`);
        el('div', 'small', this.body, seats.map((p: Maybe, i: number) => `${str(p, 'name')}${i === turn ? ' ◂' : ''}`).join(' · ') || 'Nobody seated.');
        if (!begun) {
            if (seat >= 0) {
                const row = el('div', 'story-row', this.body);
                if (seats.length < num(table, 'max', 2)) button('ASK THE ROOM', 'small', row, () => send({verb: 'resident'}))
                    .title = 'Someone in the room who is free takes a seat';
                if (seats.length >= 2) button('BEGIN', 'small', row, () => send({verb: 'begin'}));
                button('LEAVE', 'small', row, () => send({verb: 'leave'}));
            } else if (seats.length > 0 && seats.length < num(table, 'max', 2))
                button('JOIN', 'small', this.body, () => send({verb: 'join'}));
        } else {
            const state = obj(table, 'state');
            const game = str(table, 'game');
            if (game === 'knucklebones') this.knucklebones(state, seats, seat, myTurn, send);
            else if (game === 'wolves') this.wolves(state, seat, myTurn, send);
            else this.liars(state, seats, myTurn, send);
            if (seat >= 0) button('LEAVE THE GAME', 'small', this.body, () => {
                if (num(table, 'stake') <= 0 || window.confirm('Leave the game? Your stake stays in the pot.')) send({verb: 'leave'});
            });
        }
        const log = el('div', 'muted small', this.body);
        for (const line of arr(table, 'log').slice(-6)) el('div', '', log, String(line));
    }

    private knucklebones(state: Maybe, seats: Maybe[], seat: number, myTurn: boolean, send: (f: Record<string, unknown>) => void) {
        const banked = arr(state, 'banked'), at = arr(state, 'at');
        seats.forEach((p: Maybe, i: number) => el('div', 'small', this.body, `${str(p, 'name')}: ${knucklebonesLine(Number(banked[i]) || 0, Number(at[i]) || 0)}`));
        if (!myTurn) return;
        const mine = Number(at[seat]) || 0;
        const row = el('div', 'story-row', this.body);
        button(`TRY ${nextRound(mine).toUpperCase()} · ${num(state, 'chance')}%`, 'small', row, () => send({verb: 'move', action: 'try'}));
        if (mine > (Number(banked[seat]) || 0)) button('BANK', 'small', row, () => send({verb: 'move', action: 'bank'}));
    }

    private wolves(state: Maybe, seat: number, myTurn: boolean, send: (f: Record<string, unknown>) => void) {
        el('div', 'small', this.body, `You are the ${seat === 0 ? 'pack (W)' : seat === 1 ? 'herd (D)' : '—'} · deer taken ${num(state, 'taken')} of 7`);
        const moves = arr(state, 'moves').filter(Array.isArray) as number[][];
        const targets = this.picked >= 0 ? targetsFrom(moves, this.picked) : [];
        const grid = el('div', 'wolves-board', this.body);
        grid.style.display = 'grid';
        grid.style.gridTemplateColumns = 'repeat(7, 1.6em)';
        grid.style.gap = '2px';
        grid.style.margin = '6px 0';
        boardRows(str(state, 'points')).forEach((row, y) => row.forEach((c, x) => {
            const p = y * 7 + x;
            const cell = el('button', 'small', grid, c === ' ' ? '' : c === '.' ? '·' : c);
            cell.style.padding = '0';
            cell.style.height = '1.6em';
            if (c === ' ') { cell.style.visibility = 'hidden'; return; }
            const movable = myTurn && moves.some(m => m[0] === p);
            const target = targets.includes(p);
            if (p === this.picked) cell.style.outline = '2px solid currentColor';
            if (target) cell.style.background = 'rgba(200, 170, 90, 0.35)';
            cell.disabled = !movable && !target;
            cell.addEventListener('click', () => {
                if (target) { send({verb: 'move', from: this.picked, to: p}); this.picked = -1; }
                else this.picked = this.picked === p ? -1 : p;
                this.key = '';
                this.update();
            });
        }));
        if (myTurn && num(state, 'chain', -1) >= 0) button('STOP JUMPING', 'small', this.body, () => send({verb: 'move', action: 'stop'}));
        if (!myTurn) this.picked = -1;
    }

    private liars(state: Maybe, seats: Maybe[], myTurn: boolean, send: (f: Record<string, unknown>) => void) {
        const counts = arr(state, 'counts');
        el('div', 'small', this.body, seats.map((p: Maybe, i: number) => `${str(p, 'name')} ${Number(counts[i]) || 0}🦴`).join(' · '));
        const mine = arr(state, 'mine').map(Number);
        if (mine.length) el('div', '', this.body, `Your bones: ${mine.map(dieFace).join(' ')}`).style.fontSize = '1.3em';
        const count = num(state, 'count'), face = num(state, 'face');
        el('div', 'small', this.body, count > 0 ? `The bid: ${count} showing ${dieFace(face)} (${face})` : 'No bid yet this round.');
        if (!myTurn) return;
        if (!this.bidFace) this.bidFace = mine[0] || 1;
        const row = el('div', 'story-row', this.body);
        for (let f = 1; f <= 6; ++f) {
            const b = button(dieFace(f), 'small', row, () => { this.bidFace = f; this.key = ''; this.update(); });
            if (f === this.bidFace) b.style.outline = '2px solid currentColor';
        }
        const bid = nextBid(count, face, this.bidFace, num(state, 'total'));
        const act = el('div', 'story-row', this.body);
        if (bid) button(`BID ${bid[0]} × ${dieFace(bid[1])}`, 'small', act, () => { send({verb: 'move', action: 'bid', count: bid[0], face: bid[1]}); this.bidFace = 0; });
        if (count > 0) button('CALL LIAR', 'small', act, () => send({verb: 'move', action: 'call'}));
    }
}
