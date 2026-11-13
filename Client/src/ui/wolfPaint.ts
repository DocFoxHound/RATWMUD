// The painted wolves on the page (Docs/Design/81-painted-wolves.md): the paintings (one face, shared by every wolf,
// and a profile for each of the three sizes; made ready by tools/avatar_prep.py, each with a map of the fur zones, the
// ink and the eyes), drawn in a wolf's own fur (fur.ts). The face and the profile are separate pictures: a page shows
// each where it wants. The three profiles share one frame (the large wolf's, the paws on one ground line), so drawn in
// boxes of one size they keep the sizes they were painted at. Fast enough to repaint while a player picks colours:
// each painting is read once at the size it is shown, each look painted once and kept.
import {eyeRgb, paintFur, zoneRamps, type Build, type Fur, type FurSource} from './fur.ts';

/** A painting: the face, or the profile of a size. */
export type Painting = 'face' | Build;

/** The paper a painted wolf is shown on: the paintings' own, a little deeper so a white wolf stands off it. */
export const WolfPaper = '#e6dccb';

interface Sheet {
    picture: HTMLImageElement;
    map: HTMLImageElement;
}

export class PaintedWolves {
    private sheets = new Map<string, Sheet>();
    private sources = new Map<string, FurSource>();
    private made = new Map<string, HTMLCanvasElement>();
    /** A painting has arrived: draw again. */
    onReady: () => void = () => {};

    private sheet(painting: Painting): Sheet | null {
        const key = painting === 'face' ? 'face' : `${painting}-profile`;
        let s = this.sheets.get(key);
        if (!s) {
            if (typeof Image === 'undefined') return null;
            const load = (url: string) => {
                const image = new Image();
                image.onload = () => this.onReady();
                image.src = url;
                return image;
            };
            s = {picture: load(new URL(`../assets/wolves/${key}.png`, import.meta.url).href),
                map: load(new URL(`../assets/wolves/${key}-map.png`, import.meta.url).href)};
            this.sheets.set(key, s);
        }
        const ready = (i: HTMLImageElement) => i.complete && i.naturalWidth > 0;
        return ready(s.picture) && ready(s.map) ? s : null;
    }

    /** How wide a painting is for its height (0 until it has arrived). */
    aspect(painting: Painting): number {
        const s = this.sheet(painting);
        return s ? s.picture.naturalWidth / s.picture.naturalHeight : 0;
    }

    /** A painting and its map read at one size: the picture smoothed, the map by nearest pixel (its values are zones). */
    private source(painting: Painting, width: number, height: number): FurSource | null {
        const key = `${painting}-${width}x${height}`;
        const kept = this.sources.get(key);
        if (kept) return kept;
        const s = this.sheet(painting);
        if (!s) return null;
        const read = (image: HTMLImageElement, smooth: boolean) => {
            const canvas = document.createElement('canvas');
            canvas.width = width;
            canvas.height = height;
            const c = canvas.getContext('2d', {willReadFrequently: true});
            if (!c) return null;
            c.imageSmoothingEnabled = smooth;
            c.imageSmoothingQuality = 'high';
            c.drawImage(image, 0, 0, width, height);
            return c.getImageData(0, 0, width, height).data;
        };
        const picture = read(s.picture, true), map = read(s.map, false);
        if (!picture || !map) return null;
        forgetOldest(this.sources, 48);
        const source = {width, height, picture, map};
        this.sources.set(key, source);
        return source;
    }

    /** A painting in a wolf's fur, as large as fits a box (in canvas pixels); null until it has arrived. */
    get(painting: Painting, fur: Fur, boxWidth: number, boxHeight: number): HTMLCanvasElement | null {
        const aspect = this.aspect(painting);
        if (!aspect || typeof document === 'undefined') return null;
        const width = Math.max(1, Math.round(Math.min(boxWidth, boxHeight * aspect)));
        const height = Math.max(1, Math.round(width / aspect));
        const key = `${painting}-${width}x${height}-${JSON.stringify([fur.coat, fur.eyes, fur.zones ?? null])}`;
        const kept = this.made.get(key);
        if (kept) {
            this.made.delete(key);              // (Used: the newest again.)
            this.made.set(key, kept);
            return kept;
        }
        const source = this.source(painting, width, height);
        if (!source) return null;
        const canvas = document.createElement('canvas');
        canvas.width = width;
        canvas.height = height;
        const c = canvas.getContext('2d');
        if (!c) return null;
        const image = c.createImageData(width, height);
        paintFur(source, zoneRamps(fur), eyeRgb(fur.eyes), image.data);
        c.putImageData(image, 0, 0);
        forgetOldest(this.made, 256);
        this.made.set(key, canvas);
        return canvas;
    }

    /** A painting in a wolf's fur, centred in a box. False if there is nothing to draw yet. */
    draw(ctx: CanvasRenderingContext2D, painting: Painting, fur: Fur, x: number, y: number, w: number, h: number): boolean {
        const image = this.get(painting, fur, Math.floor(w), Math.floor(h));
        if (!image) return false;
        ctx.drawImage(image, Math.round(x + (w - image.width) / 2), Math.round(y + (h - image.height) / 2));
        return true;
    }

    /** A painting on its paper, filling a box: a wolf's profile (its card, a size to choose) or its face (a portrait,
     *  an avatar beside a name). False until it has arrived. */
    drawOnPaper(ctx: CanvasRenderingContext2D, painting: Painting, fur: Fur, x: number, y: number, w: number, h: number): boolean {
        if (!this.aspect(painting)) return false;
        const pad = Math.min(w, h) * 0.07;
        paper(ctx, x, y, w, h, Math.min(w, h) * (painting === 'face' ? 0.14 : 0.06));
        return this.draw(ctx, painting, fur, x + pad, y + pad, w - 2 * pad, h - 2 * pad);
    }
}

/** Keeps a cache under its size: the longest kept goes first (a Map keeps the order things were put in). In a town
 *  the In Sight list and a fight's cards hold dozens of looks at a few sizes each, so one more never clears the rest. */
function forgetOldest<T>(cache: Map<string, T>, most: number) {
    while (cache.size >= most) cache.delete(cache.keys().next().value!);
}

function paper(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, radius: number) {
    ctx.save();
    ctx.fillStyle = WolfPaper;
    ctx.beginPath();
    ctx.roundRect(x, y, w, h, radius);
    ctx.fill();
    ctx.restore();
}
