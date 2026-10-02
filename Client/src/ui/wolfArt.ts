// The wolf portrait (Docs/Design/29-client-polish.md, phase 9): one stylised wolf, drawn in code from its parts, so
// every species, sex, build and age is the same drawing with different proportions, and every marking lines up with
// the part it belongs to. Coat, gradient and markings are free colours (or the old palette). Side view, facing right,
// in a 500×340 design space scaled into the box asked for.
import type {Appearance, LifeStage} from './portrait.ts';
import {CoatColors} from './theme.ts';

export const Masks = ['socks', 'stockings', 'blaze', 'mask', 'cape', 'bib', 'belly', 'tail_tip', 'ear_tips', 'freckles', 'brindle',
    'merle', 'scar', 'eye_patches', 'saddle'] as const;
export const MaskNames: Record<string, string> = {socks: 'Socks', stockings: 'Stockings', blaze: 'Blaze', mask: 'Face mask', cape: 'Cape',
    bib: 'Chest bib', belly: 'Pale belly', tail_tip: 'Tail tip', ear_tips: 'Ear tips', freckles: 'Muzzle freckles', brindle: 'Brindle stripes',
    merle: 'Merle patches', scar: 'Old scar', eye_patches: 'Eye patches', saddle: 'Saddle'};

interface Shape {
    legLen: number;
    bodyLen: number;
    depth: number;
    head: number;
    muzzle: number;
    ear: number;
    tail: number;
}
const Species: Record<string, Shape> = {
    timber: {legLen: 95, bodyLen: 210, depth: 80, head: 52, muzzle: 46, ear: 26, tail: 1},
    maned: {legLen: 132, bodyLen: 188, depth: 64, head: 46, muzzle: 50, ear: 40, tail: 0.8},
    arctic: {legLen: 84, bodyLen: 205, depth: 88, head: 54, muzzle: 38, ear: 20, tail: 1.25},
    red: {legLen: 92, bodyLen: 196, depth: 72, head: 48, muzzle: 48, ear: 29, tail: 0.95},
    ethiopian: {legLen: 100, bodyLen: 186, depth: 62, head: 44, muzzle: 58, ear: 33, tail: 0.85},
};

const hexOf = (i: number) => `#${(CoatColors[Math.max(0, Math.min(7, i))] ?? 0x888888).toString(16).padStart(6, '0')}`;
function shade(hex: string, f: number): string {
    const n = parseInt(hex.slice(1), 16);
    const ch = (v: number) => Math.max(0, Math.min(255, Math.round(f >= 1 ? v + (255 - v) * (f - 1) : v * f)));
    return `rgb(${ch((n >> 16) & 255)},${ch((n >> 8) & 255)},${ch(n & 255)})`;
}

/** A little seeded chance, so merle patches are the same for the same wolf. */
function seeded(text: string) {
    let h = 2166136261;
    for (let i = 0; i < text.length; ++i) h = Math.imul(h ^ text.charCodeAt(i), 16777619);
    return () => {
        h = Math.imul(h ^ (h >>> 15), 2246822507);
        h = Math.imul(h ^ (h >>> 13), 3266489909);
        return ((h ^= h >>> 16) >>> 0) / 4294967296;
    };
}

interface Parts {
    farLegs: Path2D[];
    nearLegs: Path2D[];
    legTops: Array<{x: number; top: number; bottom: number}>;
    torso: Path2D;
    neck: Path2D;
    head: Path2D;
    muzzle: Path2D;
    ears: Path2D;
    tail: Path2D;
    tailTip: {x: number; y: number};
    eye: {x: number; y: number; r: number};
    nose: {x: number; y: number};
    top: number;
    bottom: number;
    xs: number;
    xh: number;
    hx: number;
    hy: number;
    hr: number;
}

