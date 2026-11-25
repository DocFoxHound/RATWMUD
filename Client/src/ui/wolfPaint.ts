// The wolf dolls on the page (Docs/Design/81-painted-wolves.md): six layered wolf dolls (female/male x small/medium/
// large; packed by tools/doll_prep.py into three lossless RGB PNGs per doll: idx, wt, ink) plus the shared face
// painting (face.png + face-map.png, kept until new faces are added). All six dolls share one ground-aligned frame
// (1033x821), so drawn in boxes of one size they keep their exact painted relative scale.
//
// Rendering strategy: each doll's three packed PNGs are read once at native resolution; each wolf look is composited
// cleanly at native resolution into a shared scratch canvas and downscaled with high-quality smoothing into a cached
// display-sized canvas (LRU). This keeps lineart razor-sharp at every size and repaints live in ~2 ms.
import {dollColours, eyeRgb, paintDoll, paintFur, zoneRamps, type Build, type DollKey, type DollSource, type Fur, type FurSource} from './fur.ts';

/** A painting: the face, one of the six sex/size dolls, or a legacy bare build (maps to male-<build>). */
export type Painting = 'face' | DollKey | Build;

/** The paper a wolf is shown on when paper is requested. */
export const WolfPaper = '#e6dccb';

interface FaceSheet {
    picture: HTMLImageElement;
    map: HTMLImageElement;
}

interface DollSheet {
    idx: HTMLImageElement;
    wt: HTMLImageElement;
    ink: HTMLImageElement;
}

function normalizeDoll(painting: DollKey | Build): DollKey {
    return painting.includes('-') ? painting as DollKey : `male-${painting}` as DollKey;
}

export class PaintedWolves {
    private faceSheet: FaceSheet | null = null;
    private dollSheets = new Map<DollKey, DollSheet>();
    private faceSources = new Map<string, FurSource>();
    private dollSources = new Map<DollKey, DollSource>();
    private made = new Map<string, HTMLCanvasElement>();
    private scratch: HTMLCanvasElement | null = null;
    private scratchImg: ImageData | null = null;
    /** An asset has arrived: draw again. */
    onReady: () => void = () => {};

    private loadImg(url: string): HTMLImageElement {
        const image = new Image();
        image.onload = () => this.onReady();
        image.src = url;
        return image;
    }

    private ready(i: HTMLImageElement): boolean {
        return i.complete && i.naturalWidth > 0;
    }

    private getFaceSheet(): FaceSheet | null {
        if (!this.faceSheet) {
            if (typeof Image === 'undefined') return null;
            this.faceSheet = {
                picture: this.loadImg(new URL('../assets/wolves/face.png', import.meta.url).href),
                map: this.loadImg(new URL('../assets/wolves/face-map.png', import.meta.url).href),
            };
        }
        return this.ready(this.faceSheet.picture) && this.ready(this.faceSheet.map) ? this.faceSheet : null;
    }

    private getDollSheet(doll: DollKey): DollSheet | null {
        let s = this.dollSheets.get(doll);
        if (!s) {
            if (typeof Image === 'undefined') return null;
            s = {
                idx: this.loadImg(new URL(`../assets/wolves/${doll}-idx.png`, import.meta.url).href),
                wt: this.loadImg(new URL(`../assets/wolves/${doll}-wt.png`, import.meta.url).href),
                ink: this.loadImg(new URL(`../assets/wolves/${doll}-ink.png`, import.meta.url).href),
            };
            this.dollSheets.set(doll, s);
        }
        return this.ready(s.idx) && this.ready(s.wt) && this.ready(s.ink) ? s : null;
    }

    /** How wide a painting is for its height (0 until its assets have arrived). */
    aspect(painting: Painting): number {
        if (painting === 'face') {
            const s = this.getFaceSheet();
            return s ? s.picture.naturalWidth / s.picture.naturalHeight : 0;
        }
        const s = this.getDollSheet(normalizeDoll(painting));
        return s ? s.idx.naturalWidth / s.idx.naturalHeight : 0;
    }

