// Fights on the page (Docs/Design/33-combat.md): reading the arena, laying it over the cell, and what clicks and keys do
// in a fight. The rules themselves are the server's (Tests/battle_tests.cpp).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {draw, testGame} from './testing.ts';
import {arenaRows, arenaSight, chanceFrom, fightTips, clockLabel, fighterAt, meterNow, myTurn, octantGap, pathTo, quarter, readBattle, readChallenge, readFights, secondsToTurn,
    stepToward, termsWords} from './battle.ts';
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
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'move', x: 4, y: 2}, 'a foe out of reach: walk up to them first');
    const walking = commands.length;
    s.applySnapshot(snapshot());
    assert.equal(commands.length, walking, 'no blow while still walking');
    const arrived = (battle.fighters as Json[]).map(f => (f.id === 'self' ? {...f, x: 4} : f));
    s.applySnapshot(snapshot({battle: {...battle, moved: true, fighters: arrived}}));
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'bite', target: 'bo'}, 'then, there, the bite');
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
    const step = commands.at(-1) as Json;
    assert.equal(step.verb, 'move', 'out of a sword\'s reach: walk in first');
    const there = (battle.fighters as Json[]).map(f => (f.id === 'self' ? {...f, x: step.x, y: step.y} : f));
    s.applySnapshot(snapshot({battle: {...battle, moved: true, fighters: there, you: {...(battle.you as Json), mouth: 'sword', gift: 'fire',
        mana: 30, flameLength: 3, flameAngle: 23, flameMana: 25}}}));
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'sword', target: 'bo'}, 'and strike with it on arrival');
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

test('fights: arrows round your wolf to face, initiative bars that fill between updates', () => {
    const {state: s, commands, painter} = testGame();
    s.applySnapshot(snapshot({battle: {...battle, fighters: (battle.fighters as Json[]).map(f => ({...f, meter: 40, rate: 20}))}}));
    s.mapRect = rect(0, 0, 800, 600);
    draw(painter, 'drawLocal');
    const arrows = s.hits.filter(h => h.action === 'face');
    assert.equal(arrows.length, 8, 'eight ways to face, on your turn');
    s.activate(arrows[2]);
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'face', dir: 2});
    const bo = s.battle!.fighters.find(f => f.id === 'bo')!;
    assert.ok(Math.abs(meterNow(bo, 1.5) - 0.7) < 1e-9, 'a bar runs on between updates: 40 + 20 a second');
    assert.equal(meterNow(bo, 10), 1, 'and stops full');
    s.applySnapshot(snapshot({battle: {...battle, turn: 'bo'}}));
    s.hits = [];
    draw(painter, 'drawLocal');
    assert.equal(s.hits.filter(h => h.action === 'face').length, 0, 'not your turn: no turning');
    const before = commands.length;
    s.keyDown({code: 'KeyW'});
    assert.equal(commands.length, before, 'no WASD in a fight');
    assert.match(s.toast, /click a lit tile/, 'but a hint how to move');
});

test('the fight screen\'s data: looks, breath and mana for one\'s side, odds against a foe, time to the next turn (doc 37)', () => {
    const b = readBattle({battle: {...battle, fighters: [
        {id: 'self', name: 'Ada', side: 0, x: 2, y: 2, status: 'fighting', health: 90, stamina: 64, mana: 30, manaMax: 44,
            appearance: {species: 'timber'}, lifeStage: 'adult'},
        {id: 'bo', name: 'Bo', side: 1, x: 3, y: 2, status: 'fighting', health: 60, meter: 40, rate: 6, odds: {hit: 85, damage: 12, reach: true}},
    ]}})!;
    const [ada, bo] = b.fighters;
    assert.equal(ada.stamina, 64);
    assert.equal(ada.mana, 30);
    assert.equal(ada.manaMax, 44);
    assert.deepEqual(ada.appearance, {species: 'timber'});
    assert.equal(bo.stamina, -1, "the other side's breath is not sent");
    assert.deepEqual(bo.odds, {hit: 85, base: 85, damage: 12, reach: true}, 'no base sent: the hit itself');
    assert.equal(ada.odds, null);
    assert.equal(secondsToTurn(bo, 0), 10, 'from 40, at 6 a second: ten seconds');
    assert.equal(secondsToTurn(bo, 4), 6, 'four seconds on: six');
    assert.equal(secondsToTurn({...bo, acting: true}, 0), 0, 'acting now');
});

