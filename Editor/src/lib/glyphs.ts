import {TERRAIN, TERRAIN_CATEGORIES, type Role, type TerrainCategory, type TerrainTile} from '../model/model.mjs';

// Terrain palette: the catalog (Data/Terrain/terrain.json) plus editor hotkeys. 1–0 and - keep the original eleven
// tiles; Shift with the same keys picks a second bank of common tiles.
export const HOTKEYS: Readonly<Record<string, string>> = {
    '.': '1', ',': '2', '"': '3', '#': '4', 'T': '5', '=': '6', '~': '7', ':': '8', '^': '9', '+': '0', '%': '-',
    'd': '⇧1', '_': '⇧2', 'f': '⇧3', 'H': '⇧4', 'j': '⇧5', 'G': '⇧6', 'P': '⇧7', 'Y': '⇧8', 'B': '⇧9', 'o': '⇧0', 'b': '⇧-',
};
export interface GlyphInfo extends TerrainTile { key?: string }
export const GLYPHS: GlyphInfo[] = TERRAIN.map(t => ({...t, key: HOTKEYS[t.code]}));
const BY_CODE = new Map(GLYPHS.map(g => [g.code, g]));
/** The catalog tile for a stored code; unknown codes read as the first tile (floor). */
export const glyphInfo = (code: string | null | undefined): GlyphInfo => BY_CODE.get(code ?? '') ?? GLYPHS[0];
/** Tiles bound to a hotkey, in key order (the palette's quick strip). */
export const QUICK_GLYPHS: GlyphInfo[] = GLYPHS.filter(g => g.key && !g.key.startsWith('⇧'))
    .sort((a, b) => '1234567890-'.indexOf(a.key!) - '1234567890-'.indexOf(b.key!));

/** The tile a key press picks: 1–0 and - (top row or keypad), with or without Shift. */
export function hotkeyGlyph(e: {code: string; shiftKey: boolean}): GlyphInfo | undefined {
    const m = /^(?:(?:Digit|Numpad)([0-9])|Minus|NumpadSubtract)$/.exec(e.code);
    if (!m) return undefined;
    const key = (e.shiftKey ? '⇧' : '') + (m[1] ?? '-');
    return GLYPHS.find(g => g.key === key);
}
/** A tile's label: "Stairs (^)". */
export const glyphLabel = (g: TerrainTile) => `${g.name} (${g.code})`;
/** Tooltip for a palette button. */
export const glyphTitle = (g: GlyphInfo) => `${g.name} · ${g.effect} · stored as ${g.code}${g.key ? ` · key ${g.key}` : ''}`;
/** Whether a tile matches a palette filter (name, effect, code, glyph or category). */
export function matchesGlyph(g: TerrainTile, query: string): boolean {
    const q = query.trim().toLowerCase();
    if (!q) return true;
    const category = TERRAIN_CATEGORIES.find(c => c.id === g.category)?.name ?? '';
    return g.code === query.trim() || g.glyph === query.trim()
        || `${g.name} ${g.effect} ${g.kind} ${category}`.toLowerCase().includes(q);
}
/** The palette's groups: each catalog category with its tiles, in catalog order. */
export const GLYPH_GROUPS: (TerrainCategory & {tiles: GlyphInfo[]})[] =
    TERRAIN_CATEGORIES.map(c => ({...c, tiles: GLYPHS.filter(g => g.category === c.id)}));

export const ROLE_INFO: Record<Role, {label: string; color: string; icon: string; blurb: string}> = {
    merchant: {label: 'Merchant', color: '#e6c481', icon: '⚖', blurb: 'Keeps shop at their work place during their hours. Players can Trade with them.'},
    guard: {label: 'Guard', color: '#a8cedA', icon: '⛨', blurb: 'On watch during their hours: walks a patrol route, or holds their work post.'},
    civilian: {label: 'Civilian', color: '#a8c7ad', icon: '⌂', blurb: 'Works during their hours, spends the evening at their evening place, sleeps at home.'},
};

// The game's 32 speaking colors (SRatwGame::SpeakingColor), so voices look the same in play.
export const VOICE_COLORS = [0xDAC69D, 0xA8C7AD, 0xA8CEDA, 0xCEAEE0, 0xE1ABA2, 0xE6C481, 0xAEC4E4, 0xD5AFC6,
    0xBECF91, 0x8BD2C6, 0xD8BAA3, 0xD0D7AD, 0x9DB9D6, 0xC5B4E4, 0xE2AEB5, 0xD7CB98,
    0xB2D0BE, 0xA4C9C4, 0xA8BDE2, 0xD4AFE0, 0xD7A68F, 0xD9BC76, 0x95C3A0, 0x8EC4D7,
    0xB5A3D2, 0xD39CC0, 0xCAB497, 0xC0CFA9, 0x97BCB5, 0xB2C7D1, 0xCCC3DC, 0xE0D6BF]
    .map(n => '#' + n.toString(16).padStart(6, '0'));

/** What the Elevation tool paints: a height (null clears the override) or a relative raise / lower by ½. */
export type HeightBrush = number | null | 'raise' | 'lower';
export const HEIGHT_CHOICES: {value: HeightBrush; label: string}[] = [
    {value: 'raise', label: 'Raise by ½'}, {value: 'lower', label: 'Lower by ½'},
    {value: null, label: 'Glyph default'}, {value: -2, label: '−2'}, {value: -1.5, label: '−1½'}, {value: -1, label: '−1'},
    {value: -0.5, label: '−½'}, {value: 0, label: '0'}, {value: 0.5, label: '+½'}, {value: 1, label: '+1'},
    {value: 1.5, label: '+1½'}, {value: 2, label: '+2'}, {value: 3, label: '+3'}, {value: 4, label: '+4'},
];

export const formatHour = (h: number) => {
    const whole = Math.floor(h), minutes = Math.round((h - whole) * 60);
    return `${String(whole).padStart(2, '0')}:${String(minutes).padStart(2, '0')}`;
};
