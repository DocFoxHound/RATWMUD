// The terrain palette: a quick strip of the hotkey tiles, and every catalog tile grouped by category in a panel
// with a filter. Choosing a tile stores its one-character code; the swatch shows its glyph as the map draws it.
import {useEffect, useRef, useState} from 'react';
import {GLYPH_GROUPS, QUICK_GLYPHS, glyphInfo, glyphTitle, matchesGlyph, type GlyphInfo} from '../lib/glyphs';
import {drawnGlyph, useGlyphRender} from '../lib/glyphFont';
import {setState, useStore} from '../lib/store';

const COLLAPSED_KEY = 'ratw-terrain-collapsed';
const loadCollapsed = (): string[] => {
    try { const v: unknown = JSON.parse(localStorage.getItem(COLLAPSED_KEY) ?? '[]'); return Array.isArray(v) ? v.filter(x => typeof x === 'string') : []; }
    catch { return []; }
};

function Swatch({tile, on, ascii}: {tile: GlyphInfo; on: boolean; ascii: boolean}) {
    return <button role="radio" aria-checked={on} className={on ? 'on' : ''} title={glyphTitle(tile)} aria-label={tile.name}
        style={{color: tile.fg, background: tile.bg}} onClick={() => setState({glyph: tile.code})}>
        <span className="tile-glyph">{drawnGlyph(tile, ascii)}</span></button>;
}

export function TerrainPalette() {
    const code = useStore(s => s.glyph);
    const {ascii} = useGlyphRender();
    const current = glyphInfo(code);
    const [open, setOpen] = useState(false);
    const [query, setQuery] = useState('');
    const [collapsed, setCollapsed] = useState<string[]>(loadCollapsed);
    const box = useRef<HTMLDivElement>(null);
    useEffect(() => {
        if (!open) return;
        const outside = (e: PointerEvent) => { if (!box.current?.contains(e.target as Node)) setOpen(false); };
        document.addEventListener('pointerdown', outside);
        return () => document.removeEventListener('pointerdown', outside);
    }, [open]);
    const toggle = (id: string) => setCollapsed(list => {
        const next = list.includes(id) ? list.filter(x => x !== id) : [...list, id];
        try { localStorage.setItem(COLLAPSED_KEY, JSON.stringify(next)); } catch { /* private window */ }
        return next;
    });
    const groups = GLYPH_GROUPS.map(g => ({...g, tiles: g.tiles.filter(t => matchesGlyph(t, query))})).filter(g => g.tiles.length);
    const choose = (tile: GlyphInfo) => { setState({glyph: tile.code}); setOpen(false); setQuery(''); };
    return <div className="glyphs" ref={box} role="radiogroup" aria-label="Terrain">
        {QUICK_GLYPHS.map(t => <Swatch key={t.code} tile={t} on={t.code === current.code} ascii={ascii} />)}
        {!QUICK_GLYPHS.includes(current) && <><span className="glyph-sep" /><Swatch tile={current} on ascii={ascii} /></>}
        <button className="terrain-more" aria-expanded={open} aria-haspopup="dialog" title="Every terrain tile, by category"
            onClick={() => setOpen(!open)}>All terrain {open ? '▴' : '▾'}</button>
        <span className="glyph-name" title={current.effect}>{current.name} <code>{current.code}</code></span>
        {open && <div className="terrain-pop" role="dialog" aria-label="All terrain"
            onKeyDown={e => { if (e.key === 'Escape') { e.stopPropagation(); setOpen(false); } }}>
            <input autoFocus type="search" value={query} placeholder="Filter terrain: name, effect or code…" aria-label="Filter terrain"
                onChange={e => setQuery(e.target.value)}
                onKeyDown={e => { if (e.key === 'Enter' && groups[0]) choose(groups[0].tiles[0]); }} />
            <div className="terrain-groups">
                {groups.map(g => {
                    const shut = !query && collapsed.includes(g.id);
                    return <section key={g.id} className="terrain-group">
                        <button className="terrain-head" aria-expanded={!shut} onClick={() => toggle(g.id)}>
                            <span>{shut ? '▸' : '▾'} {g.name}</span><small>{g.tiles.length}</small></button>
                        {!shut && <div className="terrain-list">{g.tiles.map(t =>
                            <button key={t.code} className={t.code === current.code ? 'terrain-item on' : 'terrain-item'} title={glyphTitle(t)}
                                aria-pressed={t.code === current.code} onClick={() => choose(t)}>
                                <span className="tile-glyph" style={{color: t.fg, background: t.bg}}>{drawnGlyph(t, ascii)}</span>
                                <span className="terrain-label">{t.name}</span>
                                {t.key ? <kbd>{t.key}</kbd> : <code>{t.code}</code>}
                            </button>)}</div>}
                    </section>;
                })}
                {!groups.length && <p className="hint">No terrain matches “{query}”.</p>}
            </div>
        </div>}
    </div>;
}