test('the arena preview (doc 37, phase 2): the chance from any tile, the step a click would take, the way there', () => {
    // Bo at (5, 2) faces west (4): Ada comes at him from the front, his side, or behind.
    const b = readBattle({battle: {...battle, fighters: [
        {id: 'self', name: 'Ada', side: 0, x: 2, y: 2, facing: 0, status: 'fighting', health: 90},
        {id: 'bo', name: 'Bo', side: 1, x: 5, y: 2, facing: 4, status: 'fighting', health: 60, odds: {hit: 75, base: 75, damage: 12, reach: false}},
    ], reach: [[3, 2], [4, 2], [3, 1], [4, 1], [4, 3], [5, 1], [5, 3], [6, 2], [6, 1]]}})!;
    const [ada, bo] = b.fighters;
    assert.equal(octantGap(0, 4), 4);
    assert.equal(octantGap(7, 1), 2);
    assert.equal(quarter(bo, 4, 2), 'front');
    assert.equal(quarter(bo, 5, 1), 'side');
    assert.equal(quarter(bo, 6, 2), 'back');
    assert.equal(chanceFrom(bo, 4, 2), 75, 'head on: the base');
    assert.equal(chanceFrom(bo, 5, 1), 85, 'from the side: +10');
    assert.equal(chanceFrom(bo, 6, 2), 95, 'from behind: +20, at most 95');
    const step = stepToward(b, ada, bo, 1);
    assert.deepEqual(step, {x: 4, y: 2, reaches: true}, 'a click on Bo steps to the nearest lit tile beside him');
    const way = pathTo(b, [2, 2], [6, 2]);
    assert.deepEqual(way[0], [2, 2]);
    assert.deepEqual(way.at(-1), [6, 2]);
    assert.ok(way.every(([x, y]) => (x === 2 && y === 2) || b.reach.some(([rx, ry]) => rx === x && ry === y)), 'only through lit tiles');
    assert.ok(!way.some(([x, y]) => x === 5 && y === 2), 'never through Bo');
});

test('turning by dragging from one\'s own wolf (doc 37, phase 2); a plain click on it is still a click', () => {
    const {state: s, commands, painter} = testGame();
    s.applySnapshot(snapshot());
    s.mapRect = rect(0, 0, 800, 600);
    draw(painter, 'drawLocal');
    const me = [s.mapOrigin[0] + 2.5 * s.tileSize, s.mapOrigin[1] + 2.5 * s.tileSize] as [number, number];
    s.mouseDown(me, true, false, false);
    assert.ok(s.faceDrag, 'pressing on her own wolf begins a drag');
    s.mouseMove([me[0], me[1] - s.tileSize * 2], false);
    assert.equal(s.faceDragDir, 6, 'pointing north');
    s.mouseUp();
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'face', dir: 6}, 'let go: she faces north');
    assert.equal(s.faceDrag, null);
    // A press and release on the east edge of her tile, without moving: the arrow there, as before.
    const before = commands.length;
    s.mouseDown([me[0] + s.tileSize * 0.33, me[1]], true, false, false);
    s.mouseUp();
    assert.deepEqual(commands.slice(before), [{type: 'battle', verb: 'face', dir: 0}], 'the east arrow');
});

test('a challenge names its terms (doc 37, phase 4): the menu offers them, the default first; the fight carries them', () => {
    const {state: s, commands} = testGame();
    s.applySnapshot({self: {id: 'self', name: 'Ada', x: 5, y: 5}, cell: {id: 'plaza'},
        entities: [{id: 'player-bo', kind: 'player', name: 'A dun wolf', x: 6, y: 5, actions: ['inspect', 'challenge']}]});
    s.contextTarget = 'player-bo';
    s.activate({rect: rect(0, 0, 0, 0), action: 'context', target: 'challenge'});
    assert.deepEqual(s.contextActions, ['challenge:yield', 'challenge:blood', 'challenge:death'], 'the terms, until one yields first');
    assert.ok(!commands.some(c => c.type === 'action'), 'nothing sent yet');
    s.activate({rect: rect(0, 0, 0, 0), action: 'context', target: 'challenge:blood'});
    assert.deepEqual(commands.at(-1), {type: 'action', action: 'challenge', target: 'player-bo', terms: 'blood'});
    assert.equal(readChallenge({challenge: {from: 'player-bo', name: 'A dun wolf', left: 20, terms: 'death'}})!.terms, 'death');
    const b = readBattle({battle: {...battle, terms: 'yield', crime: false, yieldBy: 'bo'}})!;
    assert.equal(b.terms, 'yield');
    assert.equal(b.yieldBy, 'bo');
    assert.equal(termsWords('blood'), 'to first blood');
    assert.equal(termsWords(readBattle({battle})!.terms), 'until one goes down', 'a fight without terms is until one goes down (doc 38)');
});

