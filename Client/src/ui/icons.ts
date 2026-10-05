// Small line icons for the fight screen (Docs/Design/37-combat-feel.md): drawn in the text's own colour, 24 units
// square, so a control can be read from its shape before its word.
const Paths: Record<string, string> = {
    // Two fangs.
    bite: 'M4 5h16M6 5l3 9 3-9M12 5l3 9 3-9',
    // A blade, its guard and grip.
    sword: 'M19 3l-9 9M19 3v4M19 3h-4M8 12l4 4M6 18l3-3M5 19l1 1',
    // A flame.
    fire: 'M12 21c-4 0-6-3-6-6 0-4 3-5 4-9 1 2 2 3 2 5 1-1 2-2 2-4 3 2 4 5 4 8 0 3-2 6-6 6zM12 21c-1.5 0-2.5-1-2.5-2.5S11 16 12 14c1 2 2.5 3 2.5 4.5S13.5 21 12 21z',
    // A cross: tending wounds.
    tend: 'M10 4h4v6h6v4h-6v6h-4v-6H4v-4h6z',
    // Turning over: rolling the flames out.
    roll: 'M20 12a8 8 0 1 1-3-6.2M17 3v3h3M9 13c1-2 2-3 3-5 1 2 2 3 2 5a2.5 2.5 0 0 1-5 0z',
    // Down to the ground: picking up.
    pickup: 'M12 3v11M7 10l5 5 5-5M5 20h14',
    // A flag lowered: yielding.
    yield: 'M6 21V11M6 11c3-1.5 6 1.5 9 0s3 0 3 0v6s-1-1-3 0-6-1.5-9 0M4 21h8',
    // A white flag.
    truce: 'M6 21V4M6 4c3-2 6 2 9 0s3 0 3 0v8s-1-1-3 0-6-2-9 0',
    // Out through a gap.
    flee: 'M14 4h5v16h-5M10 8l-4 4 4 4M6 12h10',
    // Turning left and right.
    left: 'M8 6H4V2M4 6a9 9 0 1 1-1 7',
    right: 'M16 6h4V2M20 6a9 9 0 1 0 1 7',
    // Done: on to the next.
    end: 'M5 5l7 7-7 7M12 5l7 7-7 7',
    // A crescent moon: resting a turn.
    rest: 'M20 14.5A8 8 0 1 1 9.5 4a6.5 6.5 0 0 0 10.5 10.5z',
    // Getting up.
    rise: 'M12 20V7M7 12l5-5 5 5M5 4h14',
    // Watching.
    watch: 'M2 12s4-7 10-7 10 7 10 7-4 7-10 7S2 12 2 12zM12 15a3 3 0 1 0 0-6 3 3 0 0 0 0 6z',
    // Away: asleep at the wheel.
    away: 'M20 14A8 8 0 1 1 10 4a6 6 0 0 0 10 10z',
    // Down: a fallen wolf.
    down: 'M4 4l16 16M20 4L4 20',
    // Agreed.
    yes: 'M4 12l5 5L20 6',
    // Refused.
    no: 'M6 6l12 12M18 6L6 18',
    // A missed blow.
    miss: 'M12 4a8 8 0 1 0 0 16 8 8 0 0 0 0-16z',
    // Time passing.
    wait: 'M12 7v5l3 3M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18z',
    // Guard (a shield) and Shove (a push against a bar), doc 37.
    guard: 'M12 3l7 3v5c0 5-3 8-7 10-4-2-7-5-7-10V6z',
    shove: 'M3 12h11M10 7l5 5-5 5M19 4v16',
    // Aiming for a hit zone (doc 40): a target.
    aim: 'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18zM12 7a5 5 0 1 0 0 10 5 5 0 0 0 0-10zM12 11a1 1 0 1 0 0 2 1 1 0 0 0 0-2z',
    // Stalking (doc 40): a pawprint, low.
    stalk: 'M7 16c0-2 2.5-4 5-4s5 2 5 4-2 3-5 3-5-1-5-3zM5 10a1.5 2 0 1 0 3 0 1.5 2 0 1 0-3 0M9.5 7a1.5 2 0 1 0 3 0 1.5 2 0 1 0-3 0M14.5 7a1.5 2 0 1 0 3 0 1.5 2 0 1 0-3 0M18 10a1.5 2 0 1 0 3 0 1.5 2 0 1 0-3 0',
    // Armour worn (doc 35, Part 8): a breastplate.
    armour: 'M7 4l5 2 5-2 3 4-3 2v7l-5 3-5-3v-7L4 8zM12 6v15',
    // Joining, starting.
    start: 'M12 3l2.5 6.5L21 12l-6.5 2.5L12 21l-2.5-6.5L3 12l6.5-2.5z',
    // The Gift families (doc 43). Earth: a mountain on its ground.
    earth: 'M3 19l6-10 3 5 3-3 6 8zM2 21h20',
    // Water: two waves.
    water: 'M3 10c2-2 4-2 6 0s4 2 6 0 4-2 6 0M3 16c2-2 4-2 6 0s4 2 6 0 4-2 6 0',
    // Wind: three gusts curling.
    wind: 'M3 8h11a3 3 0 1 0-3-3M3 13h15a3 3 0 1 1-3 3M3 18h7',
    // Sound: a mouth's arcs.
    sound: 'M5 9v6h3l4 4V5L8 9zM15 9a4 4 0 0 1 0 6M18 6a8 8 0 0 1 0 12',
    // Blinker: a step, and where it lands.
    blinker: 'M4 18l4-4M8 14l1 3M12 7l2-4 2 4 4 2-4 2-2 4-2-4-4-2z',
    // Gravity: a weight pulled down.
    gravity: 'M8 7h8l2 12H6zM10 7a2 2 0 1 1 4 0M12 21v1',
    // Seer: an eye.
    seer: 'M2 12c3-5 7-7 10-7s7 2 10 7c-3 5-7 7-10 7s-7-2-10-7zM12 9a3 3 0 1 1 0 6 3 3 0 0 1 0-6z',
};

/** An icon as an element, sized by the CSS (1em by default) and coloured by `currentColor`. */
export function icon(name: string, className = 'icon'): SVGSVGElement {
    const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    svg.setAttribute('viewBox', '0 0 24 24');
    svg.setAttribute('class', className === 'icon' ? 'icon' : `icon ${className}`);
    svg.setAttribute('aria-hidden', 'true');
    const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    path.setAttribute('d', Paths[name] ?? Paths.start);
    svg.append(path);
    return svg;
}

export const IconNames = Object.keys(Paths);