/** A leg: a broad thigh (or shoulder) narrowing to the knee and the paw; a hind leg bends back at the hock. */
function leg(x: number, top: number, ground: number, thick: number, hind: boolean): Path2D {
    const p = new Path2D();
    const h = ground - top;
    const knee = top + h * (hind ? 0.42 : 0.5), hock = top + h * 0.72;
    const thigh = thick * (hind ? 1.9 : 1.45), mid = thick * 0.62, low = thick * 0.5;
    const kx = x + (hind ? -thick * 0.55 : thick * 0.1), hx = x + (hind ? thick * 0.25 : 0);
    p.moveTo(x - thigh / 2, top);
    p.bezierCurveTo(x - thigh / 2, top + h * 0.25, kx - mid / 2, knee - h * 0.1, kx - mid / 2, knee);
    p.quadraticCurveTo(hx - low / 2 - (hind ? 2 : 0), hock, hx - low / 2, ground - 7);
    // The paw, a little forward.
    p.quadraticCurveTo(hx - low / 2, ground, hx, ground);
    p.lineTo(hx + low * 1.05, ground);
    p.quadraticCurveTo(hx + low * 1.25, ground - 2, hx + low / 2, ground - 8);
    p.quadraticCurveTo(hx + low / 2 + (hind ? 3 : 0), hock, kx + mid / 2, knee);
    p.bezierCurveTo(kx + mid / 2, knee - h * 0.12, x + thigh / 2, top + h * 0.2, x + thigh / 2, top);
    p.closePath();
    return p;
}

function build(a: Appearance, stage: LifeStage): Parts {
    const base = Species[a.species] ?? Species.timber;
    const male = a.sex === 'male';
    const bulk = (a.build === 'lean' ? 0.9 : a.build === 'heavy' ? 1.12 : 1) * (male ? 1.06 : 0.96);
    const young = stage === 'young', teen = stage === 'adolescent';
    const legLen = base.legLen * (young ? 0.72 : teen ? 0.9 : 1);
    const bodyLen = base.bodyLen * (young ? 0.7 : teen ? 0.88 : 1);
    const depth = base.depth * bulk * (young ? 0.78 : teen ? 0.9 : 1);
    const hr = base.head * (male ? 1.04 : 0.97) * (young ? 0.92 : 1) / 2;
    const muzzle = base.muzzle * (young ? 0.66 : teen ? 0.85 : 1);
    const ear = base.ear * (young ? 1.1 : 1);
    const ground = 312;
    const x0 = 250 - bodyLen / 2 - 40;
    const xs = x0 + bodyLen * 0.82, xh = x0 + bodyLen * 0.18;
    const bottom = ground - legLen, top = bottom - depth;
    const torso = new Path2D();
    torso.moveTo(xs + depth * 0.32, top + depth * 0.42);
    torso.bezierCurveTo(xs + depth * 0.2, top - 6, xs - bodyLen * 0.2, top + 2, x0 + bodyLen * 0.5, top + 6);
    torso.bezierCurveTo(xh + bodyLen * 0.1, top + 8, xh - depth * 0.4, top - 2, xh - depth * 0.42, top + depth * 0.45);
    torso.bezierCurveTo(xh - depth * 0.42, bottom - depth * 0.1, xh - 4, bottom + 2, xh + bodyLen * 0.16, bottom - depth * 0.18);
    // The waist tucked up, the chest deep and low behind the forelegs.
    torso.bezierCurveTo(x0 + bodyLen * 0.45, bottom - depth * 0.42, xs - bodyLen * 0.28, bottom + 2, xs - 6, bottom + 10);
    torso.bezierCurveTo(xs + depth * 0.28, bottom + 8, xs + depth * 0.46, top + depth * 0.72, xs + depth * 0.32, top + depth * 0.42);
    torso.closePath();
    const thick = 20 * bulk * (young ? 0.8 : 1);
    const legTops = [{x: xs - 4, top: bottom - depth * 0.45, bottom: ground}, {x: xh + 16, top: bottom - depth * 0.55, bottom: ground},
        {x: xs - 24, top: bottom - depth * 0.45, bottom: ground - 2}, {x: xh - 6, top: bottom - depth * 0.55, bottom: ground - 2}];
    const nearLegs = [leg(legTops[0].x, legTops[0].top, ground, thick, false), leg(legTops[1].x, legTops[1].top, ground, thick * 1.1, true)];
    const farLegs = [leg(legTops[2].x, legTops[2].top, ground - 2, thick * 0.92, false), leg(legTops[3].x, legTops[3].top, ground - 2, thick, true)];
    // The head sits forward and up from the shoulders; the neck joins them (with a ruff for a male).
    const hx = xs + depth * 0.62, hy = top - hr * 0.55 - (young ? 4 : 10);
    const neck = new Path2D();
    neck.moveTo(xs - depth * 0.25, top + 4);
    neck.quadraticCurveTo(xs + 4, hy - hr * 0.5, hx - hr * 0.3, hy - hr * 0.55);
    neck.lineTo(hx + hr * 0.1, hy + hr * 0.8);
    neck.quadraticCurveTo(xs + depth * 0.45, top + depth * 0.5, xs + depth * 0.3, top + depth * (male ? 0.62 : 0.5));
    neck.closePath();
    const head = new Path2D();
    head.ellipse(hx, hy, hr * 1.02, hr * 0.86, -0.1, 0, Math.PI * 2);
    const muzzlePath = new Path2D();
    const mx = hx + hr * 0.6, my = hy - hr * 0.05;
    muzzlePath.moveTo(mx, my - hr * 0.45);
    muzzlePath.quadraticCurveTo(mx + muzzle * 0.6, my - hr * 0.42, mx + muzzle, my + hr * 0.02);
    muzzlePath.quadraticCurveTo(mx + muzzle * 0.98, my + hr * 0.3, mx + muzzle * 0.82, my + hr * 0.36);
    muzzlePath.quadraticCurveTo(mx + muzzle * 0.4, my + hr * 0.5, mx - hr * 0.1, my + hr * 0.62);
    muzzlePath.closePath();
    const ears = new Path2D();
    for (const [ex, lean] of [[hx - hr * 0.35, -0.25], [hx + hr * 0.05, 0.05]] as const) {
        const ey = hy - hr * 0.62;
        ears.moveTo(ex - ear * 0.35, ey + 4);
        ears.lineTo(ex + Math.sin(lean) * ear, ey - ear);
        ears.lineTo(ex + ear * 0.38, ey + 2);
        ears.closePath();
    }
    // The tail falls from the rump, fuller for the arctic, slighter for the slender kinds.
    const tw = 18 * base.tail * (young ? 0.8 : 1), tl = 105 * base.tail * (young ? 0.75 : 1);
    const tx = xh - depth * 0.38, ty = top + depth * 0.25;
    const tail = new Path2D();
    tail.moveTo(tx, ty);
    tail.bezierCurveTo(tx - tl * 0.45, ty + tl * 0.1, tx - tl * 0.62, ty + tl * 0.55, tx - tl * 0.5, ty + tl * 0.95);
    tail.quadraticCurveTo(tx - tl * 0.38, ty + tl * 1.05, tx - tl * 0.3, ty + tl * 0.9);
    tail.bezierCurveTo(tx - tl * 0.3 + tw * 0.5, ty + tl * 0.5, tx - tl * 0.1, ty + tw * 1.2, tx + 4, ty + tw * 1.1);
    tail.closePath();
    return {farLegs, nearLegs, legTops, torso, neck, head, muzzle: muzzlePath, ears, tail, tailTip: {x: tx - tl * 0.42, y: ty + tl * 0.92},
        eye: {x: hx + hr * 0.42, y: hy - hr * 0.18, r: hr * 0.16}, nose: {x: mx + muzzle * 0.96, y: my + hr * 0.04}, top, bottom, xs, xh, hx, hy, hr};
}