test('planning ahead (doc 37, phase 5): while one\'s bar fills, a tile, a foe or a fallen friend clicked is planned', () => {
    const {state: s, commands} = testGame();
    const waiting: Json = {...battle, turn: '', turnLeft: 0, planning: true, haste: 2.5, reach: [[3, 2], [4, 2]]};
    s.applySnapshot(snapshot({battle: waiting}));
    const b = s.battle!;
    assert.ok(b.planning && b.plan === null && b.haste === 2.5, 'waiting, no plan yet, the bars hastened');
    s.arenaClick(3, 2);
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'plan', x: 3, y: 2}, 'a lit tile: planned, not moved to');
    s.fightTarget('bo');
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'plan', act: 'bite', target: 'bo'}, 'a foe: a bite planned');
    s.fightTarget('cy');
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'plan', act: 'tend', target: 'cy'}, 'a fallen friend: tending planned');
    // Planned already: the server says so, and the same again takes it back.
    s.applySnapshot(snapshot({battle: {...waiting, you: {...(battle.you as Json), plan: {x: 3, y: 2, act: 'bite', target: 'bo'}}}}));
    assert.deepEqual(s.battle!.plan, {move: [3, 2], act: 'bite', target: 'bo'}, 'the plan, read');
    s.fightTarget('bo');
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'unplan', part: 'act'}, 'the bite clicked again: taken back');
    s.arenaClick(3, 2);
    assert.deepEqual(commands.at(-1), {type: 'battle', verb: 'unplan', part: 'move'}, 'the tile clicked again: taken back');
    s.applySnapshot(snapshot({battle: {...waiting, you: {...(battle.you as Json), plan: {act: 'rest', target: ''}}}}));
    assert.deepEqual(s.battle!.plan, {move: null, act: 'rest', target: ''}, 'a plan without a move');
    // Walking, a fighter shows where it is going.
    s.applySnapshot(snapshot({battle: {...battle, fighters: (battle.fighters as Json[]).map(f => (f.id === 'bo' ? {...f, walk: [[4, 2], [3, 2]]} : f))}}));
    assert.deepEqual(s.battle!.fighters.find(f => f.id === 'bo')!.walk, [[4, 2], [3, 2]], "Bo's way there");
});

test('guard and shove (doc 37, phase 6): one on guard has no side or back to strike; the sword is taken with the move', () => {
    const odds = {hit: 55, base: 55, damage: 12, reach: true};
    const b = readBattle({battle: {...battle, drew: true, fighters: [
        {id: 'self', name: 'Ada', side: 0, x: 2, y: 2, facing: 0, status: 'fighting'},
        {id: 'bo', name: 'Bo', side: 1, x: 3, y: 2, facing: 0, status: 'fighting', guarding: true, odds},
    ]}})!;
    const bo = b.fighters[1];
    assert.ok(bo.guarding && b.drew, 'on guard, and a sword drawn this turn, read');
    assert.equal(chanceFrom(bo, 2, 2), 55, 'from behind him, still only the head-on chance');
    assert.equal(chanceFrom({...bo, guarding: false}, 2, 2), 75, '(not on guard: +20 from behind)');
});

test('the first fights\' tips (doc 37, phase 7): which fit when, kept once per character, and off in Settings', () => {
    const b = readBattle({battle})!;
    const me = b.fighters[0];
    assert.deepEqual(fightTips(b, me, true).map(t => t.id), ['turn', 'odds'], 'one\'s turn: the turn, then the odds');
    assert.deepEqual(fightTips({...b, moved: true}, me, true).map(t => t.id), ['turn', 'odds', 'end'], 'moved: End turn ends it sooner');
    assert.deepEqual(fightTips({...b, planning: true}, me, false).map(t => t.id), ['plan'], 'waiting: plan it');
    assert.deepEqual(fightTips(b, {...me, status: 'downed'}, true), [], 'none for the Downed');
    const {state: s} = testGame();
    s.selfId = 'self';
    assert.deepEqual(s.tipsSeen(), []);
    s.markTipSeen('turn');
    assert.deepEqual(s.tipsSeen(), ['turn'], 'seen once');
    s.activate({rect: rect(0, 0, 0, 0), action: 'fighttips', target: ''});
    assert.equal(s.fightTips, false, 'Settings: off');
    s.activate({rect: rect(0, 0, 0, 0), action: 'fighttips', target: ''});
    assert.ok(s.fightTips && s.tipsSeen().length === 0, 'and on again: shown again');
});

test('armour in fights (doc 35, Part 8): what a fighter\'s armour takes off a cut and a bite, and off its bar', () => {
    const b = readBattle({battle: {...battle, fighters: [
        {id: 'bo', name: 'Bo', side: 1, x: 3, y: 2, status: 'fighting', armour: {cut: 9, thrust: 10, dex: -5}},
        {id: 'cy', name: 'Cy', side: 1, x: 4, y: 2, status: 'fighting'},
    ]}})!;
    assert.deepEqual(b.fighters[0].armour, {cut: 9, thrust: 10, dex: -5});
    assert.equal(b.fighters[1].armour, null, 'none worn');
});
