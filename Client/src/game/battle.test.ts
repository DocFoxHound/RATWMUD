// Fights on the page (Docs/Design/33-combat.md): reading the arena, laying it over the cell, and what clicks and keys do
// in a fight. The rules themselves are the server's (Tests/battle_tests.cpp).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {draw, testGame} from './testing.ts';
import {arenaRows, arenaSight, clockLabel, fighterAt, myTurn, readBattle, readFights} from './battle.ts';
import {rect} from '../ui/painter.ts';
import type {Json} from './json.ts';

const rows = ['##########', '#........#', '#........#', '#........#', '##########'];
const battle: Json = {
    id: 'fight-1', over: false, banner: '', arena: {x: 1, y: 1, w: 8, h: 3},
    rows: ['........', '..,,....', '........'],
    you: {observer: false, side: 0, status: 'fighting', struggling: false, canStruggle: false},
    turn: 'self', turnName: 'Ada', turnLeft: 21.4, moved: false, acted: false, round: 3, watching: 1,
    fighters: [
        {id: 'self', name: 'Ada', side: 0, x: 2, y: 2, facing: 0, status: 'fighting', npc: false, label: 'Scratched', health: 90},
        {id: 'bo', name: 'Bo', side: 1, x: 5, y: 2, facing: 4, status: 'fighting', npc: false, label: 'Wounded', health: 60},
        {id: 'cy', name: 'Cy', side: 0, x: 2, y: 3, facing: 0, status: 'downed', npc: false, label: 'Downed', health: 0, downedLeft: 540},
    ],
    order: ['self', 'bo', 'self'], reach: [[3, 2], [4, 2], [3, 1]],
    log: [{seq: 4, kind: 'hit', text: 'Bo bites Ada (6).'}],
};
const snapshot = (extra: Json = {}): Json => ({
    cell: {id: 'room', width: 10, height: 5, rows}, visibility: rows.map(r => '2'.repeat(r.length)),
    self: {id: 'self', x: 2.5, y: 2.5}, entities: [{id: 'self', x: 2.5, y: 2.5}], battle, ...extra,
});

test('fights: the arena is read whole, laid over the cell, and seen whole', () => {
    const b = readBattle({battle})!;
    assert.equal(b.fighters.length, 3);
    assert.deepEqual(b.reach[0], [3, 2]);
    assert.ok(myTurn(b, 'self') && !myTurn(b, 'bo'), 'whose turn');
    assert.equal(fighterAt(b, 5, 2)?.id, 'bo');
    assert.equal(fighterAt(b, 9, 9), undefined);
    const laid = arenaRows(rows, b);
    assert.equal(laid[2], '#..,,....#', "the arena's own ground, in place");
    assert.equal(laid[0], rows[0], 'outside it, unchanged');
    const seen = arenaSight(10, 5, b);
    assert.deepEqual(seen, ['0000000000', '0222222220', '0222222220', '0222222220', '0000000000'], 'only the arena, all of it');
    assert.equal(readBattle({}), null);
    assert.equal(clockLabel(540), '9:00');
    assert.deepEqual(readFights({fights: [{id: 'f', x0: 1, y0: 2, x1: 4, y1: 5, standing0: 1, standing1: 2, side0: 'A', side1: 'B',
        canJoin: true}]})[0].names, ['A', 'B']);
});

test('fights: no walking; a lit tile moves, a foe bites, a fallen friend is tended', () => {
    const {state: s, commands} = testGame();
    s.applySnapshot(snapshot());
    assert.ok(s.battle, 'in a fight');
    assert.equal(s.tileRows[2], '#..,,....#', "the page draws the arena's ground");
    assert.equal(s.visibilityRows[0], '0000000000', 'and nothing outside it');
    const before = commands.length;
    assert.ok(s.keyDown({code: 'KeyW'}), 'WASD is taken');
    assert.equal(commands.length, before, 'but nothing walks');
    s.arenaClick(3, 2);
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'move', x: 3, y: 2}, 'a lit tile: move there');
    s.arenaClick(8, 3);
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'move', x: 3, y: 2}, 'an unlit one: nothing');
    s.arenaClick(5, 2);
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'bite', target: 'bo'}, 'a foe: bite');
    s.fightTarget('cy');
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'tend', target: 'cy'}, 'a friend who is down: tend');
    // Over, then gone: back to the world's own ground and sight, everyone fading in.
    s.clock = 10;
    s.applySnapshot(snapshot({battle: {...battle, over: true, banner: "The fight is over · Ada's side stands"}}));
    assert.equal(s.battleOverSeenAt, 10);
    s.clock = 13;
    s.applySnapshot(snapshot({battle: undefined}));
    assert.equal(s.battle, null);
    assert.equal(s.tileRows[2], rows[2], "the cell's own ground again");
    assert.equal(s.visibilityRows[0], '2222222222', "and the world's sight");
    assert.equal(s.fightEndedAt, 13, 'the fade back in starts');
});

