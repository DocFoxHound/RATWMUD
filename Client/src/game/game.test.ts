// The game screen's rules, as the Unreal client's engine tests checked them (UI/RatwUITests.cpp).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {draw, paintPart, testGame} from './testing.ts';
import {lookAt} from './look.ts';
import {MapRenderer} from './minimap.ts';
import {paceLabel, scentLabel, windLabel, environmentLabel, environmentEffectsLabel, calendarLabel, dayLabel, moonLabel, elevationLabel, lawLabel} from './labels.ts';
import {alphaOf, Size, type Art} from './weatherArt.ts';
import {MotionBuffer} from './motionBuffer.ts';
import {rect, contains} from '../ui/painter.ts';
import {terrainInfo} from './paint.ts';
import type {Json} from './json.ts';

const close = (a: number, b: number, e = 1e-6) => Math.abs(a - b) < e;

test('input: Enter writes, Shift+Enter keeps writing, Escape keeps the draft, a rejected post comes back, WASD only in navigation', () => {
    const {state: s, commands, composer} = testGame();
    s.keyDown({code: 'Enter'});
    assert.ok(s.chat, 'Enter begins composing');
    assert.ok(composer.focused);
    composer.text = 'The first line';
    assert.ok(s.composerKey({code: 'Enter', shift: true}));
    assert.ok(composer.text.includes('\n') && s.chat, 'Shift+Enter inserts a real newline and keeps writing');
    const preserved = composer.text;
    s.composerKey({code: 'Escape'});
    assert.ok(!s.chat, 'Escape returns to navigation');
    assert.equal(composer.text, preserved, 'and keeps the exact draft');
    s.keyDown({code: 'Enter'});
    composer.text = 'A retained post.';
    s.composerKey({code: 'Enter'});
    assert.ok(!s.chat, 'sending returns to movement at once');
    assert.equal(s.pendingDrafts.size, 1, 'the text is kept until the server accepts it');
    assert.ok(commands.some(c => c.type === 'chat' && c.text === 'A retained post.'));
    s.receiveEvent({type: 'error', context: 'chat', requestId: 'post_1', text: 'Test rejection'});
    assert.equal(composer.text, 'A retained post.', 'a rejected post restores its draft');
    s.keyDown({code: 'KeyW'});
    assert.ok(s.heldKeys.has('KeyW'), 'W moves in navigation');
    s.keyUp({code: 'KeyW'});
    assert.equal(s.heldKeys.size, 0, 'release stops');
    s.keyDown({code: 'Enter'});
    assert.equal(s.keyDown({code: 'KeyW'}), false, 'while writing, W is text');
    assert.equal(s.heldKeys.size, 0);
});

test('input: movement is sent while held, and letting go of focus lets go of the keys', () => {
    const {state: s, commands} = testGame();
    // Moves are numbered (seq) for held movement's acknowledgements (doc 31, Phase 3); the rest is compared here.
    const last = () => { const {seq: _seq, ...rest} = commands.at(-1) as Record<string, unknown>; return rest; };
    s.keyDown({code: 'KeyD'});
    s.keyDown({code: 'KeyW'});
    assert.deepEqual(last(), {type: 'move', x: 1, y: -1}, 'diagonals combine');
    const before = commands.length;
    s.tick(1, 0.05);
    s.tick(1.1, 0.1);
    assert.ok(commands.length > before, 'held keys are resent');
    s.focusLost();
    assert.equal(s.heldKeys.size, 0);
    assert.deepEqual(last(), {type: 'move', x: 0, y: 0}, 'losing focus stops the wolf');
    s.focusGained();
    s.keyDown({code: 'KeyA'});
    assert.deepEqual(last(), {type: 'move', x: -1, y: 0});
    s.snapshot = {travel: {active: true}};
    s.keyUp({code: 'KeyA'});
    assert.deepEqual(last(), {type: 'move', x: -1, y: 0}, 'letting go never resets an overland route');
});

test('reveal: posts unfold one at a time; speech keeps its quotation marks', () => {
    const {state: s} = testGame();
    for (const id of ['0', '1']) s.receiveEvent({type: 'roleplay', id, speaker: 'Visible speaker', text: 'Five.', channel: 'ic'});
    s.tick(1, 0.04);
    assert.ok(s.posts[0].revealed > 0, 'the first starts revealing');
    assert.equal(s.posts[1].revealed, 0, 'the second waits');
    s.tick(1.1, 0.1);
    assert.equal(s.posts[0].revealed, 5);
    assert.equal(s.posts[1].revealed, 0, 'never concurrently');
    s.tick(1.2, 0.1);
    assert.equal(s.posts[1].revealed, 5, 'the second begins after the first');
    s.receiveEvent({type: 'roleplay', speaker: 'A voice', text: 'flattened fallback',
        segments: [{kind: 'speech', text: 'The road is quiet.'}, {kind: 'action', text: 'sighs softly.'}]});
    assert.equal(s.posts.at(-1)!.text, '"The road is quiet." sighs softly.');
});

test('the story column cycles through four readable widths without touching the server or the draft', () => {
    const {state: s, commands, composer} = testGame();
    composer.text = 'A draft that survives layout changes.\nAnother paragraph.';
    for (const value of [600, 760, 360, 460]) {
        s.activate({rect: rect(0, 0, 0, 0), action: 'split', target: ''});
        assert.equal(s.storyWidth, value);
        assert.equal(composer.text, 'A draft that survives layout changes.\nAnother paragraph.');
    }
    assert.equal(commands.length, 0);
});

function facingSetup() {
    const g = testGame();
    const self: Json = {id: 'player', x: 5, y: 5, facing: Math.PI / 2, moving: false};
    const cell: Json = {id: 'test_cell', width: 32, height: 24};
    const snapshot: Json = {self, cell, entities: [{id: 'player', x: 5, y: 5, facing: Math.PI / 2, moving: false}]};
    g.state.applySnapshot(snapshot);
    g.state.mapOrigin = [600, 200];
    g.state.mapRect = rect(584, 199, 1544, 816);
    g.state.tileSize = 20;
    return {...g, self, cell, snapshot};
}

test('facing: Alt previews, Alt+click turns in place, and nothing else can', () => {
    const {state: s, commands, self, cell, snapshot} = facingSetup();
    const east: [number, number] = [800, 300], north: [number, number] = [700, 250];
    s.mouseMove(east, true);
    assert.ok(s.facingPreview, 'Alt previews');
    assert.ok(close(s.previewFacing, 0), 'east is zero radians');
    assert.equal(commands.length, 0, 'a preview sends nothing');
    assert.ok(close(s.entities.get('player')!.facing, Math.PI / 2), 'nor turns the marker');
    s.mouseMove(north, true);
    assert.ok(close(s.previewFacing, -Math.PI / 2), 'screen north is -π/2');
    s.hits.push({rect: rect(790, 290, 810, 310), action: 'target', target: 'door_or_wolf'});
    s.mouseMove(east, true);
    s.mouseDown(east, true, true, false);
    assert.equal(commands.length, 1, 'Alt+click sends one command');
    assert.equal(s.contextTarget, '', 'ahead of a wolf or door under the pointer');
    assert.deepEqual(commands[0], {type: 'face', x: 10, y: 5}, 'face, in tiles');
    assert.ok(close(s.entities.get('player')!.facing, Math.PI / 2), 'waiting for the server, not snapping');
    self.facing = 0.1;
    snapshot.time = 0.2;
    s.applySnapshot(snapshot);
    assert.ok(close(s.entities.get('player')!.facing, Math.PI / 2), 'a new facing is a target, not a snap');
    s.tick(1, 0.025);
    const f = s.entities.get('player')!.facing;
    assert.ok(f > 0.1 && f < Math.PI / 2, 'drawing turns toward it');
    const view = s.entities.get('player')!;
    view.motion.samples = [];
    view.motion.add(0, 5, 5, 3.1);
    view.motion.add(0.2, 5, 5, -3.1);
    s.motionClock = 0.1;
    s.motionOffset = 0;
    s.tick(1.1, 0.025);
    assert.ok(s.entities.get('player')!.facing > 3.1, 'the short way round across π');
    s.keyUp({code: 'AltLeft'});
    assert.ok(!s.facingPreview, 'letting go of Alt clears the preview');
    s.keyDown({code: 'AltLeft'});
    assert.ok(s.facingPreview, 'pressing Alt shows it at the pointer');
    s.keyDown({code: 'KeyW'});
    assert.ok(!s.facingPreview, 'moving hides it');
    s.mouseMove(east, true);
    assert.ok(!s.facingPreview, 'and held movement cannot bring it back');
    s.keyUp({code: 'KeyW'});
    self.moving = true;
    s.applySnapshot(snapshot);
    let before = commands.length;
    s.mouseMove(east, true);
    s.mouseDown(east, true, true, false);
    assert.ok(!s.facingPreview && commands.length === before && s.contextTarget === '', 'not while the wolf moves');
    self.moving = false;
    self.postureRemaining = 0.5;
    s.applySnapshot(snapshot);
    s.mouseMove(east, true);
    assert.ok(!s.facingPreview, 'nor while rising');
    self.postureRemaining = 0;
    s.applySnapshot(snapshot);
    s.chat = true;
    s.mouseMove(east, true);
    s.mouseDown(east, true, true, false);
    assert.ok(!s.facingPreview && s.chat && commands.length === before, 'nor while writing');
    s.chat = false;
    s.worldMap = true;
    s.mouseMove(east, true);
    s.mouseDown(east, true, true, false);
    assert.ok(!s.facingPreview && commands.length === before, 'nor on the world map');
    s.worldMap = false;
    s.modal = 'character';
    s.mouseMove(east, true);
    s.mouseDown(east, true, true, false);
    assert.ok(!s.facingPreview && commands.length === before, 'nor behind a sheet');
    s.modal = '';
    s.mouseMove(east, true);
    s.focusLost();
    assert.ok(!s.facingPreview, 'losing focus clears it');
    before = commands.length;
    s.mouseMove(east, true);
    s.mouseDown(east, true, true, false);
    assert.ok(!s.facingPreview, 'without focus the pointer cannot bring the preview back');
    assert.equal(commands.length, before, 'an unfocused Alt+click sends nothing');
    s.focusGained();
    s.mouseMove(east, true);
    s.mouseLeave();
    assert.ok(!s.facingPreview, 'leaving the page hides it');
    s.mouseMove(east, true);
    cell.id = 'next_cell';
    s.applySnapshot(snapshot);
    assert.ok(!s.facingPreview, 'a new cell clears it');
    before = commands.length;
    s.mouseDown(east, true, false, true);
    assert.equal(commands.at(-1)!.type, 'face', 'Ctrl+click turns too');
    s.hits = [];
    s.mouseDown(east, true, false, false);
    assert.equal(commands.at(-1)!.type, 'path', 'a plain click walks there');
    s.mouseMove(north, true);
    assert.ok(!s.facingPreview, 'a pending path blocks the preview until the next snapshot');
});

