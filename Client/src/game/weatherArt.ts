// Stylised weather art, painted in code (UI/RatwWeatherArt.cpp): every sheet is white with a shaped alpha, so one
// sheet serves any tint, and every sheet but the light pool tiles seamlessly. Nothing here reads the world.

export type Art = 'mist' | 'cloud' | 'rain' | 'snow' | 'dust' | 'pool';
export const Size = 256;

function hash(x: number, y: number, seed: number): number {
    let h = (Math.imul(x, 0x8da6b343) ^ Math.imul(y, 0xd8163841) ^ Math.imul(seed, 0xcb1ab31f)) >>> 0;
    h = (h ^ (h >>> 13)) >>> 0;
    h = Math.imul(h, 0x5bd1e995) >>> 0;
    return (h ^ (h >>> 15)) >>> 0;
}
const unit = (x: number, y: number, seed: number) => (hash(x, y, seed) & 0xffffff) / 0xffffff;
function smooth(a: number, b: number, v: number) {
    const t = Math.min(1, Math.max(0, (v - a) / (b - a)));
    return t * t * (3 - 2 * t);
}
const mix = (a: number, b: number, t: number) => a + (b - a) * t;
// Value noise on a lattice that wraps every px by py cells, so the sheet tiles.
function lattice(u: number, v: number, px: number, py: number, seed: number) {
    const x0 = Math.floor(u), y0 = Math.floor(v);
    const fx = smooth(0, 1, u - x0), fy = smooth(0, 1, v - y0);
    const at = (x: number, y: number) => unit(((x % px) + px) % px, ((y % py) + py) % py, seed);
    return mix(mix(at(x0, y0), at(x0 + 1, y0), fx), mix(at(x0, y0 + 1), at(x0 + 1, y0 + 1), fx), fy);
}
function fbm(x: number, y: number, px: number, py: number, octaves: number, seed: number) {
    let sum = 0, weight = 0.5, total = 0;
    for (let o = 0; o < octaves; ++o, px *= 2, py *= 2, weight *= 0.5) {
        sum += weight * lattice(x / Size * px, y / Size * py, px, py, (seed + o * 7919) >>> 0);
        total += weight;
    }
    return sum / total;
}
const wrap = (v: number) => ((v % Size) + Size) % Size;

/** The sheet's alpha, row by row, 0..255. Deterministic, so tests can inspect it. */
export function alphaOf(art: Art): Uint8ClampedArray {
    const alpha = new Float32Array(Size * Size);
    const put = (x: number, y: number, a: number) => {
        const i = wrap(y) * Size + wrap(x);
        alpha[i] = Math.max(alpha[i], a);
    };
    const each = (f: (x: number, y: number) => number) => {
        for (let y = 0; y < Size; ++y) for (let x = 0; x < Size; ++x) alpha[y * Size + x] = f(x, y);
    };
    switch (art) {
    case 'mist': each((x, y) => smooth(0.34, 0.82, fbm(x, y, 4, 4, 4, 11)) * 0.92); break;
    case 'cloud': each((x, y) => smooth(0.5, 0.66, fbm(x, y, 3, 3, 5, 23))); break;
    case 'dust':
        // Long in X, short in Y: gusts of sand read as streaks along the wind.
        each((x, y) => Math.min(1, Math.max(0, smooth(0.42, 0.84, fbm(x, y, 2, 14, 4, 37)) * 0.8 +
            smooth(0.55, 0.9, fbm(x, y, 4, 48, 2, 41)) * 0.45)));
        break;
    case 'rain':
        for (let i = 0; i < 150; ++i) {
            const x = Math.trunc(unit(i, 1, 53) * Size), y0 = Math.trunc(unit(i, 2, 53) * Size);
            const length = 9 + Math.trunc(unit(i, 3, 53) * 22);
            const bright = 0.45 + 0.55 * unit(i, 4, 53);
            // The head (bottom, the direction of fall) is brightest; the tail fades out above it.
            for (let t = 0; t <= length; ++t) {
                const a = bright * Math.pow(t / length, 1.4);
                put(x, y0 + t, a);
                put(x + 1, y0 + t, a * 0.22);
            }
        }
        break;
    case 'snow':
        for (let i = 0; i < 95; ++i) {
            const cx = unit(i, 1, 71) * Size, cy = unit(i, 2, 71) * Size;
            const r = 0.9 + unit(i, 3, 71) ** 2 * 2.2;
            for (let dy = -3; dy <= 3; ++dy)
                for (let dx = -3; dx <= 3; ++dx) {
                    const x = Math.floor(cx) + dx, y = Math.floor(cy) + dy;
                    put(x, y, 1 - smooth(r * 0.4, r, Math.hypot(x + 0.5 - cx, y + 0.5 - cy)));
                }
        }
        break;
    case 'pool':
        each((x, y) => smooth(0.5, 1, Math.hypot(x + 0.5 - Size / 2, y + 0.5 - Size / 2) / (Size / 2)));
        break;
    }
    const out = new Uint8ClampedArray(Size * Size);
    for (let i = 0; i < out.length; ++i) out[i] = Math.round(alpha[i] * 255);
    return out;
}

/** Sheets as canvases, made once each, and tinted copies by colour. */
export class Sheets {
    private sheets = new Map<Art, HTMLCanvasElement>();
    private tinted = new Map<string, HTMLCanvasElement>();

    /** Makes every sheet ahead of need, one per idle moment, so the first rain never costs a frame. */
    warm() {
        const arts: Art[] = ['cloud', 'rain', 'mist', 'snow', 'dust', 'pool'];
        const idle = (f: () => void) => (typeof requestIdleCallback === 'function' ? requestIdleCallback(f, {timeout: 2000}) : setTimeout(f, 50));
        const next = () => {
            const art = arts.shift();
            if (!art) return;
            this.sheet(art);
            idle(next);
        };
        idle(next);
    }

    /** The sheet in one colour (its alpha is the sheet's; the tint's alpha is applied when drawing). */
    get(art: Art, rgb: string): HTMLCanvasElement {
        const key = `${art}:${rgb}`;
        let canvas = this.tinted.get(key);
        if (canvas) return canvas;
        canvas = document.createElement('canvas');
        canvas.width = canvas.height = Size;
        const c = canvas.getContext('2d')!;
        c.drawImage(this.sheet(art), 0, 0);
        c.globalCompositeOperation = 'source-in';
        c.fillStyle = rgb;
        c.fillRect(0, 0, Size, Size);
        this.tinted.set(key, canvas);
        return canvas;
    }

    private sheet(art: Art): HTMLCanvasElement {
        let canvas = this.sheets.get(art);
        if (canvas) return canvas;
        canvas = document.createElement('canvas');
        canvas.width = canvas.height = Size;
        const c = canvas.getContext('2d')!;
        const image = c.createImageData(Size, Size);
        const alpha = alphaOf(art);
        for (let i = 0; i < alpha.length; ++i) {
            image.data[i * 4] = image.data[i * 4 + 1] = image.data[i * 4 + 2] = 255;
            image.data[i * 4 + 3] = alpha[i];
        }
        c.putImageData(image, 0, 0);
        this.sheets.set(art, canvas);
        return canvas;
    }
}
