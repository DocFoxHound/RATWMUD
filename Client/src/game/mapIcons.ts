// Small flat icons for what a building is, over the middle of its footprint on the minimap and the World Map (the user,
// 2026-11-14: "For shops and stores and inns and all sorts of buildings let's also add a small icon that represents
// what they are over the center of their building on the minimap"). Each is an SVG path on a 16 x 16 grid, drawn in
// the paper's colour on a round badge in the kind's own colour, readable at 12-16 pixels.

export interface MapIcon {
    label: string;              // What it is, in words (the pointer's title).
    path: string;               // SVG path data, 16 x 16, filled (even-odd).
    badge: string;              // The badge's colour.
}

export const MapIcons: Record<string, MapIcon> = {
    inn: {label: 'inn or tavern', badge: '#9a6a3a',
        path: 'M3 5h8v8.2c0 .5-.4.8-.8.8H3.8c-.5 0-.8-.3-.8-.8z M11 6.5h2c.6 0 1 .4 1 1v3c0 .6-.4 1-1 1h-2V10h1.5V8H11z ' +
            'M3 4.2C3.6 2.9 5.1 2.6 6.2 3.3 7.2 2.5 8.9 2.6 11 3.8V5H3z'},
    bakery: {label: 'bakery', badge: '#a77b3f',
        path: 'M1.8 10.2C1.8 6.6 4.6 4 8 4s6.2 2.6 6.2 6.2V12H1.8z M5 6.6l1 3.6.9-.3-1-3.6z M7.6 6.2v3.8h1V6.2z M10 6.6l-1 3.6.9.3 1-3.6z'},
    butcher: {label: 'butcher', badge: '#8e3f3a',
        path: 'M2 3.5h8.5v6H2z M10.5 5.5h1.2v2h-1.2z M11.7 5.8H15v1.4h-3.3z M4 5.5a1 1 0 1 0 0 .01z'},
    smith: {label: 'smithy or forge', badge: '#5f6870',
        path: 'M1.5 4.5h9.5c0 1.8 1.4 3 3.5 3v1.2H10.4L9.2 11h1.6v2.2H4.6V11h1.6L5 8.7H3.6C2.4 8.7 1.5 7.8 1.5 6.6z'},
    tailor: {label: 'tailor or weaver', badge: '#6d5a86',
        path: 'M3.5 2.5h9v2h-9z M4.8 4.5h6.4v7H4.8z M3.5 11.5h9v2h-9z M6 6l4 1-4 1.2 4 1-4 1.3v-.9l1.8-.4-1.8-.5V7.5l1.8-.5L6 6.9z'},
    apothecary: {label: 'apothecary or healer', badge: '#4f7d5c',
        path: 'M6.2 1.8h3.6v1.6l-.6.8v1.7c2.3.8 3.6 2.4 3.6 4.6a4.8 4.8 0 0 1-9.6 0c0-2.2 1.3-3.8 3.6-4.6V4.2l-.6-.8z ' +
            'M4.8 10.2a3.2 3.2 0 0 0 6.4 0z'},
    church: {label: 'chapel or church', badge: '#8a7f5a',
        path: 'M7.1 1h1.8v2.8h3v1.8h-3V15H7.1V5.6h-3V3.8h3z'},
    watch: {label: 'the Watch', badge: '#4b5f78',
        path: 'M8 1.5l5.5 2v4.3c0 3.4-2.4 5.6-5.5 6.7-3.1-1.1-5.5-3.3-5.5-6.7V3.5z M8 3.4v9.3c2-.9 3.6-2.5 3.6-4.9V4.8z'},
    bank: {label: 'bank or counting house', badge: '#a08a3a',
        path: 'M3 11.3h10v2.2H3z M4 8.2h8v2.2H4z M5 5.1h6v2.2H5z M6.2 2.5h3.6v1.8H6.2z'},
    stables: {label: 'stables or carters', badge: '#7a6248',
        path: 'M8 1.5a6.5 6.5 0 1 0 0 13 6.5 6.5 0 0 0 0-13z M8 3.4a4.6 4.6 0 1 1 0 9.2 4.6 4.6 0 0 1 0-9.2z ' +
            'M7.3 3.3h1.4v9.4H7.3z M3.3 7.3h9.4v1.4H3.3z'},
    wharf: {label: 'wharf or boatyard', badge: '#3f6f86',
        path: 'M7.2 2.6a1.5 1.5 0 1 1 1.6 0v2.2H11v1.5H8.8v6.4c1.7-.3 3-1.4 3.5-3l-1.4.2 1.9-2.8 1 3.3-1-.3c-.7 2.6-3.1 4.4-6.8 4.4' +
            's-6.1-1.8-6.8-4.4l-1 .3 1-3.3 1.9 2.8-1.4-.2c.5 1.6 1.8 2.7 3.5 3V6.3H5V4.8h2.2z'},
    store: {label: 'store or shop', badge: '#6f7a46',
        path: 'M2 3h12l1.2 3.6H.8z M2.5 7.6h11V14h-11z M6.5 9.2v4.8h3V9.2z'},
    market: {label: 'market', badge: '#9a5b4a',
        path: 'M1 6.2L3 2h10l2 4.2z M3.4 2.4L2.6 6h1.8l.6-3.6z M7.2 2.4L7 6h2l-.2-3.6z M11 2.4L11.6 6h1.8l-.8-3.6z ' +
            'M2.6 7.4h1.4V14H2.6z M12 7.4h1.4V14H12z M2.6 10.5h10.8v1.4H2.6z'},
    brewery: {label: 'brewery or still', badge: '#7d5530',
        path: 'M4.2 2h7.6c1 1.9 1.4 3.9 1.4 6s-.4 4.1-1.4 6H4.2C3.2 12.1 2.8 10.1 2.8 8s.4-4.1 1.4-6z ' +
            'M3.6 4.8h8.8v1H3.6z M3.6 10.2h8.8v1H3.6z'},
    workshop: {label: 'workshop or works', badge: '#6b6151',
        path: 'M2 4.6L6 2.4l3 1.1v2l-1 1 6.2 6.2-1.6 1.6L6.4 8.1l-1 1H4z'},
    farm: {label: 'farm', badge: '#7f8a3e',
        path: 'M7.3 6h1.4v9H7.3z M8 1.2c1 .9 1.2 2.2.6 3.4L8 5.4l-.6-.8C6.8 3.4 7 2.1 8 1.2z ' +
            'M4.4 4.2c1.3.1 2.3.9 2.7 2.1l.3.9-1-.2C5.2 6.8 4.4 5.6 4.4 4.2z M11.6 4.2c0 1.4-.8 2.6-2 2.8l-1 .2.3-.9c.4-1.2 1.4-2 2.7-2.1z ' +
            'M4.4 7.8c1.3.1 2.3.9 2.7 2.1l.3.9-1-.2c-1.2-.2-2-1.4-2-2.8z M11.6 7.8c0 1.4-.8 2.6-2 2.8l-1 .2.3-.9c.4-1.2 1.4-2 2.7-2.1z'},
    hall: {label: 'hall or office', badge: '#5d6a6a',
        path: 'M8 1l7 4H1z M2 6h12v1.2H2z M3 7.8h2v4.6H3z M7 7.8h2v4.6H7z M11 7.8h2v4.6h-2z M1 13h14v2H1z'},
    house: {label: 'house', badge: '#5a5048',
        path: 'M8 2l6.5 6H13v6H9.6v-4H6.4v4H3V8H1.5z'},
};

