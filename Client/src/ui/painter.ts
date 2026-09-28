// Drawing, as the Unreal client drew: boxes, text, lines, gradients and fades on a fixed 1600×1000 canvas scaled to
// the window (RatwUI in UI/SRatwGame.cpp). Coordinates are the canvas's; the painter holds the scale.
import {css, withAlpha, type Color} from './color.ts';
import {font, pixels} from './theme.ts';

export interface Rect {
    left: number;
    top: number;
    right: number;
    bottom: number;
}
export const rect = (left: number, top: number, right: number, bottom: number): Rect => ({left, top, right, bottom});
export const contains = (r: Rect, x: number, y: number) => x >= r.left && x < r.right && y >= r.top && y < r.bottom;
export type Point = [number, number];

/** Just enough of a 2D context to measure text: the canvas's own, or a stand-in in tests. */
export interface Measurer {
    font: string;
    measureText(text: string): {width: number};
}

export class Painter {
    readonly ctx: CanvasRenderingContext2D;

    constructor(ctx: CanvasRenderingContext2D) {
        this.ctx = ctx;
    }

    box(x: number, y: number, w: number, h: number, color: Color) {
        if (color.a <= 0 || w <= 0 || h <= 0) return;
        this.ctx.fillStyle = css(color);
        this.ctx.fillRect(x, y, w, h);
    }

    text(x: number, y: number, text: string, size: number, color: Color, mono = false, bold = false) {
        if (!text || color.a <= 0) return;
        const c = this.ctx;
        c.font = font(size, mono, bold);
        c.fillStyle = css(color);
        c.textBaseline = 'top';
        // Slate places a line's box at the point, with its ascent a little below: nudge to match.
        c.fillText(text, x, y + pixels(size) * 0.12);
    }

    measure(text: string, size: number, mono = false): [number, number] {
        this.ctx.font = font(size, mono);
        return [this.ctx.measureText(text).width, pixels(size) * 1.2];
    }

    lines(points: Point[], color: Color, width = 1) {
        if (points.length < 2 || color.a <= 0) return;
        const c = this.ctx;
        c.beginPath();
        c.moveTo(points[0][0], points[0][1]);
        for (const [x, y] of points.slice(1)) c.lineTo(x, y);
        c.strokeStyle = css(color);
        c.lineWidth = width;
        c.lineJoin = 'round';
        c.lineCap = 'round';
        c.stroke();
    }

    frame(x: number, y: number, w: number, h: number, color: Color) {
        this.lines([[x, y], [x + w, y], [x + w, y + h], [x, y + h], [x, y]], color);
    }

    /** Three stops: start, middle, end; horizontal runs left to right, vertical top to bottom. */
    gradient(x: number, y: number, w: number, h: number, start: Color, middle: Color, end: Color, horizontal: boolean) {
        const g = horizontal ? this.ctx.createLinearGradient(x, 0, x + w, 0) : this.ctx.createLinearGradient(0, y, 0, y + h);
        g.addColorStop(0, css(start));
        g.addColorStop(0.5, css(middle));
        g.addColorStop(1, css(end));
        this.ctx.fillStyle = g;
        this.ctx.fillRect(x, y, w, h);
    }

    /** A fade inward from both edges of a rectangle along one axis; the middle stays clear. */
    edgeFade(r: Rect, feather: number, color: Color, horizontal: boolean) {
        const w = r.right - r.left, h = r.bottom - r.top;
        const extent = horizontal ? w : h;
        const width = Math.min(feather, extent * 0.5);
        if (width <= 0 || color.a <= 0) return;
        const g = horizontal ? this.ctx.createLinearGradient(r.left, 0, r.right, 0) : this.ctx.createLinearGradient(0, r.top, 0, r.bottom);
        const stop = (d: number, c: Color) => g.addColorStop(Math.min(1, Math.max(0, d / extent)), css(c));
        stop(0, color);
        stop(width * 0.35, withAlpha(color, color.a * 0.3));
        stop(width, withAlpha(color, 0));
        stop(extent - width, withAlpha(color, 0));
        stop(extent - width * 0.35, withAlpha(color, color.a * 0.3));
        stop(extent, color);
        this.ctx.fillStyle = g;
        this.ctx.fillRect(r.left, r.top, w, h);
    }

    /** A soft glow around a rectangle: concentric rounded outlines, fading outward. */
    halo(r: Rect, radius: number, color: Color) {
        const c = this.ctx;
        for (let offset = radius; offset > 0; offset -= 0.5) {
            const alpha = color.a * 0.34 * Math.pow(1 - offset / (radius + 0.5), 2);
            c.beginPath();
            c.roundRect(r.left - offset, r.top - offset, r.right - r.left + offset * 2, r.bottom - r.top + offset * 2, offset + 3);
            c.strokeStyle = css(withAlpha(color, alpha));
            c.lineWidth = 1.8;
            c.stroke();
        }
    }

    wrap(text: string, width: number, size: number): string[] {
        return wrapText(this.ctx, text, width, size);
    }

    /** Wrapped text; returns the height it took. */
    paragraph(x: number, y: number, text: string, width: number, size: number, color: Color, leading = 1.65): number {
        const rows = this.wrap(text, width, size);
        for (const row of rows) {
            this.text(x, y, row, size, color);
            y += size * leading;
        }
        return rows.length * size * leading;
    }

    clip(r: Rect, draw: () => void) {
        this.ctx.save();
        this.ctx.beginPath();
        this.ctx.rect(r.left, r.top, r.right - r.left, r.bottom - r.top);
        this.ctx.clip();
        try {
            draw();
        } finally {
            this.ctx.restore();
        }
    }
}

/** Word wrapping as the Unreal client did it: by words, paragraphs kept, a long word left whole. */
export function wrapText(measurer: Measurer, text: string, width: number, size: number): string[] {
    measurer.font = font(size);
    const result: string[] = [];
    for (const paragraph of text.split('\n')) {
        let row = '';
        for (const word of paragraph.split(' ')) {
            const next = row ? `${row} ${word}` : word;
            if (row && measurer.measureText(next).width > width) {
                result.push(row);
                row = word;
            } else row = next;
        }
        result.push(row);
    }
    return result;
}