test('scent: anonymous compass hints, merged and bounded; wind wording; development controls only for developers', () => {
    const {state: s, commands, painter} = testGame();
    const wind: Json = {direction: 0, strength: 0.5, variable: true};
    const cell: Json = {id: 'scent_test', outdoors: true, wind};
    const senses: Json = {};
    const snapshot: Json = {cell, self: {id: 'self', name: 'Self', x: 5, y: 5}, senses};
    const cue = (sector: unknown, strength: unknown, windborne: boolean) =>
        ({sector, strength, windborne, name: 'Unseen identity', id: 'hidden_wolf', x: 20, y: 18});
    senses.scentCues = [cue(4, 1, true), cue(4, 3, true), cue(-1, 2, true), cue(8, 2, true), cue(2.5, 2, true), cue(2, 4, true),
        cue(2, 0, true), cue(2, 1.5, true), 'bad'];
    senses.movementHeard = true;
    s.applySnapshot(snapshot);
    assert.equal(s.scentCues.length, 1, 'repeats merge; invalid sectors and strengths are refused');
    assert.deepEqual(s.scentCues[0], {sector: 4, strength: 3, windborne: true});
    assert.ok(s.movementHeard);
    assert.equal(s.entities.size, 1, 'scents never become wolves on the map');
    assert.equal(scentLabel(s.scentCues), 'SCENT · unseen wolf roughly W · upwind');
    assert.ok(windLabel(s.outdoors, s.windStrength, s.windDirection, s.windVariable).includes('W -> E'), 'air flowing east is W -> E');
    s.mapRect = rect(584, 199, 1544, 816);
    s.hits = [];
    draw(painter, 'drawScent', 1000, 500);
    draw(painter, 'drawScent', 586, 500);
    assert.equal(s.hits.length, 0, 'scent arcs are never click targets');
    assert.equal(commands.length, 0);
    senses.scentCues = Array.from({length: 64}, (_, i) => cue(i % 8, (i % 3) + 1, true));
    s.applySnapshot(snapshot);
    assert.deepEqual(s.scentCues.map(c => c.sector), [0, 1, 2, 3, 4, 5, 6, 7], 'at most eight, in compass order');
    cell.outdoors = false;
    s.applySnapshot(snapshot);
    assert.equal(s.windStrength, 0, 'indoors the air is still');
    assert.equal(windLabel(s.outdoors, s.windStrength, s.windDirection, s.windVariable), 'SHELTERED · still air');
    assert.ok(!scentLabel(s.scentCues).includes('upwind'));
    s.activate({rect: rect(0, 0, 0, 0), action: 'wind', target: 'east'});
    assert.equal(commands.length, 0, 'players cannot change the wind');
    snapshot.devTools = true;
    s.applySnapshot(snapshot);
    s.activate({rect: rect(0, 0, 0, 0), action: 'wind', target: 'east'});
    assert.deepEqual(commands, [{type: 'wind', value: 'east'}]);
    cell.outdoors = true;
    wind.strength = 0;
    s.applySnapshot(snapshot);
    assert.equal(windLabel(s.outdoors, s.windStrength, s.windDirection, s.windVariable), 'AIR FLOW · calm');
    delete snapshot.senses;
    s.applySnapshot(snapshot);
    assert.ok(!s.scentCues.length && !s.movementHeard);
    assert.equal(scentLabel(s.scentCues), 'SCENT · no unseen scent detected');
});

test('pace and known travel', () => {
    assert.deepEqual([0, 1, 2, 5, 6, 8, 9, 10].map(paceLabel), ['WALK', 'TROT', 'TROT', 'TROT', 'RUN', 'RUN', 'SPRINT', 'SPRINT']);
    const {state: s, commands, composer, painter} = testGame();
    const self: Json = {id: 'wolf', pace: 0, stamina: 42, staminaRate: -10};
    const snapshot: Json = {self, cell: {id: 'home'}};
    s.applySnapshot(snapshot);
    s.mapRect = rect(584, 199, 1544, 816);
    s.keyDown({code: 'PageUp'});
    s.keyDown({code: 'PageUp'});
    assert.equal(s.displayPace(), 2, 'rapid keys add up against the pending request');
    s.applySnapshot(snapshot);
    assert.equal(s.displayPace(), 2, 'a lagging snapshot cannot erase it');
    s.wheel([900, 450], 1, false, false);
    assert.equal(s.displayPace(), 3, 'the wheel over the map raises pace');
    for (let i = 0; i < 20; ++i) s.keyDown({code: 'PageUp'});
    assert.equal(s.displayPace(), 10);
    assert.equal(commands.length, 10, 'no redundant commands at the ceiling');
    assert.deepEqual(commands.at(-1), {type: 'pace', pace: 10});
    assert.equal(self.stamina, 42);
    let before = commands.length;
    s.wheel([900, 450], 1, true, false);
    s.wheel([900, 450], 1, false, true);
    assert.deepEqual(s.mapPan, [60, 60], 'Shift and Ctrl pan');
    s.wheel([1500, 950], -1, false, false);
    assert.equal(commands.length, before, 'the wheel elsewhere does nothing');
    self.pace = 10;
    s.applySnapshot(snapshot);
    assert.equal(s.requestedPace, -1, 'a matching server pace clears the request');
    s.chat = true;
    composer.text = 'A long roleplay draft.\nAnother paragraph.';
    before = commands.length;
    s.keyDown({code: 'PageDown'});
    s.wheel([900, 450], -1, false, false);
    s.activate({rect: rect(0, 0, 0, 0), action: 'pace', target: '0'});
    assert.equal(commands.length, before, 'no pace changes while writing');
    assert.ok(composer.text.includes('\n'));
    s.chat = false;
    s.modal = 'settings';
    s.keyDown({code: 'PageDown'});
    s.wheel([900, 450], -1, false, false);
    s.activate({rect: rect(0, 0, 0, 0), action: 'pace', target: '0'});
    assert.equal(commands.length, before, 'nor behind a sheet');
    s.modal = '';
    s.keyDown({code: 'PageDown'});
    s.clock += 2;
    assert.equal(s.displayPace(), 10, 'an unanswered request falls back to the server');
    const visit = (id: string, knowledge: string, x: number) => ({id, name: id, knowledge, x, y: 0, z: 0, width: 32, height: 24});
    snapshot.travelMap = [visit('home', 'visited', 0), visit('field', 'visited', 32), visit('outline', 'seen', 64), visit('secret', 'unknown', 96), 'malformed'];
    snapshot.worldMap = [visit('live_neighbor', 'visited', 0)];
    snapshot.travel = {active: true, destination: 'field'};
    s.applySnapshot(snapshot);
    s.setPresentationPage('travel');
    assert.ok(s.worldMap && s.travelAtlas);
    assert.ok(s.canTravelTo('field'));
    assert.ok(!s.canTravelTo('outline') && !s.canTravelTo('secret') && !s.canTravelTo('live_neighbor') && !s.canTravelTo('home'));
    s.hits = [];
    draw(painter, 'drawTravelAtlas');
    const travelHits = s.hits.filter(h => h.action === 'travel');
    assert.ok(travelHits.length === 2 && travelHits.every(h => h.target === 'field'), 'only visited places, on the map and in the list');
    (snapshot.travelMap as unknown[]).push(visit('shelter', 'visited', 0));
    s.applySnapshot(snapshot);
    s.hits = [];
    draw(painter, 'drawTravelAtlas');
    assert.equal(s.hits.filter(h => h.action === 'travel' && h.target === 'shelter').length, 2, 'an interior sharing an origin keeps its targets');
    assert.equal(s.hits.filter(h => h.action === 'travel' && h.target === 'field').length, 2);
    before = commands.length;
    s.activate({rect: rect(0, 0, 0, 0), action: 'travel', target: 'secret'});
    assert.equal(commands.length, before, 'a forged target is ignored');
    s.activate({rect: rect(0, 0, 0, 0), action: 'travel', target: 'field'});
    assert.deepEqual(commands.at(-1), {type: 'travel', target: 'field'});
    before = commands.length;
    s.wheel([900, 450], -1, false, false);
    assert.equal(commands.length, before, 'the wheel pages the atlas, never pace');
    s.keyDown({code: 'Escape'});
    assert.equal(commands.at(-1)!.type, 'cancel_travel');
    before = commands.length;
    s.chat = true;
    s.activate({rect: rect(0, 0, 0, 0), action: 'travel', target: 'field'});
    s.activate({rect: rect(0, 0, 0, 0), action: 'cancel_travel', target: ''});
    assert.equal(commands.length, before, 'no routes behind the composer');
    s.keyDown({code: 'Escape'});
    assert.ok(commands.slice(before).every(c => c.type !== 'cancel_travel' && c.type !== 'move'), 'Escape while writing leaves the journey');
    before = commands.length;
    s.chat = false;
    s.setPresentationPage('world');
    assert.ok(s.worldMap && !s.travelAtlas);
    s.activate({rect: rect(0, 0, 0, 0), action: 'travel', target: 'field'});
    assert.equal(commands.length, before, 'old atlas targets do nothing on the neighbourhood map');
    Object.assign(self, {pace: 9999, stamina: -1e9, staminaRate: 1e9});
    s.clock += 2;
    s.applySnapshot(snapshot);
    assert.equal(s.displayPace(), 10, 'malformed pace is bounded');
    assert.equal(commands.length, before);
});

