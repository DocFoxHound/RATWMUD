// The game screen's small pictures (the user's notes, 2026-11-14): the time of day and the weather in the "Your
// surroundings" bar, the three senses by how well they work here, what a wolf wears and wields on its card, and the
// navigation buttons. Flat inline SVGs on a 24-unit square, readable at 18-24 px on the dark panels; each has its words
// in a tooltip where it is shown. Pure strings and choices, so the tests can check them without a page.

const svg = (body: string, cls = '') => `<svg viewBox="0 0 24 24" aria-hidden="true"${cls ? ` class="${cls}"` : ''}>${body}</svg>`;

// The palette: the game's (styles.css) and a few of the world's own colours.
const Sun = '#f2c25b', Dawn = '#f6d98a', Late = '#e8894a', Moon = '#e9e1c6', Sky = '#cfe3ef';
const Cloud = '#c9d1d3', CloudDark = '#8d989c', Rain = '#7fb0d8', Snow = '#eef3f5', Bolt = '#f2c25b', Sand = '#c9a66b';
const Steel = '#b9c3c6', SteelDark = '#76848a', Leather = '#a8754a', LeatherDark = '#6f4b2e', Straw = '#e0c27a', StrawDark = '#a8853f';
const Wood = '#94714a', Cloth = '#8f9a86', Felt = '#9b6b5a', Gold = '#d9b67b';

// A cloud, its base along y = 15.5 (moved with a transform where a picture wants it elsewhere).
const cloud = (fill: string, at = '') => `<path${at ? ` transform="${at}"` : ''} fill="${fill}" d="M6.5 15.5h11a3.5 3.5 0 0 0 0-7 5 5 0 0 0-9.6-1A4 4 0 0 0 6.5 15.5z"/>`;
const rays = (cx: number, cy: number, r1: number, r2: number, colour: string, from = 0, to = 360, step = 45) => {
    let d = '';
    for (let a = from; a <= to; a += step) {
        const t = (a * Math.PI) / 180, c = Math.cos(t), s = Math.sin(t);
        d += `M${(cx + c * r1).toFixed(1)} ${(cy + s * r1).toFixed(1)}L${(cx + c * r2).toFixed(1)} ${(cy + s * r2).toFixed(1)}`;
    }
    return `<path d="${d}" stroke="${colour}" stroke-width="1.8" stroke-linecap="round" fill="none"/>`;
};

// ------------------------------------------------------------------ The time of day

export const TimeIcons = {
    morning: svg(`<path d="M2.5 17.5h19" stroke="${Dawn}" stroke-width="1.6" stroke-linecap="round"/>` +
        `<path fill="${Dawn}" d="M7 16.5a5 5 0 0 1 10 0z"/>${rays(12, 16.5, 7, 9.4, Dawn, 200, 340, 35)}` +
        `<path d="M8 20.5h8" stroke="${Dawn}" stroke-width="1.4" stroke-linecap="round" opacity=".6"/>`),
    sun: svg(`<circle cx="12" cy="12" r="4.6" fill="${Sun}"/>${rays(12, 12, 7, 9.8, Sun)}`),
    late: svg(`<path d="M2.5 16.5h19" stroke="${Late}" stroke-width="1.6" stroke-linecap="round"/>` +
        `<path fill="${Late}" d="M6.5 15.5a5.5 5.5 0 0 1 11 0z"/>${rays(12, 15.5, 7.4, 9.6, Late, 215, 325, 36)}` +
        `<path d="M6 19h12M9 21.5h6" stroke="${Late}" stroke-width="1.4" stroke-linecap="round" opacity=".55"/>`),
    moon: svg(`<path fill="${Moon}" d="M15.5 3.6A8.6 8.6 0 1 0 20.4 17 7 7 0 0 1 15.5 3.6z"/>` +
        `<path fill="${Moon}" d="M18.5 5.5l.6 1.4 1.4.6-1.4.6-.6 1.4-.6-1.4-1.4-.6 1.4-.6z"/>`),
} as const;
export type TimeOfDay = keyof typeof TimeIcons;

/** The time of day as a picture and its words: the morning sun, the sun, the late sun, or the moon. */
export function timeOfDay(hour: number, phase: string): {icon: TimeOfDay; words: string} {
    if (phase === 'night') return {icon: 'moon', words: hour >= 19 && hour < 22 ? 'Evening, after dark' : hour >= 22 || hour < 2 ? 'Night' : 'The small hours'};
    if (phase === 'dawn') return {icon: 'morning', words: 'Dawn'};
    if (phase === 'dusk') return {icon: 'late', words: 'Dusk'};
    if (hour < 10) return {icon: 'morning', words: 'Morning'};
    if (hour < 15) return {icon: 'sun', words: hour < 12 ? 'Late morning' : hour < 13 ? 'Midday' : 'Early afternoon'};
    return {icon: 'late', words: 'Late afternoon'};
}

