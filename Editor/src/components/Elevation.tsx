// The elevation relief switch and its legend, floating over a map. Shared by Atlas and the Dungeon Master.
import {useState} from 'react';
import {ELEVATION_MODES, RELIEF, type ElevationMode} from '../lib/elevation';
import {setPlainAscii, useGlyphRender} from '../lib/glyphFont';

const NAMES: Record<ElevationMode, string> = {off: 'Off', shade: 'Shading', full: 'Full'};
const TIPS: Record<ElevationMode, string> = {
    off: 'No elevation shown',
    shade: 'Height tint and hillshade, with impassable ledges and cliffs',
    full: 'Shading plus every height change, walkable slopes and height numbers (zoom in for numbers)',
};

function Line({color, width, dash, hatch}: {color: string; width: number; dash?: string; hatch?: boolean}) {
    return <svg width="30" height="12" aria-hidden="true">
        {hatch && <>
            <rect x="4" y="0" width="22" height="12" fill="#3a2f27" />
            {[0, 6, 12, 18, 24].map(x => <line key={x} x1={x} y1="12" x2={x + 12} y2="0" stroke="rgba(255,122,69,.55)" strokeWidth="1" />)}
        </>}
        {!hatch && width > 2.5 && <line x1="2" y1="6" x2="28" y2="6" stroke="rgba(13,20,19,.8)" strokeWidth={width + 2} />}
        <line x1="2" y1={hatch ? 1.5 : 6} x2="28" y2={hatch ? 1.5 : 6} stroke={color} strokeWidth={width} strokeDasharray={dash} />
    </svg>;
}

/** Off / Shading / Full, what the strokes mean, and the Plain ASCII glyphs switch. */
export function ElevationControl({mode, onChange, shortcut}: {mode: ElevationMode; onChange: (m: ElevationMode) => void; shortcut?: string}) {
    const [legend, setLegend] = useState(true);
    const {ascii} = useGlyphRender();
    return <div className="relief-box">
        <div className="relief-head">
            <span className="relief-title" title={`Elevation relief${shortcut ? ` (${shortcut} cycles)` : ''}`}>⛰ Elevation</span>
            <div className="segmented" role="radiogroup" aria-label="Elevation relief">
                {ELEVATION_MODES.map(m => <button key={m} role="radio" aria-checked={mode === m} className={mode === m ? 'on' : ''}
                    title={TIPS[m] + (shortcut ? ` · ${shortcut} cycles` : '')} onClick={() => onChange(m)}>{NAMES[m]}</button>)}
            </div>
            {mode !== 'off' && <button className="relief-toggle" aria-expanded={legend} title={legend ? 'Hide the legend' : 'Show the legend'}
                onClick={() => setLegend(!legend)}>{legend ? '▾' : '▸'}</button>}
            <label className="relief-ascii" title="Draw each tile's plain ASCII fallback instead of its Unicode glyph, as the game's plain-text view does">
                <input type="checkbox" checked={ascii} onChange={e => setPlainAscii(e.target.checked)} /> Plain ASCII glyphs</label>
        </div>
        {mode !== 'off' && legend && <ul className="relief-legend">
            <li><span className="relief-ramp" style={{background: `linear-gradient(90deg, ${RELIEF.low}, #26312c 50%, ${RELIEF.high})`}} />
                <span>low → high (lit from the north-west)</span></li>
            <li><Line color={RELIEF.ledge} width={4} /><span>Ledge: impassable (drawn on the high side)</span></li>
            <li><Line color={RELIEF.cliff} width={3} hatch /><span><code>%</code> Cliff: solid rock face</span></li>
            {mode === 'full' && <>
                <li><Line color={RELIEF.slope} width={2} dash="5 3" /><span>1 step, walkable by <code>:</code> Slope or <code>^</code> Stairs</span></li>
                <li><Line color={RELIEF.step} width={1} /><span>½ step: walkable</span></li>
                <li><span className="relief-num">1½</span><span>height where it changes (zoom in)</span></li>
            </>}
        </ul>}
    </div>;
}
