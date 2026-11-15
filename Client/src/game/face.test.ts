// The dialogue window's face as a thing that moves (Docs/Design/83-dialogue-trees.md, Phase 5; game/face.ts): talking
// while the line is spoken, listening while the player's own line is answered or written, idle else; the mood from the
// server's closed list; a gentle pose (still under reduced motion); and the one painting until expressions are painted.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {newDialogueUi, readDialogue} from './dialogue.ts';
import {faceActivity, faceFrame, faceMood, facePose, faceState, poseTransform, speakingFor, RestingPose} from './face.ts';
import type {Json} from './json.ts';

const node = (over: Json = {}) => readDialogue({
    type: 'dialogue', session: 7, npc: 'bram', name: 'Bram', tree: 'shop', node: 'greet',
    line: 'Morning, friend. What can I get you?', mood: 'warm', speaking: 2.6, choices: [{n: 1, text: 'Goodbye.'}], ...over});

test('the face talks while the line is spoken, listens while answered or written to, and rests after', () => {
    const d = node(), ui = {...newDialogueUi(), lineAt: 10};
    assert.equal(d.speaking, 2.6);
    assert.equal(speakingFor(d), 2.6, "the server's time, longer than the writing out");
    assert.equal(faceActivity(d, ui, 10.1), 'talking');
    assert.equal(faceActivity(d, ui, 12.5), 'talking');
    assert.equal(faceActivity(d, ui, 12.7), 'idle');
    assert.equal(faceActivity(d, {...ui, whole: true}, 10.5), 'idle', 'shown whole: done speaking');
    assert.equal(faceActivity(d, {...ui, focus: 'free'}, 13), 'listening', 'while the player writes their own line');
    assert.equal(faceActivity(node({awaiting: true}), ui, 10.1), 'listening', 'while their line is answered');
    const older = node({speaking: undefined});
    assert.equal(older.speaking, 0, 'an older server sends no time');
    assert.ok(Math.abs(speakingFor(older) - older.line.length / 60) < 1e-9, 'it talks while the line writes out');
});

test('the mood is one the face knows, and the state keeps its start while the activity holds', () => {
    assert.equal(faceMood('warm'), 'warm');
    assert.equal(faceMood('sleepy'), 'sleepy');
    assert.equal(faceMood('jubilant'), '', 'an unknown word is no mood');
    assert.equal(node({mood: 'nonsense'}).mood, 'nonsense', 'the message keeps it; the face reads it as none');
    const d = node(), ui = {...newDialogueUi(), lineAt: 10};
    const a = faceState(d, ui, 10.2);
    assert.deepEqual(a, {activity: 'talking', mood: 'warm', since: 10, speaking: true});
    const b = faceState(d, ui, 11, a);
    assert.equal(b.since, 10, 'still talking: the same start');
    const c = faceState(d, ui, 13, b);
    assert.deepEqual([c.activity, c.since, c.speaking], ['idle', 13, false], 'idle from now');
    assert.equal(faceState(node({mood: 'nonsense'}), ui, 13).mood, '');
});

test('the pose is gentle, moves while talking, tilts listening, and is still under reduced motion', () => {
    const talking = {activity: 'talking' as const, mood: '' as const, since: 0, speaking: true};
    const lifts = Array.from({length: 40}, (_, i) => facePose(talking, .3 + i * .05).lift);
    assert.ok(Math.max(...lifts) - Math.min(...lifts) > 1.5, 'a bob while talking');
    assert.ok(Math.max(...lifts) <= 4, 'a few pixels at most');
    assert.ok(Math.abs(facePose(talking, 0).lift) < 1e-9, 'eased in from rest');
    const listening = facePose({activity: 'listening', mood: '', since: 0, speaking: false}, 1);
    assert.ok(listening.tilt < -1 && listening.tilt > -4, 'a small tilt of the head');
    const sleepy = facePose({...talking, mood: 'sleepy'}, 1.5), keen = facePose({...talking, mood: 'keen'}, 1.5);
    assert.notDeepEqual(sleepy, keen, 'the mood changes how it moves');
    const idle = facePose({activity: 'idle', mood: '', since: 0, speaking: false}, 2.1);
    assert.ok(idle.lift >= 0 && idle.lift <= 1 && idle.tilt === 0, 'a slow breath idle');
    assert.deepEqual(facePose(talking, 1.2, true), RestingPose, 'still under reduced motion');
    assert.equal(poseTransform(RestingPose), '');
    assert.match(poseTransform({lift: 2, tilt: 1, scale: 1.01}), /^translateY\(-2\.00px\) rotate\(1\.00deg\) scale\(1\.0100\)$/);
});

test('one painting now; painted expressions drop in by view', () => {
    const talking = {activity: 'talking' as const, mood: 'warm' as const, since: 0, speaking: true};
    for (let t = 0; t < 5; t += .07) assert.equal(faceFrame(talking, t), 'face', 'only the face is painted');
    const painted = new Set(['face', 'face.talk.0', 'face.talk.1', 'face.blink', 'face.mood.warm']);
    const has = (v: string) => painted.has(v);
    const mouths = new Set(Array.from({length: 20}, (_, i) => faceFrame(talking, i * .07, has)));
    assert.deepEqual([...mouths].sort(), ['face.talk.0', 'face.talk.1'], 'a mouth while talking');
    const idle = {activity: 'idle' as const, mood: 'warm' as const, since: 0, speaking: false};
    assert.equal(faceFrame(idle, 1, has), 'face.mood.warm', 'the mood at rest');
    assert.equal(faceFrame(idle, 4.35, has), 'face.blink', 'a blink now and then');
    assert.equal(faceFrame({...idle, mood: 'sad'}, 1, has), 'face', 'a mood not painted is the face');
});