// ------------------------------------------------------------------ The weather

const drops = (n: number) => {
    const xs = n >= 5 ? [6.5, 9.5, 12.5, 15.5, 18.5] : [8, 12, 16];
    return xs.map((x, i) => `<path d="M${x} ${17.5 + (i % 2)}l-1.4 3" stroke="${Rain}" stroke-width="1.7" stroke-linecap="round"/>`).join('');
};
export const WeatherIcons = {
    clear: svg(`<path fill="${Sky}" d="M12 3.5l1.6 6.9 6.9 1.6-6.9 1.6L12 20.5l-1.6-6.9L3.5 12l6.9-1.6z"/>` +
        `<path fill="${Sky}" opacity=".6" d="M19 3.5l.5 1.9 1.9.6-1.9.5-.5 1.9-.6-1.9-1.9-.5 1.9-.6z"/>`),
    thinCloud: svg(cloud(Cloud, 'translate(0 2)')),
    overcast: svg(cloud(CloudDark, 'translate(3 -2.5) scale(.85)') + cloud(Cloud, 'translate(-1 3)')),
    rain: svg(cloud(Cloud, 'translate(0 -1)') + drops(3)),
    heavyRain: svg(cloud(CloudDark, 'translate(0 -1)') + drops(5)),
    storm: svg(cloud(CloudDark, 'translate(0 -2)') + `<path fill="${Bolt}" d="M12.6 12.5l-3.4 5.2h2.6l-1.5 4.6 4.6-6.3h-2.7l1.6-3.5z"/>`),
    fog: svg(`<path d="M3.5 8c2.2-1.3 4.3-1.3 6.5 0s4.3 1.3 6.5 0 2.7-.8 4 0M3.5 12.5c2.2-1.3 4.3-1.3 6.5 0s4.3 1.3 6.5 0 2.7-.8 4 0M3.5 17c2.2-1.3 4.3-1.3 6.5 0s4.3 1.3 6.5 0 2.7-.8 4 0" ` +
        `stroke="${Cloud}" stroke-width="1.9" stroke-linecap="round" fill="none"/>`),
    snow: svg(cloud(Cloud, 'translate(0 -1.5)') + [7.5, 12, 16.5].map((x, i) => {
        const y = 18.6 + (i % 2) * 1.6;
        return `<path d="M${x} ${y - 1.9}v3.8M${x - 1.65} ${y - 0.95}l3.3 1.9M${x - 1.65} ${y + 0.95}l3.3-1.9" stroke="${Snow}" stroke-width="1.2" stroke-linecap="round"/>`;
    }).join('')),
    sandstorm: svg(`<path d="M3 8h11a3 3 0 1 0-3-3M3 12.5h15a3 3 0 1 1-3 3M3 17h8" stroke="${Sand}" stroke-width="1.9" stroke-linecap="round" fill="none"/>`),
    sheltered: svg(`<path fill="${Wood}" d="M12 3.5l9 7.3-1.2 1.4L12 6 4.2 12.2 3 10.8z"/>` +
        `<path fill="${CloudDark}" d="M6 11.6L12 7l6 4.6V20H6z"/><path fill="#3b2b1c" d="M10.3 14.5h3.4V20h-3.4z"/>`),
} as const;
export type WeatherIcon = keyof typeof WeatherIcons;

const Weather: Record<string, string> = {clear: 'Clear skies', overcast: 'Overcast', rain: 'Rain', storm: 'A storm', fog: 'Fog', snow: 'Snow', sandstorm: 'A sandstorm'};