test('weather and light: presentation only, bounded, and never over the world map', () => {
    const {state: s, commands, painter} = testGame();
    const env: Json = {hour: 18.25, phase: 'dusk', daylight: 0.35, illumination: 0.28, sight: 0.48, hearing: 0.62, scent: 0.5, movement: 0.85,
        id: 'hidden_wolf', name: 'Hidden identity', x: 12};
    const wind: Json = {direction: 0, strength: 0.7};
    const cell: Json = {id: 'weather_test', outdoors: true, weather: 'rain', tiles: ['.'.repeat(32)], environment: env, wind};
    const snapshot: Json = {cell, self: {id: 'self', x: 6, y: 5}, visibility: ['1'.repeat(32)]};
    s.applySnapshot(snapshot);
    s.mapRect = rect(584, 199, 1544, 816);
    s.mapOrigin = [690, 230];
    s.tileSize = 23;
    const e = s.environment;
    assert.equal(e.phase, 'dusk');
    assert.equal(environmentLabel(e.hour, e.phase, e.weather, s.outdoors), '18:15 DUSK · RAIN');
    const effects = environmentEffectsLabel(e, s.outdoors, s.reducedMotion);
    assert.ok(effects.includes('HEARING 62%') && effects.includes('FOOTING 85%'));
    type Layer = {art: Art; scroll: [number, number]; angle: number; tint: {a: number}};
    const layers = () => paintPart<Layer[]>(painter, 'weatherLayers');
    const count = (list: Layer[], art: Art) => list.filter(l => l.art === art).length;
    const rain = layers();
    assert.ok(count(rain, 'rain') >= 2, 'rain in two depths at least');
    assert.ok(rain[0].angle < 0, 'slanted by an eastward wind');
    assert.ok(rain.every(l => l.tint.a > 0 && l.tint.a < 0.7), 'translucent');
    const splashes = paintPart<Array<{x: number; y: number}>>(painter, 'weatherMarks');
    const bounds = paintPart<ReturnType<typeof rect>>(painter, 'cellBounds');
    assert.ok(splashes.length && splashes.every(m => contains(s.mapRect, m.x, m.y) && contains(bounds, m.x, m.y)), 'splashes stay in the cell');
    s.clock = 3;
    assert.notDeepEqual(layers()[0].scroll, rain[0].scroll, 'rain moves with time');
    wind.direction = Math.PI;
    s.applySnapshot(snapshot);
    assert.ok(layers()[0].angle > 0, 'a reversed wind reverses the slant');
    s.reducedMotion = true;
    const still = layers();
    s.clock = 500;
    assert.deepEqual(layers().map(l => [l.scroll, l.angle]), still.map(l => [l.scroll, l.angle]), 'reduced motion is static');
    assert.ok(environmentEffectsLabel(e, s.outdoors, s.reducedMotion).includes('STATIC WEATHER'));
    cell.weather = 'storm';
    s.applySnapshot(snapshot);
    assert.ok(count(layers(), 'rain') > count(still, 'rain'), 'a storm is heavier than rain');
    assert.equal(paintPart<number>(painter, 'lightningFlash'), 0, 'no lightning in reduced motion');
    s.reducedMotion = false;
    let brightest = 0;
    for (let t = 0; t < 30; t += 0.02) {
        s.clock = t;
        brightest = Math.max(brightest, paintPart<number>(painter, 'lightningFlash'));
    }
    assert.ok(brightest > 0.05 && brightest <= 0.2, 'lightning flashes, dimly');
    s.reducedMotion = true;
    cell.weather = 'snow';
    s.applySnapshot(snapshot);
    const snow = layers();
    assert.equal(count(snow, 'snow'), 3);
    assert.ok(!paintPart<unknown[]>(painter, 'weatherMarks').length && !count(snow, 'rain'));
    cell.weather = 'fog';
    s.applySnapshot(snapshot);
    const fog = layers();
    assert.ok(count(fog, 'mist') >= 2 && !paintPart<unknown[]>(painter, 'weatherMarks').length);
    s.clock += 90;
    assert.deepEqual(layers()[0].scroll, fog[0].scroll, 'still fog in reduced motion');
    s.reducedMotion = false;
    assert.notDeepEqual(layers()[0].scroll, fog[0].scroll, 'drifting fog otherwise');
    cell.weather = 'sandstorm';
    s.applySnapshot(snapshot);
    assert.equal(e.weather, 'sandstorm');
    assert.ok(count(layers(), 'dust') >= 2 && layers().at(-1)!.angle === s.windDirection, 'sand blows along the wind');
    cell.weather = 'clear';
    s.applySnapshot(snapshot);
    assert.equal(count(layers(), 'cloud'), 1, 'the dusk sun casts cloud shadows');
    env.daylight = 0;
    s.applySnapshot(snapshot);
    assert.equal(layers().length, 0, 'which vanish with it');
    env.daylight = 0.35;
    for (const art of ['mist', 'cloud', 'rain', 'snow', 'dust', 'pool'] as Art[]) {
        const alpha = alphaOf(art);
        const painted = alpha.filter(a => a > 8).length;
        assert.ok(alpha.length === Size * Size && painted > 0 && painted < Size * Size, `${art} is partly painted`);
    }
    const mist = alphaOf('mist');
    let seam = 0, inside = 0;
    for (let y = 0; y < Size; ++y) {
        seam = Math.max(seam, Math.abs(mist[y * Size] - mist[y * Size + Size - 1]));
        for (let x = 1; x < Size; ++x) inside = Math.max(inside, Math.abs(mist[y * Size + x] - mist[y * Size + x - 1]));
    }
    assert.ok(seam <= inside, 'mist wraps without a seam');
    cell.weather = 'fog';
    s.applySnapshot(snapshot);
    s.hits = [];
    draw(painter, 'drawLocal');
    assert.ok(!s.hits.some(h => h.target === 'hidden_wolf'), 'weather metadata never targets a hidden wolf');
    assert.equal(s.entities.size, 1);
    assert.equal(s.visibilityRows[0], '1'.repeat(32));
    assert.equal(commands.length, 0);
    s.setPresentationPage('world');
    assert.equal(layers().length, 0, 'no local fog over the world map');
    cell.weather = 'rain';
    s.applySnapshot(snapshot);
    assert.ok(!paintPart<unknown[]>(painter, 'weatherMarks').length && !layers().length);
    s.setPresentationPage('balanced');
    for (const phase of ['dawn', 'day', 'dusk', 'night']) {
        env.phase = phase;
        s.applySnapshot(snapshot);
        assert.equal(e.phase, phase);
        s.activate({rect: rect(0, 0, 0, 0), action: 'time', target: phase});
    }
    assert.equal(commands.length, 0, 'players cannot change the clock');
    snapshot.devTools = true;
    s.applySnapshot(snapshot);
    s.activate({rect: rect(0, 0, 0, 0), action: 'time', target: 'night'});
    assert.deepEqual(commands, [{type: 'time', value: 'night'}]);
    snapshot.devTools = false;
    s.applySnapshot(snapshot);
    commands.length = 0;
    s.activate({rect: rect(0, 0, 0, 0), action: 'time', target: 'day'});
    assert.equal(commands.length, 0, 'and without development tools, the clock is not offered');
    cell.outdoors = false;
    env.illumination = 0.08;
    s.applySnapshot(snapshot);
    assert.equal(e.illumination, 0.08);
    assert.ok(!paintPart<unknown[]>(painter, 'weatherMarks').length && !layers().length, 'rain stays outside');
    assert.ok(environmentLabel(e.hour, e.phase, e.weather, s.outdoors).endsWith('SHELTERED'));
    env.phase = 'day';
    env.hour = 12.5;
    s.applySnapshot(snapshot);
    assert.equal(environmentLabel(e.hour, e.phase, e.weather, s.outdoors), '12:30 DAY · SHELTERED');
    Object.assign(cell, {outdoors: true, weather: 'unsupported condition'});
    Object.assign(env, {phase: 'unsupported phase', hour: Infinity, illumination: NaN, daylight: false, hearing: '0.1', sight: 1e20, scent: -1e20});
    s.applySnapshot(snapshot);
    assert.deepEqual([e.hour, e.phase, e.weather, e.illumination, e.daylight, e.hearing, e.sight, e.scent], [12, 'day', 'clear', 1, 1, 1, 4, 0]);
    env.hour = 24;
    s.applySnapshot(snapshot);
    assert.equal(e.hour, 0, 'hour 24 is the next midnight');
    delete cell.environment;
    s.applySnapshot(snapshot);
    assert.equal(e.hour, 12);
    assert.equal(e.sight, 1);
});

