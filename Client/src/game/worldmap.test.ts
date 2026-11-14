// The user's notes of 2026-11-14 for the map: the whole realm on the World Map, zoomed, named as it is zoomed in, and
// buildings' icons; walking into a doorway with a click; stairs that don't glow at night; speech bubbles.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {draw, testGame} from './testing.ts';
import type {Json} from './json.ts';
import {IconScale, labelsAt, readRealm, realmFit} from './realm.ts';
import {iconFor, MapIcons} from './mapIcons.ts';
import {BubbleChars, bubbleSeconds, bubbleText, dotLift, spokenWords, stackBubbles} from './bubbles.ts';
import {isStairs} from './paint.ts';

const realmEvent: Json = {
    type: 'realmMap', build: 'build-73', x0: 0, y0: 0, step: 64, cols: 4, rows: 2, ground: ',,~~##__',
    labels: [{n: 'Ridgemere', k: 'city', r: 0, x: 4, y: 4}, {n: 'Frosthollow', k: 'town', r: 1, x: 12, y: 2},
        {n: 'Northgate', k: 'district', r: 2, x: 6, y: 6}, {n: '', k: 'district', r: 2, x: 0, y: 0}],
    buildings: [{n: 'The Grey Goose', k: 'inn', x: 5, y: 5}, {n: 'Moss House', k: 'house', x: 7, y: 5}, {n: 'Shed', x: 1, y: 1}],
};

test('the realm map is read from its one event: ground, names by rank, buildings', () => {
    const realm = readRealm(realmEvent);
    assert.ok(realm, 'a whole map reads');
    assert.equal(realm.ground.length, realm.cols * realm.rows, 'every sample');
    assert.deepEqual(realm.labels.map(l => l.name), ['Ridgemere', 'Frosthollow', 'Northgate'], 'a nameless label is dropped');
    assert.equal(realm.buildings[2].kind, '', 'a building with nothing to say has no kind (and no icon)');
    assert.equal(readRealm({...realmEvent, ground: ',,'}), null, 'ground of the wrong size is no map');
    assert.equal(readRealm({type: 'realmMap', pending: true}), null, 'nor is "still being made"');
    // Names come as the map is zoomed in, larger places first.
    assert.deepEqual(labelsAt(realm, 0.1).map(l => l.name), [], 'the whole realm very small: no names');
    assert.deepEqual(labelsAt(realm, 0.2).map(l => l.name), ['Ridgemere'], 'the whole realm: the regions and the city');
    assert.deepEqual(labelsAt(realm, 0.4).map(l => l.name), ['Ridgemere', 'Frosthollow'], 'then the towns');
    assert.deepEqual(labelsAt(realm, 0.7).map(l => l.name), ['Ridgemere', 'Frosthollow', 'Northgate'], 'then districts');
    assert.ok(Math.abs(realmFit(realm, 94, 47) - 0.94 * 47 / 128) < 1e-9, 'fitted, the whole realm fills the map');
    assert.equal(realmFit(null, 100, 100), 0, 'no realm, no fit');
});

