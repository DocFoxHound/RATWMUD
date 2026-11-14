// What a wolf wears and wields at a glance (doc 35; the user's notes, 2026-11-14), on its card in a fight and in the In
// Sight list: a small picture for each piece (a straw hat, a steel helm, mail, a sword, a rapier, a pitchfork...: hudIcons.ts),
// none when bare; pointing at them lists them at once (no waiting for the browser's tooltip).
import type {GearView} from '../../game/battle.ts';
import {el, setClass} from './dom.ts';
import {GearIcons, gearIconFor} from './hudIcons.ts';

const GearPlaces: Record<string, string> = {mouth: 'Muzzle', head: 'Head', neck: 'Neck', body: 'Body', back: 'Back', harness: 'Harness',
    chest_left: 'Chest, left', chest_right: 'Chest, right', paws: 'Paws'};
/** Head to tail, then what is held: the order the pictures read in. */
const Order = ['head', 'neck', 'body', 'back', 'harness', 'chest_left', 'chest_right', 'paws', 'mouth'];
let tip: HTMLElement | null = null;

/** A place for the pictures, its hover list wired up; fill it with drawGear. */
export function gearDoll(parent: HTMLElement, className: string): HTMLElement {
    const doll = el('div', `gear-doll ${className}`, parent);
    doll.addEventListener('mouseenter', () => showTip(doll));
    doll.addEventListener('mouseleave', hideGearTip);
    return doll;
}

export function hideGearTip() {
    if (tip) tip.style.display = 'none';
}

/** The pictures for what they have on, in order; nothing (and 'bare', its list saying so) when they have nothing. */
export function drawGear(parent: HTMLElement, gear: GearView[]) {
    parent.replaceChildren();
    const sorted = [...gear].sort((a, b) => rank(a.place) - rank(b.place));
    for (const g of sorted) {
        const icon = gearIconFor(g.place, g.name, g.weapon);
        const span = el('span', `gear-icon ${icon}`, parent);
        span.innerHTML = GearIcons[icon];
        span.dataset.icon = icon;
    }
    const lines = sorted.map(g => `${GearPlaces[g.place] ?? g.place}: ${g.name}${g.protect > 0 ? ` (protection ${g.protect})` : ''}`);
    parent.dataset.tip = lines.length ? `ARMOUR & WEAPONS\n${lines.join('\n')}` : 'No armour, no weapon.';
    setClass(parent, 'bare', !gear.length);
}

const rank = (place: string) => (Order.indexOf(place) + 1 || Order.length + 1);

function showTip(target: HTMLElement) {
    if (target.classList.contains('bare')) return;
    if (!tip) tip = el('div', 'gear-tip', document.body);
    tip.textContent = target.dataset.tip ?? '';
    const r = target.getBoundingClientRect();
    tip.style.display = 'block';
    const w = tip.offsetWidth, h = tip.offsetHeight;
    tip.style.left = `${Math.max(8, Math.min(window.innerWidth - w - 8, r.left))}px`;
    tip.style.top = `${r.bottom + 6 + h > window.innerHeight ? r.top - h - 6 : r.bottom + 6}px`;
}