test('elevation: heights with the tiles, bounded, read relative to the wolf', () => {
    const {state: s, painter} = testGame();
    const tile = (x: number, y: number, glyph: string, height: unknown) => ({x, y, glyph, height, visible: true});
    const snapshot: Json = {cell: {id: 'hill', width: 6, height: 5, outdoors: true,
        tiles: [tile(1, 1, '.', 1.5), tile(2, 1, '%', 3), tile(3, 1, ':', -0.5), tile(1, 2, '.', true), tile(2, 2, '.', 99)]},
    self: {id: 'self', x: 1.5, y: 1.5}};
    s.applySnapshot(snapshot);
    assert.equal(s.heightAt(2, 1), 3);
    assert.equal(s.selfHeight(), 1.5);
    assert.equal(elevationLabel(s.selfHeight()), 'GROUND +1½');
    assert.equal(s.heightAt(1, 2), 0, 'a boolean is not a height');
    assert.equal(s.heightAt(2, 2), 16, 'heights are bounded');
    assert.equal(s.heightAt(-4, 1), s.heightAt(0, 1), 'outside, the nearest edge');
    const view = s.entities.get('self')!;
    view.x = 3.5;
    view.y = 1.5;
    assert.equal(elevationLabel(s.selfHeight()), 'GROUND −½');
    s.mapRect = rect(584, 199, 1544, 816);
    s.hits = [];
    draw(painter, 'drawLocal');
    assert.ok(!s.hits.some(h => !h.target), 'elevation adds no click targets');
    assert.ok(!s.plainGlyphs);
    s.activate({rect: rect(0, 0, 0, 0), action: 'glyphs', target: ''});
    assert.ok(s.plainGlyphs);
    draw(painter, 'drawLocal');
    const cliff = terrainInfo('%');
    assert.ok(cliff && cliff.ascii === '%' && cliff.glyph === '▒');
});

test('a large cell follows the wolf and stops at its edges', () => {
    const {state: s, painter} = testGame();
    const rows = Array.from({length: 256}, () => '_'.repeat(256));
    const seen = Array.from({length: 256}, () => '2'.repeat(256));
    const heights = Array.from({length: 256}, (_, y) => (y === 100 ? 'R' : 'P').repeat(256));
    s.applySnapshot({cell: {id: 'city', width: 256, height: 256, outdoors: true, rows, heights}, visibility: seen, self: {id: 'self', x: 130.5, y: 140.5}});
    assert.equal(s.heightAt(3, 100), 1, "'R' is one step up");
    assert.equal(s.heightAt(3, 99), 0);
    s.mapRect = rect(584, 199, 1544, 816);
    draw(painter, 'drawLocal');
    const me = s.entities.get('self')!;
    const x = s.mapOrigin[0] + me.x * s.tileSize, y = s.mapOrigin[1] + me.y * s.tileSize;
    assert.ok(Math.abs(x - (584 + 1544) / 2) < 30 && Math.abs(y - 508) < 30, 'centred on the wolf');
    me.x = me.y = 2;
    draw(painter, 'drawLocal');
    assert.ok(s.mapOrigin[0] <= s.mapRect.left && s.mapOrigin[1] <= s.mapRect.top && s.mapOrigin[0] > s.mapRect.left - 120 &&
        s.mapOrigin[1] > s.mapRect.top - 120, 'at a corner it stops at the edge');
});

test('cell atmosphere follows the room, never the viewport; lighting presets for developers only', () => {
    const {state: s, commands, painter} = testGame();
    const env: Json = {hour: 23, phase: 'night', daylight: 0, illumination: 1, artificialLight: 1, daylightAccess: 1, glowStrength: 1,
        lightingTone: 'warm', lightSource: 'artificial'};
    const cell: Json = {id: 'small_tavern', width: 16, height: 12, outdoors: false, environment: env};
    const snapshot: Json = {cell, self: {}};
    s.applySnapshot(snapshot);
    s.mapRect = rect(584, 199, 1544, 816);
    s.mapOrigin = [840, 340];
    s.tileSize = 28;
    type Atm = {bounds: ReturnType<typeof rect>; darkness: number; glowStrength: number; weatherStrength: number; feather: number;
        glowColor: {r: number; b: number}};
    const atmosphere = () => paintPart<Atm>(painter, 'atmosphere');
    const warm = atmosphere();
    assert.equal(s.environment.illumination, 1);
    assert.equal(warm.darkness, 0);
    assert.equal(warm.glowStrength, 1);
    assert.ok(warm.glowColor.r > warm.glowColor.b, 'amber');
    assert.deepEqual(warm.bounds, rect(840, 340, 1288, 676));
    assert.ok(warm.feather > 0 && warm.feather < (warm.bounds.bottom - warm.bounds.top) / 2);
    s.mapOrigin = [840 - 170, 340 + 50];
    assert.deepEqual(atmosphere().bounds, rect(670, 390, 1118, 726), 'panning moves all four edges');
    s.cellWidth = 96;
    s.cellHeight = 64;
    s.mapOrigin = [-100, -500];
    const large = atmosphere();
    assert.ok(large.bounds.left + large.feather < s.mapRect.left && large.bounds.right - large.feather > s.mapRect.right &&
        large.bounds.top + large.feather < s.mapRect.top && large.bounds.bottom - large.feather > s.mapRect.bottom);
    const visible = paintPart<ReturnType<typeof rect>>(painter, 'visibleCellBounds');
    assert.ok(visible.left === s.mapRect.left + 1 && visible.right === s.mapRect.right - 1);
    s.reducedMotion = true;
    s.clock = 1000;
    assert.equal(atmosphere().glowStrength, large.glowStrength);
    s.worldMap = true;
    const map = atmosphere();
    assert.ok(map.glowStrength === 0 && map.darkness === 0 && map.weatherStrength === 0);
    s.worldMap = false;
    Object.assign(env, {hour: 12, phase: 'day', daylight: 1, glowStrength: 0, lightSource: 'mixed'});
    s.applySnapshot(snapshot);
    const noon = atmosphere();
    assert.ok(noon.glowStrength === 0 && noon.darkness === 0 && noon.weatherStrength === 0);
    Object.assign(env, {illumination: 0.08, artificialLight: 0, daylightAccess: 0, lightSource: 'dark'});
    s.applySnapshot(snapshot);
    assert.ok(atmosphere().darkness > 0.9 && atmosphere().glowStrength === 0);
    assert.ok(environmentEffectsLabel(s.environment, s.outdoors, false).includes('UNLIT'));
    Object.assign(env, {illumination: 1, glowStrength: 1, lightingTone: 'cool'});
    s.applySnapshot(snapshot);
    assert.ok(atmosphere().glowColor.b > atmosphere().glowColor.r, 'cool light is blue');
    cell.weather = 'rain';
    s.applySnapshot(snapshot);
    assert.ok(atmosphere().weatherStrength === 0 && !paintPart<unknown[]>(painter, 'weatherMarks').length, 'rain stays outside');
    cell.outdoors = true;
    for (const weather of ['rain', 'snow', 'fog']) {
        cell.weather = weather;
        s.applySnapshot(snapshot);
        assert.ok(atmosphere().weatherStrength > 0 && atmosphere().glowStrength === 0);
    }
    Object.assign(env, {glowStrength: Infinity, artificialLight: true, daylightAccess: '1', lightingTone: 'hidden_wolf', lightSource: 'Hidden identity'});
    s.applySnapshot(snapshot);
    const e = s.environment;
    assert.deepEqual([e.glowStrength, e.artificialLight, e.daylightAccess, e.lightingTone, e.lightSource], [0, 0, 1, 'neutral', 'daylight']);
    s.activate({rect: rect(0, 0, 0, 0), action: 'lighting', target: 'warm'});
    assert.equal(commands.length, 0);
    snapshot.devTools = true;
    s.applySnapshot(snapshot);
    s.activate({rect: rect(0, 0, 0, 0), action: 'lighting', target: 'warm'});
    assert.deepEqual(commands, [{type: 'lighting', value: 'warm'}]);
    snapshot.devTools = false;
    delete cell.environment;
    s.applySnapshot(snapshot);
    commands.length = 0;
    s.activate({rect: rect(0, 0, 0, 0), action: 'lighting', target: 'warm'});
    assert.equal(commands.length, 0);
    assert.ok(e.glowStrength === 0 && e.artificialLight === 0 && e.illumination === 1 && e.lightingTone === 'neutral');
});