/** The weather where the wolf stands as a picture and its words ("Light rain", "Thin cloud"); indoors, sheltered. */
export function weatherIcon(weather: string, intensity: number, outdoors: boolean): {icon: WeatherIcon; words: string} {
    if (!outdoors) return {icon: 'sheltered', words: 'Sheltered: under a roof'};
    const strength = intensity < 0.35 ? 'Light ' : intensity > 0.75 ? 'Heavy ' : '';
    switch (weather) {
        case 'overcast': return intensity < 0.45 ? {icon: 'thinCloud', words: 'Thin cloud'} : {icon: 'overcast', words: 'Overcast'};
        case 'rain': return {icon: intensity > 0.75 ? 'heavyRain' : 'rain', words: `${strength}rain`.replace(/^./, c => c.toUpperCase())};
        case 'storm': return {icon: 'storm', words: 'A storm'};
        case 'fog': return {icon: 'fog', words: intensity < 0.35 ? 'Mist' : intensity > 0.75 ? 'Thick fog' : 'Fog'};
        case 'snow': return {icon: 'snow', words: `${strength}snow`.replace(/^./, c => c.toUpperCase())};
        case 'sandstorm': return {icon: 'sandstorm', words: 'A sandstorm'};
        default: return {icon: 'clear', words: Weather[weather] ?? 'Clear skies'};
    }
}

// ------------------------------------------------------------------ The senses

const ink = '#ded5c3';
const eye = {
    open: `<path d="M2.5 12S6.3 5.8 12 5.8 21.5 12 21.5 12 17.7 18.2 12 18.2 2.5 12 2.5 12z" fill="none" stroke="${ink}" stroke-width="1.6"/>` +
        `<circle cx="12" cy="12" r="3.4" fill="${ink}"/><circle cx="13.2" cy="10.8" r="1" fill="#182122"/>`,
    half: `<path d="M2.5 12.5s3.8 5.7 9.5 5.7 9.5-5.7 9.5-5.7" fill="none" stroke="${ink}" stroke-width="1.6"/>` +
        `<path d="M3 11.6c2.8-1.6 5.8-2.3 9-2.3s6.2.7 9 2.3" fill="none" stroke="${ink}" stroke-width="2.2" stroke-linecap="round"/>` +
        `<path d="M8.8 10.9a3.2 3.2 0 0 0 6.4 0z" fill="${ink}"/>`,
    closed: `<path d="M3 11c2.9 3.6 5.9 5.2 9 5.2s6.1-1.6 9-5.2" fill="none" stroke="${ink}" stroke-width="1.8" stroke-linecap="round"/>` +
        `<path d="M6 14.8l-1.3 2M10 16.1l-.5 2.3M14 16.1l.5 2.3M18 14.8l1.3 2" stroke="${ink}" stroke-width="1.4" stroke-linecap="round"/>`,
};
const ear = (waves: number) => `<path d="M5 20.5l2.6-15.8c.2-1 1.4-1.2 1.9-.4l6 9.6c.4.7.1 1.5-.6 1.8z" fill="${ink}"/>` +
    `<path d="M8.4 9.5l3 4.9-3.6 1.6z" fill="#182122" opacity=".55"/>` +
    (waves >= 1 ? `<path d="M17.4 9.2a4.5 4.5 0 0 1 0 6" fill="none" stroke="${ink}" stroke-width="1.6" stroke-linecap="round"/>` : '') +
    (waves >= 2 ? `<path d="M19.6 6.9a8 8 0 0 1 0 10.6" fill="none" stroke="${ink}" stroke-width="1.6" stroke-linecap="round"/>` : '') +
    (waves === 0 ? `<path d="M15.5 8.5l5 7M20.5 8.5l-5 7" stroke="${ink}" stroke-width="1.5" stroke-linecap="round" opacity=".8"/>` : '');
const nose = (lines: number) => `<path d="M3.5 13.2c0-2.4 2.6-3.7 5.5-3.7s5.5 1.3 5.5 3.7c0 2.2-2.9 4.8-5.5 4.8s-5.5-2.6-5.5-4.8z" fill="${ink}"/>` +
    `<ellipse cx="7" cy="13.4" rx="1.1" ry=".8" fill="#182122"/><ellipse cx="11" cy="13.4" rx="1.1" ry=".8" fill="#182122"/>` +
    [7.5, 11.5, 15.5].slice(0, lines).map((y, i) => `<path d="M16 ${y}c1.2-1 2.3-1 3.4 0s1.6 1 2.1.4" fill="none" stroke="${ink}" stroke-width="1.5" stroke-linecap="round" opacity="${1 - i * 0.15}"/>`).join('') +
    (lines === 0 ? `<path d="M16.5 9l4.5 6M21 9l-4.5 6" stroke="${ink}" stroke-width="1.5" stroke-linecap="round" opacity=".8"/>` : '');

export type Sense = 'sight' | 'hearing' | 'scent';
export type SenseLevel = 'keen' | 'dulled' | 'poor';