/** The part(s) a marking belongs to, and its shape within them. */
function maskShape(mask: string, p: Parts, rnd: () => number): {clip: Path2D; shape: Path2D} | null {
    const union = (...paths: Path2D[]) => {
        const u = new Path2D();
        for (const q of paths) u.addPath(q);
        return u;
    };
    const s = new Path2D();
    const legs = union(...p.nearLegs, ...p.farLegs);
    switch (mask) {
    case 'socks':
    case 'stockings': {
        const share = mask === 'socks' ? 0.25 : 0.55;
        for (const l of p.legTops) s.rect(l.x - 40, l.bottom - (l.bottom - l.top) * share, 80, 60);
        return {clip: legs, shape: s};
    }
    case 'blaze':
        s.moveTo(p.hx - p.hr * 0.05, p.hy - p.hr * 0.75);
        s.lineTo(p.hx + p.hr * 0.25, p.hy - p.hr * 0.75);
        s.lineTo(p.nose.x - 4, p.nose.y - p.hr * 0.12);
        s.lineTo(p.nose.x - 18, p.nose.y + p.hr * 0.05);
        s.closePath();
        return {clip: union(p.head, p.muzzle), shape: s};
    case 'mask':
        s.ellipse(p.eye.x + p.hr * 0.3, p.eye.y + p.hr * 0.05, p.hr * 0.95, p.hr * 0.5, 0, 0, Math.PI * 2);
        return {clip: union(p.head, p.muzzle), shape: s};
    case 'eye_patches':
        s.ellipse(p.eye.x, p.eye.y, p.hr * 0.42, p.hr * 0.32, -0.2, 0, Math.PI * 2);
        return {clip: p.head, shape: s};
    case 'cape':
        s.rect(p.xs - (p.xs - p.xh) * 0.75, p.top - p.hr * 3, (p.xs - p.xh) * 1.4, (p.bottom - p.top) * 0.42 + p.hr * 3);
        return {clip: union(p.torso, p.neck), shape: s};
    case 'saddle':
        s.ellipse((p.xs + p.xh) / 2, p.top + 4, (p.xs - p.xh) * 0.42, (p.bottom - p.top) * 0.42, 0, 0, Math.PI * 2);
        return {clip: p.torso, shape: s};
    case 'bib':
        s.ellipse(p.xs + (p.bottom - p.top) * 0.28, p.top + (p.bottom - p.top) * 0.4, (p.bottom - p.top) * 0.32, (p.bottom - p.top) * 0.55, 0, 0, Math.PI * 2);
        return {clip: union(p.torso, p.neck), shape: s};
    case 'belly':
        s.rect(p.xh - 40, p.bottom - (p.bottom - p.top) * 0.32, p.xs - p.xh + 120, 80);
        return {clip: p.torso, shape: s};
    case 'tail_tip':
        s.arc(p.tailTip.x, p.tailTip.y, 30, 0, Math.PI * 2);
        return {clip: p.tail, shape: s};
    case 'ear_tips':
        s.rect(p.hx - p.hr * 2, p.hy - p.hr * 3, p.hr * 4, p.hr * 1.75);
        return {clip: p.ears, shape: s};
    case 'freckles':
        for (let i = 0; i < 9; ++i) s.arc(p.nose.x - 10 - rnd() * 32, p.nose.y - 6 + rnd() * 14, 1.6, 0, Math.PI * 2);
        return {clip: p.muzzle, shape: s};
    case 'brindle':
        for (let x = p.xh - 60; x < p.xs + 60; x += 14) {
            s.moveTo(x, p.top - 10);
            s.lineTo(x + 6, p.top - 10);
            s.lineTo(x - 18, p.bottom + 10);
            s.lineTo(x - 24, p.bottom + 10);
            s.closePath();
        }
        return {clip: union(p.torso, p.neck), shape: s};
    case 'merle':
        for (let i = 0; i < 11; ++i) {
            const x = p.xh - 20 + rnd() * (p.xs - p.xh + 60), y = p.top + rnd() * (p.bottom - p.top);
            s.ellipse(x, y, 8 + rnd() * 16, 6 + rnd() * 12, rnd() * 3, 0, Math.PI * 2);
        }
        return {clip: union(p.torso, p.neck, p.tail), shape: s};
    case 'scar':
        s.moveTo(p.eye.x - p.hr * 0.6, p.eye.y - p.hr * 0.5);
        s.lineTo(p.eye.x - p.hr * 0.5, p.eye.y - p.hr * 0.56);
        s.lineTo(p.eye.x + p.hr * 0.5, p.eye.y + p.hr * 0.55);
        s.lineTo(p.eye.x + p.hr * 0.4, p.eye.y + p.hr * 0.6);
        s.closePath();
        return {clip: union(p.head, p.muzzle), shape: s};
    }
    return null;
}

