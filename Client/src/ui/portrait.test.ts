// The portrait sheets (Data/Portraits) as portrait.ts expects them: one per species, an 8-bit RGBA PNG of four equal
// life-stage frames in a 2×2 grid, each a sprite with opaque pixels on a transparent ground.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync, statSync} from 'node:fs';
import {inflateSync} from 'node:zlib';
import {readAppearance} from './portrait.ts';

const species = ['timber', 'maned', 'arctic', 'red', 'ethiopian'];
const sheet = (name: string) => new URL(`../../../Data/Portraits/${name}.png`, import.meta.url);
// The generated art never quite reaches 255 (its sprites are 251-254): near enough is opaque.
const Opaque = 250;
const Signature = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];

interface Png {
    width: number;
    height: number;
    bitDepth: number;
    colorType: number;
    interlace: number;
    data: Uint8Array;      // The concatenated IDAT chunks.
}

function readPng(bytes: Uint8Array): Png {
    assert.deepEqual([...bytes.subarray(0, 8)], Signature, 'a PNG signature');
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    let at = 8, ihdr: DataView | null = null;
    const idat: Uint8Array[] = [];
    for (;;) {
        assert.ok(at + 12 <= bytes.length, 'chunks up to IEND');
        const length = view.getUint32(at), type = String.fromCharCode(...bytes.subarray(at + 4, at + 8));
        const body = bytes.subarray(at + 8, at + 8 + length);
        if (at === 8) assert.equal(type, 'IHDR', 'IHDR first');
        if (type === 'IHDR') ihdr = new DataView(body.buffer, body.byteOffset, body.byteLength);
        if (type === 'IDAT') idat.push(body);
        at += 12 + length;
        if (type === 'IEND') break;
    }
    assert.ok(ihdr);
    const data = new Uint8Array(idat.reduce((n, c) => n + c.length, 0));
    let offset = 0;
    for (const c of idat) { data.set(c, offset); offset += c.length; }
    return {width: ihdr.getUint32(0), height: ihdr.getUint32(4), bitDepth: ihdr.getUint8(8), colorType: ihdr.getUint8(9),
        interlace: ihdr.getUint8(12), data};
}

/** 8-bit RGBA, not interlaced: the scanlines unfiltered. */
function pixels(png: Png): Uint8Array {
    const raw = inflateSync(png.data), stride = png.width * 4, out = new Uint8Array(stride * png.height);
    assert.equal(raw.length, (stride + 1) * png.height, 'one filter byte and one row per scanline');
    for (let y = 0; y < png.height; ++y) {
        const filter = raw[y * (stride + 1)], line = y * (stride + 1) + 1, row = y * stride, up = row - stride;
        for (let i = 0; i < stride; ++i) {
            const a = i >= 4 ? out[row + i - 4] : 0, b = y > 0 ? out[up + i] : 0, c = i >= 4 && y > 0 ? out[up + i - 4] : 0;
            let predictor = 0;
            if (filter === 1) predictor = a;
            else if (filter === 2) predictor = b;
            else if (filter === 3) predictor = (a + b) >> 1;
            else if (filter === 4) {
                const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
                predictor = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
            } else assert.equal(filter, 0, `a known filter on row ${y}`);
            out[row + i] = (raw[line + i] + predictor) & 255;
        }
    }
    return out;
}

test('every species the loader accepts has its sheet', () => {
    for (const name of species) {
        assert.ok(readAppearance({species: name, sex: 'female', stature: 'average', pattern: 'solid', baseColor: 0, gradientColor: 0,
            markingColor: 0, gradientAmount: 0, patternAmount: 0}), `${name} is a species`);
        assert.ok(statSync(sheet(name)).isFile(), `${name}.png exists`);
    }
});

for (const name of species)
    test(`${name}.png is an 8-bit RGBA sheet of four sprite frames`, () => {
        assert.ok(statSync(sheet(name)).size < 16 * 1024 * 1024, 'under 16 MiB');
        const png = readPng(new Uint8Array(readFileSync(sheet(name))));
        assert.equal(png.bitDepth, 8, '8 bits a channel');
        assert.equal(png.colorType, 6, 'RGBA');
        assert.equal(png.interlace, 0, 'not interlaced');
        for (const side of [png.width, png.height]) {
            assert.ok(side >= 4 && side <= 4096, `a size within reason (${side})`);
            assert.equal(side % 2, 0, 'even, for four equal frames');
        }
        const rgba = pixels(png), w = png.width / 2, h = png.height / 2;
        for (const [fx, fy] of [[0, 0], [w, 0], [0, h], [w, h]]) {
            let opaque = 0, clear = 0;
            for (let y = fy; y < fy + h; ++y)
                for (let x = fx; x < fx + w; ++x) {
                    const alpha = rgba[(y * png.width + x) * 4 + 3];
                    if (alpha >= Opaque) ++opaque;
                    else if (alpha === 0) ++clear;
                }
            assert.ok(opaque > 0 && clear > 0, `the frame at ${fx},${fy} has a sprite on a clear ground (${opaque} opaque, ${clear} clear)`);
        }
    });
