// The dialogue window's face, as a thing that moves (Docs/Design/83-dialogue-trees.md, Phase 5: the animation hooks).
// There is one painting of a face today (doc 81's face.png) and no expressions, so the face only *moves*: it lifts and
// nods gently while the resident speaks, tilts a little while they listen, breathes when idle, and wears its mood as a
// tint and a frame. Everything here is pure (the window's FaceSlot draws it), so painted frames or a warp can drop in:
//
//   - faceActivity(): what the face is doing (idle, talking, listening), from the conversation and the clock.
//   - faceState():    that, the mood, and when the activity began (the window calls FaceSlot.setState with it).
//   - facePose():     how the paper moves at a moment (lift, tilt, scale): a CSS transform now; a warp's inputs later.
//   - faceFrame():    which painting to draw at a moment. Always 'face' now. Painted expressions (doc 83, "What real
//                     animation needs") are new portrait views, 'face.talk.1', 'face.blink', 'face.mood.warm'..., each
//                     with its own zone map so the fur recolour still works; this function picks among those that exist.
//
// The moods are the server's closed list (Core: talk::Library::faceMoods): a node's `mood` tag, the resident's state
// (asleep, grieving, hurt, worn out), the NPC Mind's emotion, or how they feel about the wolf. '' is the face at rest.
import type {Dialogue, DialogueUi} from './dialogue.ts';

export const FaceMoods = ['calm', 'warm', 'pleased', 'keen', 'formal', 'stern', 'wary', 'cross', 'sad', 'worried', 'tired',
    'sleepy', 'hurt'] as const;
export type FaceMood = typeof FaceMoods[number] | '';
export type FaceActivity = 'idle' | 'talking' | 'listening';

export interface FaceState {
    activity: FaceActivity;
    mood: FaceMood;
    since: number;              // The clock (seconds) when the activity began: a frame loop's phase starts there.
    speaking: boolean;          // activity === 'talking' (Phase 1's flag, kept for the CSS class).
}

/** A mood the face knows, or none (an older or newer server's word it doesn't). */
export function faceMood(word: string): FaceMood {
    return (FaceMoods as readonly string[]).includes(word) ? word as FaceMood : '';
}

/** How long the line is being spoken (seconds): the server's `speaking`, else as it writes out (sixty a second). */
export function speakingFor(d: Dialogue): number {
    return Math.max(d.speaking, d.line.length / 60);
}

/** What the face is doing now: listening while the player's own line is being answered (or while they write one),
 *  talking while the line is spoken, else idle. */
export function faceActivity(d: Dialogue, ui: DialogueUi, now: number): FaceActivity {
    if (d.awaiting) return 'listening';
    if (d.line && !ui.whole && now - ui.lineAt < speakingFor(d)) return 'talking';
    if (ui.focus === 'edit' || ui.focus === 'free') return 'listening';
    return 'idle';
}

/** The face's state, keeping `since` while the activity holds (so a frame loop's phase doesn't jump each frame). */
export function faceState(d: Dialogue, ui: DialogueUi, now: number, last?: FaceState): FaceState {
    const activity = faceActivity(d, ui, now), mood = faceMood(d.mood);
    const since = last && last.activity === activity ? last.since : activity === 'talking' ? ui.lineAt : now;
    return {activity, mood, since, speaking: activity === 'talking'};
}

/** How the paper sits at a moment: lifted (px, up), tilted (degrees, clockwise) and scaled. Gentle on purpose: a bob
 *  of about two syllables a second while talking, a slow breath idle, a small tilt of the head listening. A sleepy or
 *  tired face moves slower and less; a keen or cross one a little more. Still under reduced motion. */
export interface FacePose {
    lift: number;
    tilt: number;
    scale: number;
}
export const RestingPose: FacePose = {lift: 0, tilt: 0, scale: 1};

export function facePose(state: FaceState, now: number, reduced = false): FacePose {
    if (reduced) return RestingPose;
    const t = Math.max(0, now - state.since);
    const energy = state.mood === 'sleepy' || state.mood === 'tired' || state.mood === 'sad' || state.mood === 'hurt' ? .6
        : state.mood === 'keen' || state.mood === 'cross' || state.mood === 'pleased' ? 1.25 : 1;
    const ease = Math.min(1, t / .25);                          // (Into each activity over a quarter second.)
    if (state.activity === 'talking') {
        const bob = Math.abs(Math.sin(t * Math.PI * 2.2 * energy));
        return {lift: ease * (1 + 2.2 * bob) * energy, tilt: ease * .6 * Math.sin(t * Math.PI * .9), scale: 1 + ease * .012 * bob};
    }
    if (state.activity === 'listening')
        return {lift: ease * 1, tilt: ease * -2.5 * energy, scale: 1};
    const breath = Math.sin(t * Math.PI * 2 / (energy < 1 ? 5.5 : 4.2));
    return {lift: .7 * energy * (breath + 1) / 2, tilt: 0, scale: 1 + .004 * (breath + 1) / 2};
}

/** The painting for a moment. Only 'face' exists (doc 81): a painted set would answer 'face.talk.N' while talking,
 *  'face.blink' now and then, and 'face.mood.<mood>' at rest, falling back to 'face' for any it lacks (`has`). */
export function faceFrame(state: FaceState, now: number, has: (view: string) => boolean = v => v === 'face'): string {
    const t = Math.max(0, now - state.since);
    const wanted: string[] = [];
    if (state.activity === 'talking') wanted.push(`face.talk.${Math.floor(t * 8) % 2}`);
    if (state.activity !== 'talking' && t % 4.3 < .14) wanted.push('face.blink');
    if (state.mood) wanted.push(`face.mood.${state.mood}`);
    return wanted.find(has) ?? 'face';
}

/** The CSS transform for a pose (the paper moves; the painting inside it is drawn once and cached). */
export function poseTransform(p: FacePose): string {
    if (p.lift === 0 && p.tilt === 0 && p.scale === 1) return '';
    return `translateY(${(-p.lift).toFixed(2)}px) rotate(${p.tilt.toFixed(2)}deg) scale(${p.scale.toFixed(4)})`;
}