export const SenseIcons: Record<Sense, Record<SenseLevel, string>> = {
    sight: {keen: svg(eye.open), dulled: svg(eye.half), poor: svg(eye.closed)},
    hearing: {keen: svg(ear(2)), dulled: svg(ear(1)), poor: svg(ear(0))},
    scent: {keen: svg(nose(3)), dulled: svg(nose(1)), poor: svg(nose(0))},
};

const SenseWords: Record<Sense, Record<SenseLevel, string>> = {
    sight: {keen: 'clear', dulled: 'dimmed', poor: 'very poor'},
    hearing: {keen: 'sharp', dulled: 'muffled', poor: 'very poor'},
    scent: {keen: 'strong', dulled: 'faint', poor: 'very poor'},
};

/** How well a sense works here (1 is as on a clear day; the server's multiplier, 0..4), as a level and its words. */
export function senseLevel(sense: Sense, value: number): {level: SenseLevel; words: string} {
    const level: SenseLevel = value >= 0.75 ? 'keen' : value >= 0.4 ? 'dulled' : 'poor';
    const name = sense === 'sight' ? 'Sight' : sense === 'hearing' ? 'Hearing' : 'Scent';
    return {level, words: `${name}: ${SenseWords[sense][level]} (${Math.round(value * 100)}%)`};
}

// ------------------------------------------------------------------ What a wolf wears and wields