test('calendar, moon and the finite economy', () => {
    const {state: s, commands, painter} = testGame();
    const calendar: Json = {year: 3, dayOfYear: 191, dayOfSeason: 7, season: 'autumn', moonName: 'full moon', moonIllumination: 1};
    const env: Json = {calendar};
    const self: Json = {id: 'self', cash: 20, age: 70, strength: 64, dexterity: 60, effectiveDexterity: 56.4, wisdom: 85, x: 16, y: 7.5};
    const item = (id: string): Json => ({id, icon: 'food', quantity: id === 'herbs' ? 3 : 1, stock: 6, owned: id === 'herbs' ? 3 : 1,
        buyPrice: id === 'herbs' ? 2 : 6, sellPrice: id === 'herbs' ? 1 : 3, canBuy: true, canSell: true});
    const herbs = item('herbs'), meal = item('meal');
    const merchant: Json = {id: 'npc_keeper', name: 'The keeper', cash: 40, items: [herbs, meal]};
    const resource: Json = {id: 'herb_patch', x: 17.5, y: 7.5, remaining: 8};
    const snapshot: Json = {cell: {id: 'clearing', outdoors: true, environment: env}, self, merchant, resource,
        inventory: [herbs, meal, 'invalid inventory row']};
    s.applySnapshot(snapshot);
    assert.equal(calendarLabel(snapshot), 'YEAR 3 · AUTUMN 7 · DAY 191 / 365');
    assert.equal(moonLabel(snapshot), 'FULL MOON · 100% LIT');
    calendar.year = true;
    calendar.moonName = 'Hidden NPC identity';
    assert.equal(calendarLabel(snapshot), 'THE SHARED WORLD');
    assert.equal(moonLabel(snapshot), 'MOON · UNKNOWN');
    calendar.year = 3;
    calendar.moonName = 'waxing crescent';
    calendar.moonIllumination = Infinity;
    assert.equal(moonLabel(snapshot), 'WAXING CRESCENT · 0% LIT');
    assert.ok(s.canTradeItem('herbs', true) && s.canTradeItem('meal', false));
    assert.ok(!s.canTradeItem('secret_item', true));
    s.contextTarget = 'npc_keeper';
    s.activate({rect: rect(0, 0, 0, 0), action: 'context', target: 'trade'});
    assert.equal(s.modal, 'trade');
    assert.ok(!commands.some(c => c.type === 'trade'), 'opening trade trades nothing');
    commands.length = 0;
    s.activate({rect: rect(0, 0, 0, 0), action: 'trade_buy', target: 'meal'});
    assert.deepEqual(commands, [{type: 'trade', target: 'npc_keeper', item: 'meal', quantity: 1, buy: true}], 'no client price');
    commands.length = 0;
    s.activate({rect: rect(0, 0, 0, 0), action: 'trade_sell', target: 'herbs'});
    assert.equal(commands[0].buy, false);
    commands.length = 0;
    self.cash = 1;
    assert.ok(!s.canTradeItem('herbs', true), 'unaffordable');
    self.cash = 20;
    merchant.cash = 0;
    assert.ok(!s.canTradeItem('meal', false), 'a broke keeper cannot buy');
    merchant.cash = 40;
    herbs.canBuy = 'true';
    assert.ok(!s.canTradeItem('herbs', true), 'string booleans authorise nothing');
    herbs.canBuy = true;
    herbs.buyPrice = 0.5;
    assert.ok(!s.canTradeItem('herbs', true), 'no fractional pennies');
    herbs.buyPrice = 2;
    herbs.stock = 0;
    assert.ok(!s.canTradeItem('herbs', true));
    herbs.stock = 6;
    herbs.canSell = false;
    assert.ok(!s.canTradeItem('herbs', false));
    herbs.canSell = true;
    assert.ok(['herbs', 'meal'].every(g => s.canTradeItem(g, true) && s.canTradeItem(g, false)), 'four offers');
    delete snapshot.merchant;
    s.applySnapshot(snapshot);
    assert.ok(['herbs', 'meal'].every(g => !s.canTradeItem(g, true) && !s.canTradeItem(g, false)), 'no stale offers');
    s.activate({rect: rect(0, 0, 0, 0), action: 'trade_buy', target: 'meal'});
    assert.equal(commands.length, 0);
    s.activate({rect: rect(0, 0, 0, 0), action: 'eat', target: ''});
    assert.deepEqual(commands, [{type: 'eat'}]);
    commands.length = 0;
    meal.quantity = 0;
    s.activate({rect: rect(0, 0, 0, 0), action: 'eat', target: ''});
    assert.equal(commands.length, 0);
    assert.ok(s.canGather());
    s.activate({rect: rect(0, 0, 0, 0), action: 'gather', target: ''});
    assert.deepEqual(commands, [{type: 'gather'}]);
    commands.length = 0;
    self.x = 15.7;
    assert.ok(!s.canGather(), 'the 1.7-tile reach');
    self.x = 16;
    resource.remaining = 0;
    assert.ok(!s.canGather());
    resource.remaining = 8;
    resource.x = NaN;
    assert.equal(s.visibleResource(), null);
    resource.x = 17.5;
    s.mapRect = rect(584, 199, 1544, 816);
    s.hits = [];
    draw(painter, 'drawLocal');
    assert.ok(s.hits.some(h => h.target === 'herb_patch'));
    delete snapshot.resource;
    s.applySnapshot(snapshot);
    s.hits = [];
    draw(painter, 'drawLocal');
    assert.ok(!s.hits.some(h => h.target === 'herb_patch'));
    s.activate({rect: rect(0, 0, 0, 0), action: 'gather', target: ''});
    assert.equal(commands.length, 0);
    for (const page of ['character', 'inventory']) {
        s.setPresentationPage(page);
        assert.equal(s.modal, page);
    }
    s.activate({rect: rect(0, 0, 0, 0), action: 'calendar', target: 'year'});
    assert.equal(commands.length, 0);
    snapshot.devTools = true;
    s.applySnapshot(snapshot);
    s.activate({rect: rect(0, 0, 0, 0), action: 'calendar', target: 'day'});
    assert.deepEqual(commands, [{type: 'calendar', value: 'day'}]);
    commands.length = 0;
    s.activate({rect: rect(0, 0, 0, 0), action: 'calendar', target: 'unsupported'});
    assert.equal(commands.length, 0);
    s.activate({rect: rect(0, 0, 0, 0), action: 'weather', target: 'seasonal'});
    assert.deepEqual(commands, [{type: 'weather', value: 'seasonal'}]);
    commands.length = 0;
    s.setPresentationPage('trade');
    assert.equal(s.modal, 'trade');
    delete env.calendar;
    assert.equal(calendarLabel(snapshot), 'THE SHARED WORLD');
    assert.equal(moonLabel(snapshot), '');
});

