// The little doll on a wolf's card (doc 35), in a fight and in the In Sight list: a wolf in outline, armour shaded where
// it is worn, a blade at the muzzle; pointing at it lists them at once (no waiting for the browser's tooltip).
import type {GearView} from '../../game/battle.ts';
import {el, setClass} from './dom.ts';

const GearPlaces: Record<string, string> = {mouth: 'Muzzle', head: 'Head', neck: 'Neck', body: 'Body', back: 'Back', harness: 'Harness',
    chest_left: 'Chest, left', chest_right: 'Chest, right', paws: 'Paws'};
const WolfOutline = 'M38 62 C22 70 14 92 20 104 C28 96 36 84 52 76 C70 66 96 58 140 58 C170 58 192 54 206 46 L214 26 L222 40 L230 24 ' +
    'L236 44 C248 48 262 56 274 62 L276 70 C262 72 248 72 238 74 C232 86 224 96 214 102 L214 140 L204 140 L202 106 C196 108 188 110 180 110 ' +
    'L178 140 L168 140 L168 108 C140 112 110 112 88 106 L84 140 L74 140 L72 100 C66 96 62 92 60 86 L56 140 L46 140 L48 84 C46 76 44 70 38 62 Z';
let tip: HTMLElement | null = null;

/** A place for the doll, its hover list wired up; fill it with drawGear. */
export function gearDoll(parent: HTMLElement, className: string): HTMLElement {
    const doll = el('div', `gear-doll ${className}`, parent);
    doll.addEventListener('mouseenter', () => showTip(doll));
    doll.addEventListener('mouseleave', hideGearTip);
    return doll;
}

export function hideGearTip() {
    if (tip) tip.style.display = 'none';
}

/** Draws the doll for what they have on; 'bare' (and its list says so) when nothing. */
export function drawGear(parent: HTMLElement, gear: GearView[]) {
    parent.replaceChildren();
    const ns = 'http://www.w3.org/2000/svg';
    const svg = document.createElementNS(ns, 'svg');
    svg.setAttribute('viewBox', '0 0 300 150');
    parent.appendChild(svg);
    const add = (tag: string, attrs: Record<string, string | number>, cls: string) => {
        const e = document.createElementNS(ns, tag);
        for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, String(v));
        e.setAttribute('class', cls);
        svg.appendChild(e);
    };
    add('path', {d: WolfOutline}, 'gear-wolf');
    const on = (place: string) => gear.find(g => g.place === place);
    const kind = (g: GearView | undefined) => (g?.weapon ? 'gear-weapon' : 'gear-armour');
    if (on('body')) add('ellipse', {cx: 130, cy: 84, rx: 72, ry: 22}, kind(on('body')));
    if (on('back')) add('ellipse', {cx: 140, cy: 62, rx: 62, ry: 8}, kind(on('back')));
    if (on('neck')) add('ellipse', {cx: 216, cy: 74, rx: 13, ry: 19}, kind(on('neck')));
    if (on('head')) add('circle', {cx: 238, cy: 52, r: 17}, kind(on('head')));
    if (on('paws')) for (const x of [46, 74, 168, 204]) add('rect', {x, y: 124, width: 10, height: 16, rx: 2}, kind(on('paws')));
    if (on('mouth')) add('path', {d: 'M266 70 L298 54 L300 60 L272 76 Z'}, 'gear-weapon');
    const lines = gear.map(g => `${GearPlaces[g.place] ?? g.place}: ${g.name}${g.protect > 0 ? ` (protection ${g.protect})` : ''}`);
    parent.dataset.tip = lines.length ? `ARMOUR & WEAPONS\n${lines.join('\n')}` : 'No armour, no weapon.';
    setClass(parent, 'bare', !gear.length);
}

function showTip(target: HTMLElement) {
    if (!tip) tip = el('div', 'gear-tip', document.body);
    tip.textContent = target.dataset.tip ?? '';
    const r = target.getBoundingClientRect();
    tip.style.display = 'block';
    const w = tip.offsetWidth, h = tip.offsetHeight;
    tip.style.left = `${Math.max(8, Math.min(window.innerWidth - w - 8, r.left))}px`;
    tip.style.top = `${r.bottom + 6 + h > window.innerHeight ? r.top - h - 6 : r.bottom + 6}px`;
}