/** The markings to draw: the chosen ones, else the old pattern in the old marking colour. */
function markingsOf(a: Appearance): Array<{mask: string; color: string; opacity: number}> {
    if (a.markings?.length) return a.markings;
    const color = a.markingTint || hexOf(a.markingColor), opacity = a.patternAmount;
    if (a.pattern === 'saddle') return [{mask: 'saddle', color, opacity}];
    if (a.pattern === 'mantle') return [{mask: 'cape', color, opacity}, {mask: 'saddle', color, opacity}];
    if (a.pattern === 'piebald') return [{mask: 'merle', color, opacity}, {mask: 'socks', color, opacity}];
    return [];
}

/** Draws the wolf into a canvas of 500×340 (the caller scales it). */
export function drawWolf(c: CanvasRenderingContext2D, a: Appearance, stage: LifeStage) {
    const p = build(a, stage);
    const coat = a.coat || hexOf(a.baseColor), under = a.gradientTint || hexOf(a.gradientColor);
    // The coat from the back down, giving way to the belly colour as far as the gradient's strength reaches.
    const reach = Math.max(0, Math.min(1, a.gradientAmount));
    const fill = (dark = 1) => {
        const g = c.createLinearGradient(0, p.top - p.hr * 2, 0, 316);
        g.addColorStop(0, shade(coat, dark));
        g.addColorStop(Math.max(0.05, 1 - reach), shade(coat, dark * 0.95));
        g.addColorStop(1, shade(reach > 0 ? under : coat, dark));
        return g;
    };
    c.save();
    c.lineJoin = 'round';
    c.strokeStyle = 'rgba(20,16,12,0.55)';
    c.lineWidth = 2.2;
    // Back to front: the far legs (in shadow), the tail, the body and neck, the head, the near legs, the ears.
    // The outline of the whole wolf first, every part's fill on top: no seams where the parts meet.
    const order: Array<[Path2D, number]> = [...p.farLegs.map(l => [l, 0.72] as [Path2D, number]), [p.tail, 0.92], [p.torso, 1], [p.neck, 1],
        [p.head, 1], [p.muzzle, 1], ...p.nearLegs.map(l => [l, 1] as [Path2D, number]), [p.ears, 0.95]];
    c.lineWidth = 4.5;
    for (const [path] of order) c.stroke(path);
    for (const [path, dark] of order) {
        c.fillStyle = fill(dark);
        c.fill(path);
    }
    // A thin line around the head, so it reads as its own.
    c.lineWidth = 1.2;
    c.strokeStyle = 'rgba(20,16,12,0.3)';
    c.stroke(p.head);
    // The markings, each within its part.
    const rnd = seeded(JSON.stringify([a.species, a.sex, a.coat, a.baseColor, a.markings]));
    for (const m of markingsOf(a)) {
        const shape = maskShape(m.mask, p, rnd);
        if (!shape) continue;
        c.save();
        c.clip(shape.clip);
        c.globalAlpha = Math.max(0, Math.min(1, m.opacity));
        c.fillStyle = m.color;
        c.fill(shape.shape);
        c.restore();
    }
    // Inner ears, eye, nose; a greying muzzle with age.
    c.fillStyle = 'rgba(60,30,30,0.35)';
    c.save();
    c.clip(p.ears);
    c.fillRect(p.hx - p.hr, p.hy - p.hr * 1.6, p.hr * 1.2, p.hr);
    c.restore();
    if (stage === 'old') {
        c.save();
        c.clip(p.muzzle);
        c.globalAlpha = 0.45;
        c.fillStyle = '#d8d6cf';
        c.fill(p.muzzle);
        c.restore();
    }
    c.fillStyle = '#1a1410';
    c.beginPath();
    c.ellipse(p.eye.x, p.eye.y, p.eye.r * 1.25, p.eye.r, -0.15, 0, Math.PI * 2);
    c.fill();
    c.fillStyle = a.eyes || '#d9a441';
    c.beginPath();
    c.ellipse(p.eye.x + 1, p.eye.y, p.eye.r * 0.85, p.eye.r * 0.7, -0.15, 0, Math.PI * 2);
    c.fill();
    c.fillStyle = '#0d0a08';
    c.beginPath();
    c.arc(p.eye.x + 2, p.eye.y, p.eye.r * 0.35, 0, Math.PI * 2);
    c.fill();
    c.beginPath();
    c.ellipse(p.nose.x, p.nose.y, 7, 5.5, 0, 0, Math.PI * 2);
    c.fill();
    // A soft shadow on the ground.
    c.globalCompositeOperation = 'destination-over';
    c.fillStyle = 'rgba(0,0,0,0.28)';
    c.beginPath();
    c.ellipse((p.xs + p.xh) / 2, 316, (p.xs - p.xh) * 0.75, 8, 0, 0, Math.PI * 2);
    c.fill();
    c.restore();
}