const helmDome = (fill: string, rim: string) => `<path fill="${fill}" d="M5 15a7 7 0 0 1 14 0z"/><path fill="${rim}" d="M3 15h18v2.2H3z"/>`;
export const GearIcons = {
    strawHat: svg(`<ellipse cx="12" cy="16" rx="10" ry="3.4" fill="${Straw}"/><path fill="${Straw}" d="M7.2 15.5c0-4 2-7 4.8-7s4.8 3 4.8 7z"/>` +
        `<path fill="${StrawDark}" d="M7.3 13.4h9.4l.2 2h-9.8z"/><path d="M4.5 16.6c2.2.9 4.7 1.3 7.5 1.3s5.3-.4 7.5-1.3" stroke="${StrawDark}" stroke-width=".8" fill="none"/>`),
    hat: svg(`<ellipse cx="12" cy="16.5" rx="8" ry="2.6" fill="${Felt}"/><path fill="${Felt}" d="M7.6 16c0-4.5 1.6-8.5 4.4-8.5s4.4 4 4.4 8.5z"/>` +
        `<path fill="#6d4639" d="M7.7 13.6h8.6l.1 1.9H7.6z"/><path d="M15.5 9.5c2-1.8 3.8-2.6 5-2.4" stroke="${Gold}" stroke-width="1.3" stroke-linecap="round" fill="none"/>`),
    hood: svg(`<path fill="${Cloth}" d="M12 3.5c-4.5 0-7.5 4-7.5 9 0 3.5 1 6.2 2.2 8h10.6c1.2-1.8 2.2-4.5 2.2-8 0-5-3-9-7.5-9z"/>` +
        `<path fill="#4c5547" d="M12 7.5c-2.7 0-4.4 2.6-4.4 6 0 2.6.8 4.8 1.6 7h5.6c.8-2.2 1.6-4.4 1.6-7 0-3.4-1.7-6-4.4-6z"/>`),
    leatherHelm: svg(helmDome(Leather, LeatherDark) + `<path fill="${LeatherDark}" d="M8 9.5l1.8 5.5H7.4zM16 9.5l.6 5.5h-2.4z"/>` +
        `<circle cx="12" cy="11" r=".9" fill="${Gold}"/><circle cx="5.5" cy="16.1" r=".7" fill="${Gold}"/><circle cx="18.5" cy="16.1" r=".7" fill="${Gold}"/>`),
    steelHelm: svg(`<ellipse cx="12" cy="16" rx="10" ry="2.8" fill="${SteelDark}"/><path fill="${Steel}" d="M5.6 16a6.4 7.5 0 0 1 12.8 0z"/>` +
        `<path d="M12 8.6V16" stroke="${SteelDark}" stroke-width="1.2"/><path d="M8 11.5c1-1.8 2.3-2.6 3.3-2.8" stroke="#eef3f5" stroke-width="1" stroke-linecap="round" fill="none" opacity=".8"/>`),
    leatherArmour: svg(`<path fill="${Leather}" d="M8 3.5l4 2.2 4-2.2 4 3-1.6 4.2V20.5H5.6V10.7L4 6.5z"/>` +
        `<path d="M12 5.7v14.8M5.6 12h12.8M5.6 16.2h12.8" stroke="${LeatherDark}" stroke-width="1" stroke-dasharray="1.6 1.2" fill="none"/>`),
    mail: svg(`<path fill="${SteelDark}" d="M8 3.5l4 2.2 4-2.2 4 3-1.6 4.2V20.5H5.6V10.7L4 6.5z"/>` +
        `<path d="M7 9.5h10M7 12.5h10M7 15.5h10M7 18.5h10" stroke="${Steel}" stroke-width="1.4" stroke-dasharray="1.4 1" fill="none"/>`),
    collar: svg(`<ellipse cx="12" cy="12" rx="8.5" ry="5.6" fill="none" stroke="${Leather}" stroke-width="3.6"/>` +
        [5, 9.2, 14.8, 19].map((x, i) => `<circle cx="${x}" cy="${i === 0 || i === 3 ? 12 : 16.7}" r="1.1" fill="${Steel}"/>`).join('') +
        `<path d="M10.5 6.4h3" stroke="${Gold}" stroke-width="2.2"/>`),
    greaves: svg(`<path fill="${Steel}" d="M8 3.5h6v10.5l5.5 3.2v3.3H8z"/><path d="M8 7.3h6M8 10.6h6" stroke="${SteelDark}" stroke-width="1.2"/>` +
        `<path fill="${LeatherDark}" d="M8 18.4h11.5v2.1H8z"/>`),
    claws: svg([5.5, 11, 16.5].map(x => `<path fill="${Steel}" d="M${x} 20c0-6 1.4-11 4.6-15.5-.9 4.6-.8 9.6.4 15.5z"/>`).join('')),
    sword: svg(`<path fill="${Steel}" d="M19.8 3.2l1 1-11.6 11.6-1.9-.1-.1-1.9z"/><path d="M15.5 4.5l4 4" stroke="#eef3f5" stroke-width=".7" opacity=".7"/>` +
        `<path fill="${Gold}" d="M5.2 13.8l1.4-1.4 5 5-1.4 1.4z"/><path fill="${LeatherDark}" d="M7.2 16.8l1.3 1.3-3 3-1.3-1.3z"/><circle cx="4" cy="20.6" r="1.3" fill="${Gold}"/>`),
    rapier: svg(`<path d="M8.6 15.4L21 3" stroke="${Steel}" stroke-width="1.5" stroke-linecap="round"/>` +
        `<path d="M10.8 17.4a4 4 0 0 1-4.2-4.2" stroke="${Gold}" stroke-width="1.6" fill="none" stroke-linecap="round"/>` +
        `<path d="M5.5 11.8l6.7 6.7" stroke="${Gold}" stroke-width="1.4" stroke-linecap="round"/><path d="M7.6 16.4l-3.4 3.4" stroke="${LeatherDark}" stroke-width="2.4" stroke-linecap="round"/>`),
    knife: svg(`<path fill="${Steel}" d="M20.5 3.5c-1.3 4.2-4.3 8-8.2 10.6l-2.4-2.4c2.6-3.9 6.4-6.9 10.6-8.2z"/>` +
        `<path fill="${Wood}" d="M9.4 12.2l2.4 2.4-5.6 5.6c-.7.7-1.7.7-2.4 0s-.7-1.7 0-2.4z"/>`),
    pitchfork: svg(`<path d="M3.5 20.5L14.5 9.5" stroke="${Wood}" stroke-width="2" stroke-linecap="round"/>` +
        `<path d="M12.3 7.3l4.4 4.4M14.5 9.5l5.8-5.8M12.3 7.3l5.4-5.4M16.7 11.7l5.4-5.4" stroke="${Steel}" stroke-width="1.5" stroke-linecap="round"/>`),
    club: svg(`<path fill="${Wood}" d="M17.7 2.8c2.3 0 3.5 1.2 3.5 3.5 0 2-1.6 3.5-3.6 5.1l-9.7 9.3-2.6-2.6 9.3-9.7c1.6-2 3.1-5.6 3.1-5.6z"/>` +
        `<circle cx="17.5" cy="6.4" r=".9" fill="${LeatherDark}"/><circle cx="14.6" cy="9.6" r=".8" fill="${LeatherDark}"/>`),
    staff: svg(`<path d="M5 20.5L18 4" stroke="${Wood}" stroke-width="2.2" stroke-linecap="round"/><circle cx="18.5" cy="3.8" r="1.8" fill="${Wood}"/>` +
        `<path d="M10.5 13.6l1.6 1.2" stroke="${LeatherDark}" stroke-width="2.6"/>`),
    shawl: svg(`<path fill="${Felt}" d="M3 6.5c3 1.5 6 2.2 9 2.2s6-.7 9-2.2l-3.5 12-5.5-4-5.5 4z"/>`),
    harness: svg(`<path d="M4 7c4 3 12 3 16 0M4 17c4-3 12-3 16 0M8 8.6v6.8M16 8.6v6.8" stroke="${Leather}" stroke-width="2.2" fill="none" stroke-linecap="round"/>` +
        `<rect x="10" y="10" width="4" height="4" rx=".8" fill="${Steel}"/>`),
} as const;
export type GearIcon = keyof typeof GearIcons;

