// How terrain glyphs are drawn: in the bundled DejaVu Sans Mono (declared with @font-face in styles.css, served
// from public/fonts), as each tile's Unicode glyph or, with "Plain ASCII glyphs" on, its plain-text fallback.
// Canvases draw no glyphs until the font has loaded (or failed to), then redraw: never in a fallback font.
// Shared by Atlas and the Dungeon Master.
import {useSyncExternalStore} from 'react';
import {TERRAIN} from '../model/model.mjs';

export const GLYPH_FAMILY = 'DejaVu Sans Mono';
/** The canvas font stack: the bundled font first, so symbols such as ♨ ⚒ ⚑ never fall to an emoji font. */
export const GLYPH_STACK = `"${GLYPH_FAMILY}", monospace`;
const ASCII_KEY = 'ratw-plain-ascii';

export interface GlyphRender {
    /** The glyph font has loaded (or could not be loaded): glyphs may be drawn. */
    ready: boolean;
    /** Draw each tile's `ascii` fallback instead of its Unicode glyph. */
    ascii: boolean;
    /** Changes whenever either does, so cached drawings can be thrown away. */
    version: number;
}

const stored = () => { try { return localStorage.getItem(ASCII_KEY) === '1'; } catch { return false; } };
let render: GlyphRender = {ready: typeof document === 'undefined' || !document.fonts, ascii: stored(), version: 0};
const listeners = new Set<() => void>();
const update = (patch: Partial<GlyphRender>) => {
    render = {...render, ...patch, version: render.version + 1};
    listeners.forEach(l => l());
};

export const glyphRender = () => render;
export function setPlainAscii(on: boolean) {
    if (on === render.ascii) return;
    try { localStorage.setItem(ASCII_KEY, on ? '1' : '0'); } catch { /* private window */ }
    update({ascii: on});
}
/** Re-renders the caller when the font loads or the glyph style changes (and starts loading the font). */
export function useGlyphRender(): GlyphRender {
    return useSyncExternalStore(l => { listeners.add(l); void loadGlyphFont(); return () => listeners.delete(l); }, glyphRender);
}

let loading: Promise<void> | null = null;
/**
 * Loads the glyph font once (asking for every catalog glyph, so the whole face is fetched) and resolves when glyphs
 * may be drawn. Started by the first canvas that subscribes, after the stylesheet with the @font-face is in place.
 * Should the load settle without the face (no stylesheet yet, or a failed fetch), glyphs draw in the fallback and
 * the canvases redraw once the real face arrives.
 */
export function loadGlyphFont(): Promise<void> {
    if (loading) return loading;
    if (typeof document === 'undefined' || !document.fonts) return (loading = Promise.resolve());
    const fonts = document.fonts, spec = `16px "${GLYPH_FAMILY}"`;
    const sample = [...new Set(TERRAIN.flatMap(t => [t.glyph, t.ascii]))].join('');
    // Whenever the face itself finishes loading (even after a fallback settle), redraw with it.
    fonts.addEventListener?.('loadingdone', e => {
        if ((e as FontFaceSetLoadEvent).fontfaces?.some(f => f.family.replace(/["']/g, '') === GLYPH_FAMILY)) update({ready: true});
    });
    loading = fonts.load(spec, sample).then(() => undefined, () => undefined).then(() => update({ready: true}));
    return loading;
}

/** Text presentation for symbols with an emoji form (Miscellaneous Symbols, Dingbats and above): U+FE0E. */
export const textPresentation = (glyph: string) => ((glyph.codePointAt(0) ?? 0) >= 0x2600 ? glyph + '\uFE0E' : glyph);
/** What a canvas draws for a tile. */
export const drawnGlyph = (tile: {glyph: string; ascii: string}, ascii = render.ascii) =>
    ascii ? tile.ascii : textPresentation(tile.glyph);