/** The icon for a kind, or null for one with none (an unknown kind, or ""). */
export function iconFor(kind: string): MapIcon | null {
    return Object.hasOwn(MapIcons, kind) ? MapIcons[kind] : null;
}

const paths = new Map<string, Path2D | null>();
function pathOf(icon: MapIcon): Path2D | null {
    let p = paths.get(icon.path);
    if (p === undefined) {
        p = typeof Path2D === 'function' ? new Path2D(icon.path) : null;
        paths.set(icon.path, p);
    }
    return p;
}

/** Draws an icon centred at (x, y), `size` pixels across (its badge), at `alpha`. False where paths can't be drawn. */
export function drawMapIcon(c: CanvasRenderingContext2D, kind: string, x: number, y: number, size = 14, alpha = 1): boolean {
    const icon = iconFor(kind);
    const path = icon ? pathOf(icon) : null;
    if (!icon || !path) return false;
    c.save();
    c.globalAlpha *= alpha;
    c.beginPath();
    c.arc(x, y, size / 2, 0, Math.PI * 2);
    c.fillStyle = icon.badge;
    c.fill();
    c.lineWidth = 1;
    c.strokeStyle = 'rgba(11,18,19,0.85)';
    c.stroke();
    const k = (size * 0.72) / 16;
    c.translate(x - 8 * k, y - 8 * k);
    c.scale(k, k);
    c.fillStyle = '#f1e8d6';
    c.fill(path, 'evenodd');
    c.restore();
    return true;
}
