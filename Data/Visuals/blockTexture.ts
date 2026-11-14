/** Console-style character textures shared by the game and Atlas. Terrain codes still decide movement and sight. */
export const treeCodes = new Set(['P', 'Y', '$', '1', '2', '@']);
const roofCodes = new Set(['Z', 'L', 'Q', 'V', '[']);
const wallCodes = new Set(['#', 'H', 'M', ']', 'j']);
const openTextures = new Set(['.', 'd', '/', '_', 'f', ',', '"', '!', ';', 'r', 's', '*', '0', 'D', '-', '3', '5', '4']);

const hash = (x: number, y: number) => (Math.imul(x + 1013, 1597334677) ^ Math.imul(y + 2047, 3812015801)) >>> 0;

/** Each surface is made from printed characters, never a painted trunk, roof, or texture shape. */
export function paintBlockTile(c: CanvasRenderingContext2D, code: string, x: number, y: number, px: number, py: number,
                               tile: number, foreground: string, light = 1): boolean {
    if (!treeCodes.has(code) && !roofCodes.has(code) && !wallCodes.has(code) && !openTextures.has(code)) return false;
    c.save();
    c.globalAlpha *= 0.55 + light * 0.45;
    c.fillStyle = treeCodes.has(code) ? code === '2' ? '#ad9b7c' : '#b18b61' : foreground;
    c.textAlign = 'center'; c.textBaseline = 'middle';
    if (tile < 15) {
        c.font = Math.max(6, Math.floor(tile * .7)) + 'px monospace';
        const mark = treeCodes.has(code) ? 'T' : roofCodes.has(code) || wallCodes.has(code) ? '#' : code === '!' ? ':' : '.';
        c.fillText(mark, px + tile / 2, py + tile / 2);
        c.restore();
        return true;
    }
    c.font = Math.max(7, Math.floor(tile * .39)) + 'px monospace';
    let lines: [string, string];
    if (treeCodes.has(code)) lines = [' || ', '/||\\'];
    else if (roofCodes.has(code)) lines = code === 'Q' || code === 'V' ? ['====', '=++='] : ['////', '=/=/'];
    else if (code === 'j') lines = ['|[]|', '|__|'];
    else if (wallCodes.has(code)) lines = ['####', '#::#'];
    else if (code === '!') lines = hash(x, y) & 1 ? ['::::', ':.:.'] : [':.:.', '::::'];
    else if (code === ',' || code === '"' || code === ';' || code === '4') lines = hash(x, y) & 1 ? ["'  ,", ", ' "] : [",  '", "' , "];
    else if (code === 'd' || code === '.' || code === '/' || code === '-') lines = ['....', '. ..'];
    else lines = [' .  ', '  . '];
    c.fillText(lines[0], px + tile / 2, py + tile * .28);
    c.fillText(lines[1], px + tile / 2, py + tile * .73);
    c.restore();
    return true;
}

export interface TextureWindow {x0: number; y0: number; x1: number; y1: number}
export interface CoveredRect {left: number; top: number; right: number; bottom: number}

/** Character crowns and eaves appear above actors; bounds keep the covered wolf's outline visible. */
export function paintUpperTiles(c: CanvasRenderingContext2D, view: TextureWindow, ox: number, oy: number, tile: number,
                                at: (x: number, y: number) => string | undefined, light = 1): CoveredRect[] {
    if (tile < 15) return [];
    const covered: CoveredRect[] = [];
    c.save();
    c.globalAlpha *= .48 + light * .52;
    c.textAlign = 'center'; c.textBaseline = 'middle';
    c.font = Math.max(7, Math.floor(tile * .36)) + 'px monospace';
    for (let y = view.y0 - 2; y <= view.y1 + 1; ++y) for (let x = view.x0 - 2; x <= view.x1 + 2; ++x) {
        const code = at(x, y);
        if (code && treeCodes.has(code)) {
            const cx = ox + (x + .5) * tile, bottom = oy + (y + .12) * tile;
            const dead = code === '2', snowy = code === '1';
            c.fillStyle = dead ? '#ad9b7c' : snowy ? '#d4e5dc' : code === 'Y' || code === '@' ? '#8bad61' : '#6caa81';
            const crown = dead ? ['   +   ', ' --+-- ', '   +   ', '  / \\  ']
                : ['   ^   ', '  ^^^  ', ' ^^^^^ ', '^^^^^^^'];
            for (let row = 0; row < crown.length; ++row)
                c.fillText(crown[row], cx, bottom - tile * (1.5 - row * .38));
            covered.push({left: cx - tile * .75, right: cx + tile * .75,
                top: bottom - tile * 1.68, bottom: bottom - tile * .22});
        } else if (code && (roofCodes.has(code) || wallCodes.has(code)) && !roofCodes.has(at(x, y - 1) ?? '') && !wallCodes.has(at(x, y - 1) ?? '')) {
            const cx = ox + (x + .5) * tile, py = oy + y * tile;
            const roof = roofCodes.has(code) ? code : at(x, y + 1);
            const building = !!roof && roofCodes.has(roof);
            const reach = building ? .82 : .27;
            c.fillStyle = roof === 'L' ? '#c69a70' : roof === '[' ? '#d5785c' : roof === 'Z' ? '#a9bac1' : '#d1c8af';
            c.fillText(building ? '////' : '====', cx, py - tile * (building ? .65 : .16));
            if (building) c.fillText('====', cx, py - tile * .27);
            covered.push({left: cx - tile * .52, right: cx + tile * .52, top: py - tile * reach, bottom: py + tile * .08});
        }
    }
    c.restore();
    return covered;
}