test('every building kind the server names has an icon, a path on a 16 grid with a badge colour', () => {
    for (const kind of ['inn', 'bakery', 'butcher', 'smith', 'tailor', 'apothecary', 'church', 'watch', 'bank', 'stables', 'wharf',
        'store', 'market', 'brewery', 'workshop', 'farm', 'hall', 'house']) {
        const icon = iconFor(kind);
        assert.ok(icon && /^M/.test(icon.path) && /^#[0-9a-f]{6}$/.test(icon.badge), `${kind} has an icon`);
    }
    assert.equal(iconFor(''), null, 'nothing to say, no icon');
    assert.equal(iconFor('toString'), null, 'and no icon from the object itself');
    assert.equal(Object.keys(MapIcons).length, 18, 'eighteen kinds');
});

test('the World Map asks for the realm once in the world, keeps it, zooms about the pointer and drags', () => {
    const {state: s, commands, painter} = testGame();
    s.applySnapshot({cell: {id: 'town', width: 16, height: 8, outdoors: true, rows: Array(8).fill('.'.repeat(16))},
        worldMap: [{id: 'town', name: 'Town', x: 0, y: 0, width: 16, height: 8, knowledge: 'visited', current: true}],
        self: {id: 'self', x: 4.5, y: 4.5}, entities: [{id: 'self', x: 4.5, y: 4.5}], doors: [], time: 0, cellGeneration: 1} as Json);
    s.tick(10, 0.016);
    s.tick(11, 0.016);
    assert.equal(commands.filter(c => c.type === 'realmMap').length, 1, 'asked once, not every frame');
    s.receiveEvent({type: 'realmMap', pending: true});
    s.tick(17, 0.016);
    assert.equal(commands.filter(c => c.type === 'realmMap').length, 2, 'asked again while it is still being made');
    s.receiveEvent(realmEvent);
    s.tick(30, 0.016);
    assert.equal(commands.filter(c => c.type === 'realmMap').length, 2, 'and never once it has it');
    assert.equal(s.realm?.build, 'build-73');
    s.worldMap = true;
    s.hits = [];
    painter.paint(800, 600);
    assert.ok(s.worldScale === 0 && s.worldScaleShown === s.worldFit, 'the World Map opens fitted to the whole realm');
    assert.deepEqual(s.worldPan, [256 / 2 - 4.5, 128 / 2 - 4.5], "fitted, the realm's middle is the map's middle");
    assert.ok(s.hits.some(h => h.action === 'world_zoom' && h.target === 'in'), 'with buttons to zoom');
    s.activate(s.hits.find(h => h.action === 'world_zoom' && h.target === 'in')!);
    assert.ok(Math.abs(s.worldScale - s.worldFit * 1.6) < 1e-9, '+ zooms in');
    s.mapRect = {left: 0, top: 0, right: 800, bottom: 600} as typeof s.mapRect;
    const before = s.worldScale, pan = [...s.worldPan];
    s.wheel([600, 300], 1, false, false);
    assert.ok(Math.abs(s.worldScale - before * 1.25) < 1e-9, 'the wheel zooms');
    // The point under the pointer (200 px right of the middle) stays under it.
    assert.ok(Math.abs((pan[0] + 200 / before) - (s.worldPan[0] + 200 / s.worldScale)) < 1e-9, 'about the pointer');
    s.worldScaleShown = s.worldScale;
    s.hits = [];
    const p0 = [...s.worldPan];
    s.mouseDown([300, 300], true, false, false);
    s.mouseMove([340, 280], false);
    s.mouseUp();
    assert.ok(Math.abs(s.worldPan[0] - (p0[0] - 40 / s.worldScale)) < 1e-9 && Math.abs(s.worldPan[1] - (p0[1] + 20 / s.worldScale)) < 1e-9,
        'dragging pans the map with the pointer');
    s.mouseMove([500, 500], false);
    assert.ok(Math.abs(s.worldPan[0] - (p0[0] - 40 / s.worldScale)) < 1e-9, 'and only while the button is down');
    s.activate({rect: {left: 0, top: 0, right: 0, bottom: 0} as typeof s.mapRect, action: 'world_zoom', target: 'fit'});
    assert.equal(s.worldScale, 0, 'the whole realm again');
    assert.ok(IconScale >= 1, '(icons come only once buildings are big enough to carry one)');
});

test('a left click on a door walks into it; its menu is a right click', () => {
    const {state: s, commands, painter} = testGame();
    s.applySnapshot({cell: {id: 'street', width: 16, height: 8, outdoors: true, rows: ['................', '.....####.......', '.....##+#.......',
        '................', '................', '................', '................', '................']},
    self: {id: 'self', x: 7.5, y: 5.5}, entities: [{id: 'self', x: 7.5, y: 5.5}],
    doors: [{id: 'door_shop', name: 'Shop door', x: 7.5, y: 2.5, open: false, actions: ['open', 'knock']}], time: 0, cellGeneration: 1} as Json);
    draw(painter, 'drawLocal');
    const door = s.hits.find(h => h.action === 'target' && h.target === 'door_shop');
    assert.ok(door, 'the door is a click target');
    const at: [number, number] = [(door.rect.left + door.rect.right) / 2, (door.rect.top + door.rect.bottom) / 2];
    s.mouseDown(at, true, false, false);
    const path = commands.find(c => c.type === 'path');
    assert.deepEqual(path && [path.x, path.y], [7.5, 2.5], 'a left click heads into the doorway (the server opens it if it may)');
    assert.equal(s.contextTarget, '', 'with no menu in the way');
    s.mouseDown(at, false, false, false);
    assert.equal(s.contextTarget, 'door_shop', "a right click opens the door's menu, as before");
    assert.deepEqual(s.contextActions, ['open', 'knock']);
});

test('stairs are drawn as the ground, not lit like a door', () => {
    assert.ok(isStairs('^', 'Rope End stairs') && isStairs('.', 'Loft steps') && isStairs('^', 'a way up'), 'a stairs tile or name');
    assert.ok(!isStairs('+', 'Shop door') && !isStairs('.', 'Upstairs Landing door'), 'a door is a door');
});

test('speech bubbles: what was said, trailing off; how long; stacked; three dots in a wave', () => {
    assert.equal(bubbleText('  Good   morning,\nneighbour. '), 'Good morning, neighbour.', 'one line of words');
    const long = 'The river rose in the night and took the old mill wheel with it, and the miller says he saw something swimming upstream';
    const said = bubbleText(long);
    assert.ok(said.length <= BubbleChars && said.endsWith('…'), `at most ${BubbleChars} characters, trailing off: ${said}`);
    assert.ok(long.startsWith(said.slice(0, -1)) && !said.slice(0, -1).endsWith(' '), 'cut at a word, its start kept');
    assert.equal(bubbleText('x'.repeat(100)).length, BubbleChars, 'a word longer than the bubble is cut too');
    assert.equal(bubbleText('Short.'), 'Short.', 'a short line whole');
    assert.ok(bubbleSeconds('Hi.') >= 3.5 && bubbleSeconds(said) > bubbleSeconds('Hi.') && bubbleSeconds(long.repeat(5)) === 9,
        'longer lines stay longer, to a limit');
    assert.equal(spokenWords([{kind: 'action', text: 'nods'}, {kind: 'speech', text: 'Aye.'}, {kind: 'speech', text: 'Come in.'}], '*nods* "Aye."'),
        'Aye. … Come in.', 'only the spoken parts');
    assert.equal(spokenWords([{kind: 'action', text: 'nods'}], 'nods'), '', 'an action alone has no bubble words');
    const placed = stackBubbles([{x: 0, y: 100, w: 100, h: 30}, {x: 50, y: 105, w: 100, h: 30}, {x: 400, y: 100, w: 50, h: 30}]);
    assert.equal(placed[0].y, 100, 'the first where it wants to be');
    assert.ok(placed[1].y + placed[1].h <= placed[0].y, 'one that would cover it goes above it');
    assert.equal(placed[2].y, 100, 'one clear of the rest stays');
    const lifts = [0, 1, 2].map(d => [0, 0.1, 0.2, 0.3, 0.4].map(t => dotLift(t, d)));
    assert.ok(lifts.every(series => series.some(v => v > 0)) && lifts[0][1] !== lifts[2][1], 'three dots, each bobbing, in turn');
});

test("a heard line's bubble goes over the wolf it came from, in its colour; a voice with no wolf gets none", () => {
    const {state: s} = testGame();
    s.applySnapshot({cell: {id: 'street', width: 16, height: 8, outdoors: true, rows: Array(8).fill('.'.repeat(16))},
        self: {id: 'self', x: 2.5, y: 2.5}, entities: [{id: 'self', x: 2.5, y: 2.5}, {id: 'wren', name: 'Wren', kind: 'npc', x: 5.5, y: 2.5, color: 4}],
        doors: [], time: 0, cellGeneration: 1} as Json);
    s.clock = 50;
    s.receiveEvent({type: 'roleplay', channel: 'ic', sourceId: 'wren', speaker: 'Wren', color: 6, text: 'waves "Rope, tallow, herbs!"',
        segments: [{kind: 'action', text: 'waves'}, {kind: 'speech', text: 'Rope, tallow, herbs!'}]});
    const wren = s.entities.get('wren')!;
    assert.equal(wren.said, 'Rope, tallow, herbs!', 'her words, without the action');
    assert.equal(wren.saidColor, 6, 'in her chosen colour');
    assert.ok((wren.saidUntil ?? 0) > 53, 'for a few seconds');
    s.receiveEvent({type: 'roleplay', channel: 'ic', anonymous: true, speaker: 'A voice', text: '...', segments: [{kind: 'speech', text: '...'}]});
    assert.equal(wren.said, 'Rope, tallow, herbs!', 'a voice too far off (no wolf named) puts words over nobody');
});