/** The little picture for one thing a wolf has on, from where it is and its name (the server sends both). */
export function gearIconFor(place: string, name: string, weapon: boolean): GearIcon {
    const n = name.toLowerCase();
    if (place === 'mouth' || (weapon && place !== 'paws')) {
        if (/rapier|needle/.test(n)) return 'rapier';
        if (/fork|pitch/.test(n)) return 'pitchfork';
        if (/knife|cleaver|dagger|hook/.test(n)) return 'knife';
        if (/cudgel|cosh|flail|club|pick|mace|hammer/.test(n)) return 'club';
        if (/fang|claw/.test(n)) return 'claws';
        if (/staff|tool|hoe|rake|spade|shovel|crook|pole/.test(n)) return 'staff';
        return weapon ? 'sword' : 'staff';
    }
    if (place === 'head') {
        if (/straw|sun hat/.test(n)) return 'strawHat';
        if (/kettle|helm|chamfron|steel|iron/.test(n)) return 'steelHelm';
        if (/leather/.test(n)) return 'leatherHelm';
        if (/hood|coif|veil/.test(n)) return 'hood';
        return 'hat';
    }
    if (place === 'neck' || place === 'throat') return 'collar';
    if (place === 'paws') return weapon || /claw/.test(n) ? 'claws' : 'greaves';
    if (place === 'back' || place === 'shoulders') return /mail|steel/.test(n) ? 'mail' : 'shawl';
    if (place === 'harness') return 'harness';
    return /mail|brigandine|steel|iron|chain|plate/.test(n) ? 'mail' : 'leatherArmour';
}

// ------------------------------------------------------------------ The navigation and the help

const line = (d: string) => svg(`<path d="${d}" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"/>`);
export const NavIcons = {
    character: line('M5 3.5l3.6 4.6h6.8L19 3.5l.6 8.3L12 20.5l-7.6-8.7zM9.4 12.4h.1M14.5 12.4h.1M10.6 16.4L12 17.6l1.4-1.2'),
    chapter: line('M6 21V3.5M6 4h11l-2.5 4L17 12H6'),
    inventory: line('M5 9h14l-1 11H6zM9 9V7a3 3 0 0 1 6 0v2M5 13h14'),
    journal: line('M5 4.5h10.5A2.5 2.5 0 0 1 18 7v13H7.5A2.5 2.5 0 0 1 5 17.5zM5 17.5A2.5 2.5 0 0 1 7.5 15H18M9 8.5h5'),
    friends: line('M3 19.5c0-3 2.2-5 5-5s5 2 5 5M5.5 9a2.5 2.5 0 1 0 5 0 2.5 2.5 0 1 0-5 0M14 14.8c3.6-.6 7 1.3 7 4.7M14.5 8.5a2.5 2.5 0 1 0 5 0 2.5 2.5 0 1 0-5 0'),
    settings: line('M12 8.8a3.2 3.2 0 1 0 0 6.4 3.2 3.2 0 1 0 0-6.4zM12 2.8v2.5M12 18.7v2.5M2.8 12h2.5M18.7 12h2.5M5.5 5.5l1.8 1.8M16.7 16.7l1.8 1.8M5.5 18.5l1.8-1.8M16.7 7.3l1.8-1.8'),
    storyteller: line('M20 3.5C13 5 8.5 10 7 17l-2 3.5M7 17c4.5-.5 8-3 10-7M12 9h5'),
    dev: line('M3.5 5.5h17v13h-17zM7 10l3 2.5L7 15M12 15h5'),
    help: line('M12 21a9 9 0 1 0 0-18 9 9 0 1 0 0 18zM9.5 9.5a2.6 2.6 0 0 1 5 .8c0 1.8-2.5 2.2-2.5 4M12 17.2v.1'),
} as const;
export type NavIcon = keyof typeof NavIcons;