test('portraits: your own exact age; others only by life stage', () => {
    const {state: s, commands} = testGame();
    const own = {species: 'timber'};
    s.applySnapshot({self: {appearance: own, age: 73}});
    s.setPresentationPage('character');
    assert.equal(s.portraitAppearance(), own);
    assert.equal(s.portraitAge(), 73);
    const other = {species: 'arctic'};
    const inspect: Json = {type: 'inspect', title: 'Visible wolf', appearance: other, lifeStage: 'young', age: 98};
    s.receiveEvent(inspect);
    assert.equal(s.portraitAppearance(), other);
    assert.equal(s.portraitAge(), 6, 'the public stage, never the private age');
    delete inspect.appearance;
    s.receiveEvent(inspect);
    assert.equal(s.portraitAppearance(), null);
    s.leaveCharacter();
    assert.equal(commands.at(-1)!.type, 'character_leave');
});

test('motion buffer: interpolates between samples, never through a jump, never beyond the last', () => {
    const m = new MotionBuffer();
    assert.ok(m.add(0, 0, 0, 0) && m.add(0.1, 1, 0, 0));
    assert.ok(!m.add(0.1, 2, 0, 0), 'no repeated time');
    assert.ok(!m.add(NaN, 0, 0, 0));
    assert.ok(close(m.at(0.05).x, 0.5));
    assert.equal(m.at(5).x, 1, 'no extrapolation');
    m.add(0.2, 20, 0, 0);
    assert.equal(m.samples.length, 1, 'a jump past a wall starts again');
    for (let i = 1; i < 50; ++i) m.add(0.2 + i * 0.05, 20 + i * 0.01, 0, 0);
    assert.equal(m.samples.length, 32);
});

test('motion buffer: constant speed at any frame rate despite jitter and a lost packet', () => {
    for (const hz of [30, 60, 144]) {
        const buffer = new MotionBuffer();
        let next = 0, maxError = 0, maxSpeedError = 0, previous = 0;
        for (let frame = 0; frame <= hz * 4; ++frame) {
            const now = frame / hz;
            // Alternating 20 ms arrival jitter and one lost packet, covered by the 100 ms render buffer; samples are 20 Hz.
            while (next * 0.05 + (next % 2 ? 0.02 : 0) <= now + 1e-9) {
                if (next !== 31) buffer.add(next * 0.05, next * 0.15, 2, next * 0.025);
                ++next;
            }
            const pose = buffer.at(now - 0.1);
            if (now > 0.5) {
                maxError = Math.max(maxError, Math.abs(pose.x - (now - 0.1) * 3));
                maxSpeedError = Math.max(maxSpeedError, Math.abs((pose.x - previous) * hz - 3));
            }
            previous = pose.x;
        }
        assert.ok(maxSpeedError < 1e-8, `${hz} Hz: constant speed`);
        assert.ok(maxError < 1e-8, `${hz} Hz: accurate position`);
        assert.ok(buffer.samples.length <= 32);
    }
    const b = new MotionBuffer();
    b.add(0, 0, 0, 3.1);
    b.add(0.05, 1, 0, -3.1);
    assert.ok(b.at(0.025).facing > 3.1, 'the short arc across π');
    assert.ok(!b.add(0.02, 99, 99, 0) && !b.add(0.05, 99, 99, 0), 'reordered and repeated poses refused');
    b.add(0.1, 1, 0, -3.1);
    assert.equal(b.at(0.1).x, 1, 'a stop holds the exact position');
    assert.equal(b.at(50).x, 1, 'a drought never extrapolates through walls');
    b.add(1, 2, 0, 0);
    assert.equal(b.samples.length, 1, 'a long gap starts again');
    b.add(1.05, 20, 0, 0);
    assert.equal(b.at(1.01).x, 20, 'a teleport never slides');
});

test('motion frames: hidden wolves go at once and never come back from late metadata; a new room waits for its scene', () => {
    const {state: s} = testGame();
    const self: Json = {id: 'self', x: 5, y: 5};
    const snapshot: Json = {cell: {id: 'room'}, self, entities: [{id: 'other', x: 6, y: 5}], time: 0, cellGeneration: 1};
    s.applySnapshot(snapshot);
    const frame = (time: number, generation: number, x: number) => ({motionSession: '', observer: 'self', cellId: 'room', cellGeneration: generation,
        revision: 0, time, entities: [{id: 'self', x, y: 5, facing: 0, moving: false}]});
    s.applyMotion(frame(0.05, 1, 5.15));
    assert.ok(!s.entities.has('other'), 'a wolf out of sight goes at once');
    s.applySnapshot(snapshot);
    assert.ok(!s.entities.has('other'), 'late metadata cannot bring it back');
    assert.equal(s.entities.get('self')!.motion.samples.at(-1)!.x, 5.15, 'nor rewind a pose');
    s.applyMotion(frame(0.1, 2, 8));
    assert.equal(s.entities.get('self')!.motion.samples.at(-1)!.x, 5.15, "a new room's motion waits for its scene");
    self.x = 8;
    Object.assign(snapshot, {time: 0.1, cellGeneration: 2});
    s.applySnapshot(snapshot);
    assert.equal(s.entities.get('self')!.x, 8, 'the new room snaps at entry');
    assert.equal(s.entities.get('self')!.motion.samples.length, 1);
    let next = 3, previous = 8, maxSpeedError = 0;
    for (let f = 1; f <= 180; ++f) {
        s.tick(f / 60, 1 / 60);
        const now = 0.1 + f / 60;
        while (next * 0.05 <= now + 1e-9) {
            s.applyMotion(frame(next * 0.05, 2, 8 + (next * 0.05 - 0.1) * 3));
            ++next;
        }
        const x = s.entities.get('self')!.x;
        if (f > 40) maxSpeedError = Math.max(maxSpeedError, Math.abs((x - previous) * 60 - 3));
        previous = x;
    }
    assert.ok(maxSpeedError < 0.0001, 'no recurring acceleration at 60 frames a second');
    s.tick(4, 1);
    const resumed = s.latestMotionTime + 0.05;
    s.applyMotion(frame(resumed, 2, 17.15));
    assert.equal(s.entities.get('self')!.motion.samples.length, 1, 'resuming after a stall rebases the timeline');
    assert.ok(close(s.motionClock - s.motionOffset, resumed, 1e-8), 'with headroom to interpolate again');
});

test('input to motion: timed from a key that starts a standing wolf walking to the frame that shows it moved', () => {
    const {state: s} = testGame();
    s.applySnapshot({cell: {id: 'room'}, self: {id: 'self', x: 5, y: 5}, entities: [], time: 0, cellGeneration: 1});
    const frame = (time: number, x: number) => ({motionSession: '', observer: 'self', cellId: 'room', cellGeneration: 1,
        revision: 0, time, entities: [{id: 'self', x, y: 5, facing: 0, moving: x !== 5}]});
    s.applyMotion(frame(0.05, 5));
    s.heldKeys.add('KeyD');
    s.sendMove();
    s.applyMotion(frame(0.1, 5));
    assert.equal(s.inputToMotion.length, 0, 'not yet: the wolf has not moved');
    s.applyMotion(frame(0.15, 5.2));
    assert.equal(s.inputToMotion.length, 1, 'measured once it has');
    assert.ok(s.inputToMotion[0] >= 0);
    s.sendMove();
    s.applyMotion(frame(0.2, 5.4));
    assert.equal(s.inputToMotion.length, 1, 'still walking: nothing new to measure');
    s.heldKeys.clear();
    s.sendMove();
    s.heldKeys.add('KeyA');
    s.sendMove();
    s.applyMotion(frame(0.25, 5.3));
    assert.equal(s.inputToMotion.length, 2, 'stopping and setting off again is measured again');
});

test('the law label shows custody before a warrant, and nothing for the law-abiding', () => {
    assert.equal(lawLabel({}), '');
    assert.equal(lawLabel({wanted: {charges: 'theft', owed: 9.4}}), 'WANTED · theft · owes 9p');
    assert.equal(lawLabel({wanted: {charges: 'theft', owed: 9}, custody: {seconds: 61}}), 'HELD IN THE GAOL · 2 min');
});