    private readImageData(image: HTMLImageElement, width: number, height: number, smooth: boolean): Uint8ClampedArray | null {
        const canvas = document.createElement('canvas');
        canvas.width = width;
        canvas.height = height;
        const c = canvas.getContext('2d', {willReadFrequently: true});
        if (!c) return null;
        c.imageSmoothingEnabled = smooth;
        if (smooth) c.imageSmoothingQuality = 'high';
        c.drawImage(image, 0, 0, width, height);
        return c.getImageData(0, 0, width, height).data;
    }

    private faceSource(width: number, height: number): FurSource | null {
        const key = `face-${width}x${height}`;
        const kept = this.faceSources.get(key);
        if (kept) return kept;
        const s = this.getFaceSheet();
        if (!s) return null;
        const picture = this.readImageData(s.picture, width, height, true);
        const map = this.readImageData(s.map, width, height, false);
        if (!picture || !map) return null;
        forgetOldest(this.faceSources, 24);
        const source = {width, height, picture, map};
        this.faceSources.set(key, source);
        return source;
    }

    private dollSource(doll: DollKey): DollSource | null {
        const kept = this.dollSources.get(doll);
        if (kept) return kept;
        const s = this.getDollSheet(doll);
        if (!s) return null;
        const w = s.idx.naturalWidth, h = s.idx.naturalHeight;
        const idx = this.readImageData(s.idx, w, h, false);
        const wt = this.readImageData(s.wt, w, h, false);
        const ink = this.readImageData(s.ink, w, h, false);
        if (!idx || !wt || !ink) return null;
        const source: DollSource = {width: w, height: h, idx, wt, ink};
        this.dollSources.set(doll, source);
        return source;
    }

    /** A painting in a wolf's fur, as large as fits a box (in canvas pixels); null until its assets have arrived. */
    get(painting: Painting, fur: Fur, boxWidth: number, boxHeight: number): HTMLCanvasElement | null {
        const aspect = this.aspect(painting);
        if (!aspect || typeof document === 'undefined') return null;
        const width = Math.max(1, Math.round(Math.min(boxWidth, boxHeight * aspect)));
        const height = Math.max(1, Math.round(width / aspect));
        const norm = painting === 'face' ? 'face' : normalizeDoll(painting);
        const key = `${norm}-${width}x${height}-${JSON.stringify([fur.coat, fur.eyes, fur.claws ?? null, fur.zones ?? null, fur.layers ?? null])}`;
        const kept = this.made.get(key);
        if (kept) {
            this.made.delete(key);
            this.made.set(key, kept);
            return kept;
        }
        const canvas = document.createElement('canvas');
        canvas.width = width;
        canvas.height = height;
        const c = canvas.getContext('2d');
        if (!c) return null;

        if (norm === 'face') {
            const source = this.faceSource(width, height);
            if (!source) return null;
            const image = c.createImageData(width, height);
            paintFur(source, zoneRamps(fur), eyeRgb(fur.eyes), image.data);
            c.putImageData(image, 0, 0);
        } else {
            const source = this.dollSource(norm);
            if (!source) return null;
            if (!this.scratch || this.scratch.width !== source.width || this.scratch.height !== source.height) {
                this.scratch = document.createElement('canvas');
                this.scratch.width = source.width;
                this.scratch.height = source.height;
                this.scratchImg = this.scratch.getContext('2d')!.createImageData(source.width, source.height);
            }
            const sc = this.scratch.getContext('2d');
            if (!sc || !this.scratchImg) return null;
            paintDoll(source, dollColours(fur), this.scratchImg.data);
            sc.putImageData(this.scratchImg, 0, 0);
            c.imageSmoothingEnabled = true;
            c.imageSmoothingQuality = 'high';
            c.drawImage(this.scratch, 0, 0, width, height);
        }

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

    /** A painting on its paper, filling a box. False until its assets have arrived. */
    drawOnPaper(ctx: CanvasRenderingContext2D, painting: Painting, fur: Fur, x: number, y: number, w: number, h: number): boolean {
        if (!this.aspect(painting)) return false;
        const pad = Math.min(w, h) * 0.07;
        paper(ctx, x, y, w, h, Math.min(w, h) * (painting === 'face' ? 0.14 : 0.06));
        return this.draw(ctx, painting, fur, x + pad, y + pad, w - 2 * pad, h - 2 * pad);
    }
}

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
