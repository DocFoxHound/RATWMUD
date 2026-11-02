// The ground round the cell (Docs/Design/70-the-realm.md, "Sight across seams"): a band of the cells beside it,
// outdoors at the same level, so the local map runs on over a seam. The server sends it as rows from the top
// (Core/RatwWorld.h, Fringe): the band's rows above and below the cell whole (band, cell, band wide), and each row
// beside the cell only its `band` tiles at each end.

/** Where tile (x, y), in the cell's own coordinates, lies in the band: [row, column], or null inside the cell or past it. */
export function bandSpot(band: number, width: number, height: number, x: number, y: number): [number, number] | null {
    if (band <= 0 || x < -band || y < -band || x >= width + band || y >= height + band) return null;
    if (y < 0) return [y + band, x + band];
    if (y >= height) return [band + height + (y - height), x + band];
    if (x >= 0 && x < width) return null;
    return [band + y, x < 0 ? x + band : band + x - width];
}

/** Whether `rows` are a band `band` wide round a `width` x `height` cell, as bandSpot reads them. */
export function bandFits(band: number, width: number, height: number, rows: readonly string[]): boolean {
    if (band <= 0 || rows.length !== height + 2 * band) return false;
    return rows.every((row, r) => row.length === (r < band || r >= band + height ? width + 2 * band : 2 * band));
}