test('the week and the day: the weekday in the calendar, and a market, rest day or festival beside the moon', () => {
    const snapshot = {cell: {environment: {calendar: {year: 1, dayOfYear: 6, dayOfSeason: 6, season: 'Spring', weekday: 'Marketday'}},
        day: {kind: 'market', name: '', foul: false}}};
    assert.equal(calendarLabel(snapshot), 'YEAR 1 · MARKETDAY · SPRING 6 · DAY 6 / 365');
    assert.equal(dayLabel(snapshot), 'MARKET DAY · STALLS OUT');
    snapshot.cell.day = {kind: 'festival', name: 'Harvest Home', foul: true};
    assert.equal(dayLabel(snapshot), 'HARVEST HOME · KEPT INDOORS');
    snapshot.cell.day = {kind: 'work', name: '', foul: false};
    assert.equal(dayLabel(snapshot), '');
    assert.equal(dayLabel({}), '');
});

test('the map ground is drawn offscreen once and redrawn only when what it shows changes', () => {
    const {state: s, painter, surfaces} = testGame(true);
    const rows = Array.from({length: 24}, () => '.'.repeat(32));
    const snapshot: Json = {cell: {id: 'glade', width: 32, height: 24, rows, heights: []}, visibility: rows.map(() => '2'.repeat(32)),
        self: {id: 'me', x: 16, y: 12}, time: 0, cellGeneration: 1};
    s.applySnapshot(snapshot);
    s.tick(0.016, 0.016);
    draw(painter, 'drawLocal');
    draw(painter, 'drawLocal');
    assert.equal(painter.terrain.rebuilds, 1, 'drawn once for two frames');
    assert.equal(surfaces.length, 1);
    s.applySnapshot({...snapshot, time: 0.2});
    draw(painter, 'drawLocal');
    assert.equal(painter.terrain.rebuilds, 1, 'an unchanged snapshot (the same held parts) keeps it');
    const seen = rows.map((_, y) => (y === 3 ? '1' : '2').repeat(32));
    s.applySnapshot({...snapshot, time: 0.4, visibility: seen});
    draw(painter, 'drawLocal');
    assert.equal(painter.terrain.rebuilds, 2, 'a change in what the wolf sees redraws it');
    s.plainGlyphs = true;
    draw(painter, 'drawLocal');
    assert.equal(painter.terrain.rebuilds, 3, 'and so does a change of glyphs');
});

test("the player's own wolf answers at once, carried ahead while moving, eased rather than jumped", () => {
    const {state: s} = testGame();
    s.applySnapshot({cell: {id: 'room'}, self: {id: 'me', x: 5, y: 5}, entities: [{id: 'other', x: 2, y: 2}], time: 0, cellGeneration: 1});
    const frame = (time: number, x: number) => ({motionSession: '', observer: 'me', cellId: 'room', cellGeneration: 1, revision: 0, time,
        entities: [{id: 'me', x, y: 5, facing: 0, moving: true}, {id: 'other', x: 2 + time * 3, y: 2, facing: 0, moving: true}]});
    let t = 0;
    for (let i = 1; i <= 20; ++i) {
        s.applyMotion(frame(i * 0.05, 5 + i * 0.05 * 3));
        for (let f = 0; f < 3; ++f) s.tick((t += 1 / 60), 1 / 60);
    }
    const me = s.entities.get('me')!, other = s.entities.get('other')!;
    const newest = 5 + 20 * 0.05 * 3;
    assert.ok(newest - me.x < 0.25, `own wolf close to its newest pose (${me.x} of ${newest})`);
    assert.ok(me.x - (other.x + 3) > 0.05, `others are drawn further in the past, between real poses (${other.x + 3} against ${me.x})`);
    s.applyMotion(frame(1.05, 40));
    s.tick((t += 1 / 60), 1 / 60);
    assert.equal(me.x, 40, 'a jump of more than two tiles is taken at once');
});

test('looking: the pointer names what is under it, from what the client already holds', () => {
    const {state: s, painter} = testGame();
    const rows = ['..~~', '.PP.', '....'];
    s.applySnapshot({cell: {id: 'glade', width: 4, height: 3, rows, heights: ['PPPP', 'PPPP', 'PPRP']},
        visibility: ['2222', '2212', '0222'], self: {id: 'me', x: 0.5, y: 0.5}, entities: [{id: 'ash', name: 'Ash', x: 3.5, y: 2.5, npc: true,
            work: 'baker', state: 'kneading dough', lifeStage: 'old'}], doors: [{id: 'gate', name: 'The gate', x: 0.5, y: 2.5, open: false}],
        time: 0, cellGeneration: 1});
    s.tick(0.016, 0.016);
    s.mapRect = rect(0, 0, 400, 300);
    draw(painter, 'drawLocal');
    const at = (x: number, y: number) => lookAt(s, [s.mapOrigin[0] + x * s.tileSize, s.mapOrigin[1] + y * s.tileSize]);
    assert.equal(at(2.5, 0.5)!.what, 'Shallow water');
    assert.match(at(2.5, 0.5)!.why, /slower/i, 'with what it does to a wolf');
    assert.equal(at(1.5, 1.5)!.what, 'Pine');
    assert.equal(at(2.5, 1.5)!.what, 'Pine (remembered)', 'remembered ground says so');
    assert.equal(at(0.5, 2.5)!.what, 'The gate', 'a door before the ground under it');
    const ash = at(3.5, 2.5)!;
    assert.equal(ash.what, 'Ash');
    assert.match(ash.why, /Baker · old · kneading dough/);
    assert.match(at(2.5, 2.5)!.why, /above you/, 'height relative to the wolf');
    s.applySnapshot({cell: {id: 'glade', width: 4, height: 3, rows, heights: ['PPPP', 'PPPP', 'PPRP']}, visibility: ['2222', '2212', '0222'],
        self: {id: 'me', x: 0.5, y: 0.5}, entities: [], time: 0.2, cellGeneration: 1});
    assert.equal(at(0.5, 2.5)!.what, 'Unexplored', 'unseen ground gives nothing away');
    assert.equal(lookAt(s, [-5, -5]), null, 'nothing off the map');
    s.worldMap = true;
    assert.equal(at(2.5, 0.5), null);
});

test('talk targets: chosen from the list or a menu, sent with what is said, let go when out of sight or with Esc', () => {
    const {state: s, commands} = testGame();
    const snapshot: Json = {cell: {id: 'square'}, self: {id: 'me', x: 5, y: 5}, entities: [
        {id: 'ash', name: 'Ash', x: 6, y: 5, npc: true, actions: ['inspect', 'talk']},
        {id: 'bram', name: 'Bram', x: 14, y: 5, npc: true, actions: ['inspect', 'talk']},
        {id: 'wren', name: 'Wren', x: 5, y: 9, actions: ['inspect']}], time: 0, cellGeneration: 1};
    s.applySnapshot(snapshot);
    s.tick(0.016, 0.016);
    assert.equal(s.speakingTo().nearby?.name, 'Ash', 'with no one chosen, the one resident close by');
    s.sightClicked('ash', [0, 0]);
    s.sightClicked('bram', [0, 0]);
    assert.deepEqual(s.talkTargets, ['ash', 'bram']);
    s.sightClicked('wren', [10, 10]);
    assert.deepEqual(s.talkTargets, ['ash', 'bram'], 'a player is not a talk target: their row opens a menu');
    assert.equal(s.contextTarget, 'wren');
    s.contextTarget = '';
    s.setChat(true);
    s.composer.text = '"Good evening, both."';
    s.submitPost();
    const lastChat = () => commands.filter(c => c.type === 'chat').at(-1)!;
    assert.deepEqual(lastChat().targets, ['ash', 'bram'], 'sent with what is said');
    s.sightClicked('ash', [0, 0]);
    assert.deepEqual(s.talkTargets, ['bram'], 'clicked again, let go');
    s.receiveEvent({type: 'talkTarget', id: 'ash'});
    assert.deepEqual(s.talkTargets, ['bram', 'ash'], 'the Talk menu item adds one');
    s.receiveEvent({type: 'roleplay', id: 'r1', speaker: 'Ash', text: 'Evening.', to: ['you']});
    assert.deepEqual(s.posts.at(-1)!.to, ['you'], 'a reply knows it was for you');
    s.applySnapshot({...snapshot, time: 0.2, entities: [(snapshot.entities as Json[])[0]]});
    s.tick(0.3, 0.016);
    assert.deepEqual(s.talkTargets, ['ash'], 'out of sight, let go');
    s.keyDown({code: 'Escape'});
    assert.deepEqual(s.talkTargets, [], 'Esc on the map lets go of everyone');
    s.setChat(true);
    s.composer.text = '"Anyone?"';
    s.submitPost();
    assert.equal('targets' in lastChat(), false, 'with no one chosen, none are sent');
});