test('fights: the arena is drawn, fighters can be clicked, and others see a red square', () => {
    const {state: s, painter} = testGame();
    s.applySnapshot(snapshot());
    s.mapRect = rect(0, 0, 800, 600);
    draw(painter, 'drawLocal');
    const hits = s.hits.filter(h => h.action === 'fighter').map(h => h.target).sort();
    assert.deepEqual(hits, ['bo', 'cy', 'self'], 'every fighter is a click target');
    assert.ok(!s.hits.some(h => h.action === 'target' && h.target === 'self'), "the world's wolves aren't drawn in the arena");
    s.applySnapshot(snapshot({battle: undefined, fights: [{id: 'fight-2', x0: 1, y0: 1, x1: 4, y1: 3, standing0: 1, standing1: 1}]}));
    assert.equal(s.fights.length, 1, 'an onlooker sees the fight');
    draw(painter, 'drawLocal');
});

test('fights: turning is free and only on your turn; a sword strikes; fire is aimed', () => {
    const {state: s, commands} = testGame();
    s.applySnapshot(snapshot());
    s.keyDown({code: 'KeyE'});
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'face', dir: 1}, 'E turns right an eighth');
    s.keyDown({code: 'KeyQ'});
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'face', dir: 7}, 'Q turns left');
    s.arenaFace(2, 1);
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'face', dir: 6}, 'facing a tile: north');
    // Not her turn: no turning.
    s.applySnapshot(snapshot({battle: {...battle, turn: 'bo'}}));
    const before = commands.length;
    s.keyDown({code: 'KeyE'});
    assert.equal(commands.length, before, 'locked until her turn comes again');
    // A sword in the jaws: a click on a foe strikes with it.
    s.applySnapshot(snapshot({battle: {...battle, you: {...(battle.you as Json), mouth: 'sword', gift: 'fire', mana: 30, flameLength: 3,
        flameAngle: 23, flameMana: 25}}}));
    s.fightTarget('bo');
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'sword', target: 'bo'});
    // Fire: aim, then a click sends it where it goes.
    s.aiming = 'flame';
    s.arenaClick(5, 2);
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'flame', x: 5, y: 2});
    assert.equal(s.aiming, '', 'aimed once');
});

test('fights: one story entry per fight, kept up to date; blows nudge the W', () => {
    const {state: s} = testGame();
    s.applySnapshot(snapshot());
    const entry = s.posts.find(p => p.encounter);
    assert.ok(entry, 'the fight has an entry in the story');
    assert.match(entry!.text, /Latest: Bo bites Ada/);
    s.clock = 1;
    const hit = {seq: 5, kind: 'hit', text: 'Ada bites Bo (7).', actor: 'self', target: 'bo'};
    s.applySnapshot(snapshot({battle: {...battle, log: [...(battle.log as Json[]), hit]}}));
    assert.equal(s.posts.filter(p => p.encounter).length, 1, 'still one entry');
    assert.match(entry!.text, /Latest: Ada bites Bo/);
    assert.equal(entry!.encounter!.lines.length, 2, 'every line kept for Expand');
    s.clock = 1.12;
    const [dx] = s.fx.offset('self', s.clock, false);
    const [bx] = s.fx.offset('bo', s.clock, false);
    assert.ok(dx > 0.1 && bx > 0.1, 'Ada lunges toward Bo, and Bo recoils away');
    assert.deepEqual(s.fx.offset('self', s.clock, true), [0, 0], 'reduced motion: no slide');
    assert.ok(s.fx.marks(s.battle!, s.clock, false).some(m => m.glyph === '*'), 'and a mark where it landed');
    s.applySnapshot(snapshot({battle: undefined}));
    assert.ok(entry!.encounter!.over && /ended/.test(entry!.text), 'when it ends, the entry says so');
});
