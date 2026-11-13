// The painted wolves on the page (Docs/Design/81-painted-wolves.md): the three paintings (a profile and a face each,
// cut from their paper by tools/avatar_prep.py, with a map of the fur zones, the ink and the eyes), drawn in a wolf's
// own fur (fur.ts). Fast enough to repaint while a player picks colours: each painting is read once at the size it is
// shown, each look painted once and kept.
import {eyeRgb, paintFur, zoneRamps, type Build, type Fur, type FurSource} from './fur.ts';

export type View = 'profile' | 'face';

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

    private sheet(build: Build, view: View): Sheet | null {
        const key = `${build}-${view}`;
        let s = this.sheets.get(key);
        if (!s) {
            if (typeof Image === 'undefined') return null;
            const load = (url: string) => {
                const image = new Image();
                image.onload = () => this.onReady();
                image.src = url;
                return image;
            };
            s = {picture: load(new URL(`../assets/wolves/${build}-${view}.png`, import.meta.url).href),
                map: load(new URL(`../assets/wolves/${build}-${view}-map.png`, import.meta.url).href)};
            this.sheets.set(key, s);
        }
        const ready = (i: HTMLImageElement) => i.complete && i.naturalWidth > 0;
        return ready(s.picture) && ready(s.map) ? s : null;
    }

    /** How wide a painting is for its height (0 until it has arrived). */
    aspect(build: Build, view: View): number {
        const s = this.sheet(build, view);
        return s ? s.picture.naturalWidth / s.picture.naturalHeight : 0;
    }

    /** A painting and its map read at one size: the picture smoothed, the map by nearest pixel (its values are zones). */
    private source(build: Build, view: View, width: number, height: number): FurSource | null {
        const key = `${build}-${view}-${width}x${height}`;
        const kept = this.sources.get(key);
        if (kept) return kept;
        const s = this.sheet(build, view);
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
        if (this.sources.size > 24) this.sources.clear();
        const source = {width, height, picture, map};
        this.sources.set(key, source);
        return source;
    }

    /** A wolf painted in its fur, as large as fits a box (in canvas pixels); null until its painting has arrived. */
    get(build: Build, fur: Fur, view: View, boxWidth: number, boxHeight: number): HTMLCanvasElement | null {
        const aspect = this.aspect(build, view);
        if (!aspect || typeof document === 'undefined') return null;
        const width = Math.max(1, Math.round(Math.min(boxWidth, boxHeight * aspect)));
        const height = Math.max(1, Math.round(width / aspect));
        const key = `${build}-${view}-${width}x${height}-${JSON.stringify([fur.coat, fur.eyes, fur.zones ?? null])}`;
        const kept = this.made.get(key);
        if (kept) return kept;
        const source = this.source(build, view, width, height);
        if (!source) return null;
        const canvas = document.createElement('canvas');
        canvas.width = width;
        canvas.height = height;
        const c = canvas.getContext('2d');
        if (!c) return null;
        const image = c.createImageData(width, height);
        paintFur(source, zoneRamps(fur), eyeRgb(fur.eyes), image.data);
        c.putImageData(image, 0, 0);
        if (this.made.size > 96) this.made.clear();
        this.made.set(key, canvas);
        return canvas;
    }

    /** One view of a wolf, centred in a box. False if there is nothing to draw yet. */
    draw(ctx: CanvasRenderingContext2D, build: Build, fur: Fur, view: View, x: number, y: number, w: number, h: number): boolean {
        const image = this.get(build, fur, view, Math.floor(w), Math.floor(h));
        if (!image) return false;
        ctx.drawImage(image, Math.round(x + (w - image.width) / 2), Math.round(y + (h - image.height) / 2));
        return true;
    }

    /** A wolf's card: its paper, the profile, and the face beside it. False until both paintings have arrived. */
    drawCard(ctx: CanvasRenderingContext2D, build: Build, fur: Fur, x: number, y: number, w: number, h: number): boolean {
        if (!this.aspect(build, 'profile') || !this.aspect(build, 'face')) return false;
        paper(ctx, x, y, w, h, Math.min(w, h) * 0.04);
        const pad = Math.min(w, h) * 0.05, gap = pad * 0.6;
        const faceWidth = (w - 2 * pad - gap) * 0.27;
        this.draw(ctx, build, fur, 'profile', x + pad, y + pad, w - 2 * pad - gap - faceWidth, h - 2 * pad);
        this.draw(ctx, build, fur, 'face', x + w - pad - faceWidth, y + pad, faceWidth, (h - 2 * pad) * 0.62);
        return true;
    }

    /** One view alone on its paper (a build to choose, a face beside a name). False until it has arrived. */
    drawOnPaper(ctx: CanvasRenderingContext2D, build: Build, fur: Fur, view: View, x: number, y: number, w: number, h: number): boolean {
        if (!this.aspect(build, view)) return false;
        const pad = Math.min(w, h) * 0.07;
        paper(ctx, x, y, w, h, Math.min(w, h) * 0.14);
        return this.draw(ctx, build, fur, view, x + pad, y + pad, w - 2 * pad, h - 2 * pad);
    }
}

function paper(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, radius: number) {
    ctx.save();
    ctx.fillStyle = WolfPaper;
    ctx.beginPath();
    ctx.roundRect(x, y, w, h, radius);
    ctx.fill();
    ctx.restore();
}