test('regional weather: the weather where the wolf stands, its strength in words, and the field for drawing', () => {
    const {state: s} = testGame();
    const field = {step: 16, cols: 3, rows: 2, kinds: 'rrcrfc', amounts: '930410'};
    s.applySnapshot({cell: {id: 'downs', outdoors: true, weather: 'rain', localWeather: {kind: 'rain', intensity: 0.2}, weatherField: field},
        self: {id: 'me', x: 1, y: 1}, time: 0, cellGeneration: 1});
    assert.equal(s.environment.weather, 'rain');
    assert.equal(s.environment.intensity, 0.2);
    assert.equal(environmentLabel(12, 'day', s.environment.weather, true, s.environment.intensity), '12:00 DAY · LIGHT RAIN');
    assert.equal(environmentLabel(12, 'day', 'snow', true, 0.9), '12:00 DAY · HEAVY SNOW');
    assert.deepEqual(s.weatherField, field);
    s.applySnapshot({cell: {id: 'downs', outdoors: true, weather: 'fog', weatherField: {...field, kinds: 'rr'}}, self: {id: 'me', x: 1, y: 1},
        time: 0.2, cellGeneration: 1});
    assert.equal(s.weatherField, null, 'a malformed field is not drawn');
    assert.equal(s.environment.weather, 'fog', 'without local weather, the cell\'s');
});

test('the map of the country: places where they truly are and as large as they are, centred on the wolf', () => {
    const {state: s} = testGame();
    s.applySnapshot({cell: {id: 'a', x: 0, y: 0, width: 256, height: 256, outdoors: true}, self: {id: 'me', x: 128, y: 128},
        worldMap: [{id: 'a', name: 'Here', x: 0, y: 0, width: 256, height: 256, knowledge: 'visited', current: true},
            {id: 'b', name: 'East', x: 256, y: 0, width: 256, height: 128, knowledge: 'visited'},
            {id: 'c', name: 'Never seen', x: -256, y: 0, width: 256, height: 256, knowledge: 'unknown'}],
        time: 0, cellGeneration: 1});
    s.tick(0.016, 0.016);
    const rects: number[][] = [];
    const record = new Proxy({} as Record<string, unknown>, {
        get: (_, key) => (key === 'fillRect' ? (...a: number[]) => rects.push(a) : key === 'canvas' ? undefined : () => {}),
        set: () => true,
    }) as unknown as CanvasRenderingContext2D;
    new MapRenderer(() => null).draw(record, s, {x: 0, y: 0, w: 400, h: 300}, 0.5);
    // Without offscreen canvases, each place is a box: Here centred on the wolf, East beside it, the unknown not at all.
    const boxes = rects.filter(r => r[2] !== 400 && r[2] !== 3);
    assert.deepEqual(boxes[0], [200 - 64, 150 - 64, 128, 128], 'here: 256 tiles at half a pixel a tile, the wolf in the middle');
    assert.deepEqual(boxes[1], [200 + 64, 150 - 64, 128, 64], 'east: beside it, as wide and as tall as it is');
    assert.equal(boxes.length, 2, 'a place never seen is not drawn');
});

test('playtest fixes: attacking a resident asks first; faint voices once; one\'s own words at once', () => {
    const {state: s, commands} = testGame();
    s.applySnapshot({self: {id: 'self', name: 'Oriel', x: 5, y: 5}, cell: {id: 'plaza'},
        entities: [{id: 'npc_porter', kind: 'npc', name: 'A wolf who carries loads for hire', x: 6, y: 5, actions: ['inspect', 'attack']},
            {id: 'bandit_1', kind: 'npc', name: 'Bandit', hostile: true, x: 4, y: 5, actions: ['inspect', 'attack']}]});
    s.contextTarget = 'npc_porter';
    s.activate({rect: rect(0, 0, 0, 0), action: 'context', target: 'attack'});
    assert.ok(!commands.some(c => c.action === 'attack'), 'the first Attack on a resident only asks');
    assert.equal(s.contextTarget, 'npc_porter', 'and the menu stays open');
    assert.equal(s.armed, 'attack|npc_porter');
    s.activate({rect: rect(0, 0, 0, 0), action: 'context', target: 'attack'});
    assert.ok(commands.some(c => c.action === 'attack' && c.target === 'npc_porter'), 'the second goes for them');
    commands.length = 0;
    s.contextTarget = 'bandit_1';
    s.activate({rect: rect(0, 0, 0, 0), action: 'context', target: 'attack'});
    assert.ok(commands.some(c => c.action === 'attack' && c.target === 'bandit_1'), 'a bandit is fought without asking');
    const before = s.posts.length;
    s.receiveEvent({type: 'roleplay', id: '1', speaker: 'A voice', anonymous: true, text: '"..."', channel: 'ic'});
    s.receiveEvent({type: 'roleplay', id: '2', speaker: 'A voice', anonymous: true, text: '"..."', channel: 'ic'});
    assert.equal(s.posts.length, before + 1, 'a voice too far off is said once');
    assert.equal(s.posts.at(-1)!.text, 'Words too far off to make out.');
    s.receiveEvent({type: 'roleplay', id: '3', speaker: 'Oriel', text: 'Oriel circles, breath ragged.', channel: 'ic'});
    assert.equal(s.posts.at(-1)!.revealed, s.posts.at(-1)!.text.length, 'one\'s own words are not written out again');
});

test('the Dev Console: only a Dungeon Master has it; slash commands go to it, never into the world as words', () => {
    const {state: s, commands, composer} = testGame();
    s.snapshot = {self: {name: 'Ada'}};
    s.keyDown({code: 'Backquote'});
    assert.ok(!s.devConsole, 'a player who is not a Dungeon Master has no console');
    s.activate({rect: rect(0, 0, 0, 0), action: 'dev_console', target: ''});
    assert.ok(!s.devConsole, 'not even asked for');
    s.runDevCommand('/fight-test-1');
    assert.equal(commands.filter(c => c.type === 'dev' || c.type === 'devCommands').length, 0, 'and sends no console commands');
    s.keyDown({code: 'Enter'});
    composer.text = '/fight-test-1';
    s.composerKey({code: 'Enter'});
    assert.ok(commands.some(c => c.type === 'chat' && c.text === '/fight-test-1'), 'their slash is only words');

    commands.length = 0;
    s.snapshot = {self: {name: 'Ada', dungeonMaster: true}};
    s.keyDown({code: 'Backquote'});
    assert.ok(s.devConsole, '` opens the console');
    assert.deepEqual(commands.filter(c => c.type === 'devCommands'), [{type: 'devCommands'}], 'which asks the server what it offers');
    s.receiveEvent({type: 'devCommands', commands: [['/help', 'Lists them.'], ['/fight-test-1', 'A fight.'], ['/fight-end-myself', 'Ends it.']]});
    assert.deepEqual(s.devSuggestions('').map(c => c[0]), ['/fight-end-myself', '/fight-test-1', '/help'], 'all of them, alphabetically');
    assert.deepEqual(s.devSuggestions('/').map(c => c[0]), ['/fight-end-myself', '/fight-test-1', '/help']);
    assert.deepEqual(s.devSuggestions('fi').map(c => c[0]), ['/fight-end-myself', '/fight-test-1'], 'those that begin as typed');
    assert.deepEqual(s.devSuggestions('/FIGHT-T').map(c => c[0]), ['/fight-test-1'], 'whatever the case');
    assert.deepEqual(s.devSuggestions('/zz'), [], 'none that begin otherwise');
    s.keyDown({code: 'Backquote'});
    assert.ok(!s.devConsole, 'and closes it');
    s.activate({rect: rect(0, 0, 0, 0), action: 'dev_console', target: ''});
    assert.ok(s.devConsole, 'the top bar button opens it too');
    s.keyDown({code: 'Escape'});
    assert.ok(!s.devConsole, 'Esc closes it');
    s.keyDown({code: 'Enter'});
    composer.text = '/fight-test-1';
    s.composerKey({code: 'Enter'});
    assert.deepEqual(commands.filter(c => c.type === 'dev' || c.type === 'chat'), [{type: 'dev', command: '/fight-test-1'}],
        "a Dungeon Master's slash command goes to the console");
    assert.ok(!s.chat && composer.text === '' && s.devConsole, 'and opens it, where the answer is');
    s.runDevCommand('fight-end-myself');
    assert.deepEqual(commands.at(-1), {type: 'dev', command: '/fight-end-myself'}, 'typed without its slash, it gets one');
    s.receiveEvent({type: 'devResult', command: '/fight-end-myself', ok: false, text: "You aren't in a fight."});
    assert.deepEqual(s.devLog, [{command: '/fight-end-myself', ok: false, text: "You aren't in a fight."}], 'the answer goes to its log');
    assert.equal(s.posts.length, 0, 'not into the story');
});
