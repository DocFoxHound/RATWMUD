// Uploaded portraits on the client (Docs/Design/29-client-polish.md, phase 9): fetched once by ID and kept, and sent
// as raw pixels after the browser has cropped the picture square and scaled it to 256×256 (the server encodes it).
import type {Json} from '../game/json.ts';

export const Side = 256;
const Parts = 8;                                    // Each part well under the 64 KB a command may be.

export class ArtCache {
    private items = new Map<string, HTMLImageElement | 'loading' | 'denied'>();
    send: (command: Json) => void = () => {};
    onReady: () => void = () => {};

    /** The portrait, once it has arrived (asked for the first time it is wanted); null until then, or if not allowed. */
    get(id: string | undefined | null): HTMLImageElement | null {
        if (!id || typeof document === 'undefined') return null;
        const v = this.items.get(id);
        if (v && typeof v === 'object') return v.complete && v.naturalWidth > 0 ? v : null;
        if (!v) {
            this.items.set(id, 'loading');
            this.send({type: 'artwork_get', id});
        }
        return null;
    }

    receive(e: Json) {
        const id = typeof e.id === 'string' ? e.id : '';
        if (!id) return;
        if (e.denied || typeof e.png !== 'string') {
            this.items.set(id, 'denied');
            return;
        }
        const image = new Image();
        image.onload = () => this.onReady();
        image.src = `data:image/png;base64,${e.png}`;
        this.items.set(id, image);
    }

    /** Forget a portrait (a decision changed who may see it): asked for again when next wanted. */
    forget(id: string) {
        this.items.delete(id);
    }
}

export const artCache = new ArtCache();

/** A picture file as 256×256 RGBA pixels: the middle square of it, scaled. */
export async function squarePixels(file: Blob): Promise<{pixels: Uint8ClampedArray; preview: HTMLCanvasElement}> {
    if (file.size > 12 * 1024 * 1024) throw new Error('That picture is too large (12 MB at most).');
    const bitmap = await createImageBitmap(file);
    if (bitmap.width < 32 || bitmap.height < 32) throw new Error('That picture is too small.');
    const side = Math.min(bitmap.width, bitmap.height);
    const canvas = document.createElement('canvas');
    canvas.width = canvas.height = Side;
    const c = canvas.getContext('2d')!;
    c.imageSmoothingQuality = 'high';
    c.drawImage(bitmap, (bitmap.width - side) / 2, (bitmap.height - side) / 2, side, side, 0, 0, Side, Side);
    bitmap.close();
    return {pixels: c.getImageData(0, 0, Side, Side).data, preview: canvas};
}

/** Sends the pixels for a character, in parts. */
export function sendPortrait(pixels: Uint8ClampedArray, characterId: string, submit: (command: Json) => void) {
    const uploadId = `up-${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 8)}`;
    const size = Math.ceil(pixels.length / Parts);
    for (let part = 0; part < Parts; ++part) {
        const slice = pixels.subarray(part * size, Math.min(pixels.length, (part + 1) * size));
        let binary = '';
        for (let i = 0; i < slice.length; i += 8192) binary += String.fromCharCode(...slice.subarray(i, i + 8192));
        submit({type: 'artwork_upload', characterId, uploadId, part, parts: Parts, data: btoa(binary)});
    }
}
