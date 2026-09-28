// zlib decompression (RFC 1950 around RFC 1951), synchronous. The browser's DecompressionStream works, but every
// message then takes several turns of the event loop; at twenty-five messages a second that fell behind. This is the
// classic small inflater (after tinf): Huffman tables built per block, a sliding output that is the whole message.

class Bits {
    at = 0;
    private bit = 0;
    private bits = 0;
    private data: Uint8Array;
    constructor(data: Uint8Array, at: number) {
        this.data = data;
        this.at = at;
    }
    read(n: number): number {
        while (this.bits < n) {
            if (this.at >= this.data.length) throw new Error('inflate: out of data');
            this.bit |= this.data[this.at++] << this.bits;
            this.bits += 8;
        }
        const v = this.bit & ((1 << n) - 1);
        this.bit >>>= n;
        this.bits -= n;
        return v;
    }
    align() {
        this.bit = 0;
        this.bits = 0;
    }
}

interface Tree {
    counts: Uint16Array;       // Codes of each length.
    symbols: Uint16Array;      // Symbols by code.
}

function build(lengths: Uint8Array, offset: number, count: number): Tree {
    const counts = new Uint16Array(16), symbols = new Uint16Array(count), offsets = new Uint16Array(16);
    for (let i = 0; i < count; ++i) counts[lengths[offset + i]]++;
    counts[0] = 0;
    for (let i = 0, sum = 0; i < 16; ++i) {
        offsets[i] = sum;
        sum += counts[i];
    }
    for (let i = 0; i < count; ++i) if (lengths[offset + i]) symbols[offsets[lengths[offset + i]]++] = i;
    return {counts, symbols};
}

function decode(bits: Bits, tree: Tree): number {
    let code = 0, first = 0, index = 0;
    for (let length = 1; length < 16; ++length) {
        code |= bits.read(1);
        const count = tree.counts[length];
        if (code - first < count) return tree.symbols[index + code - first];
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    throw new Error('inflate: bad code');
}

const LengthBase = [3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258];
const LengthExtra = [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0];
const DistBase = [1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577];
const DistExtra = [0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13];
const CodeOrder = [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15];

const fixed = (() => {
    const lengths = new Uint8Array(288 + 30);
    lengths.fill(8, 0, 144);
    lengths.fill(9, 144, 256);
    lengths.fill(7, 256, 280);
    lengths.fill(8, 280, 288);
    lengths.fill(5, 288, 318);
    return {literals: build(lengths, 0, 288), distances: build(lengths, 288, 30)};
})();

function dynamicTrees(bits: Bits) {
    const hlit = bits.read(5) + 257, hdist = bits.read(5) + 1, hclen = bits.read(4) + 4;
    const lengths = new Uint8Array(288 + 32);
    for (let i = 0; i < hclen; ++i) lengths[CodeOrder[i]] = bits.read(3);
    const codeTree = build(lengths, 0, 19);
    lengths.fill(0, 0, 19);
    for (let n = 0; n < hlit + hdist;) {
        const symbol = decode(bits, codeTree);
        if (symbol < 16) lengths[n++] = symbol;
        else {
            let repeat: number, value = 0;
            if (symbol === 16) {
                if (n === 0) throw new Error('inflate: repeat of nothing');
                value = lengths[n - 1];
                repeat = 3 + bits.read(2);
            } else if (symbol === 17) repeat = 3 + bits.read(3);
            else repeat = 11 + bits.read(7);
            if (n + repeat > hlit + hdist) throw new Error('inflate: too many lengths');
            lengths.fill(value, n, n + repeat);
            n += repeat;
        }
    }
    return {literals: build(lengths, 0, hlit), distances: build(lengths, hlit, hdist)};
}

/** Inflates zlib data whose inflated size is known (the server says it). Throws if it is malformed or the wrong size. */
export function inflate(data: Uint8Array, size: number): Uint8Array {
    if (data.length < 6 || (data[0] & 0x0f) !== 8 || ((data[0] << 8) | data[1]) % 31 !== 0 || data[1] & 0x20)
        throw new Error('inflate: not zlib');
    const out = new Uint8Array(size);
    let written = 0;
    const bits = new Bits(data, 2);
    for (let last = 0; !last;) {
        last = bits.read(1);
        const type = bits.read(2);
        if (type === 0) {
            bits.align();
            const at = bits.at;
            if (at + 4 > data.length) throw new Error('inflate: out of data');
            const length = data[at] | (data[at + 1] << 8), check = data[at + 2] | (data[at + 3] << 8);
            if ((length ^ 0xffff) !== check || at + 4 + length > data.length || written + length > size) throw new Error('inflate: bad stored block');
            out.set(data.subarray(at + 4, at + 4 + length), written);
            written += length;
            bits.at = at + 4 + length;
            continue;
        }
        if (type === 3) throw new Error('inflate: bad block');
        const {literals, distances} = type === 1 ? fixed : dynamicTrees(bits);
        for (;;) {
            const symbol = decode(bits, literals);
            if (symbol < 256) {
                if (written >= size) throw new Error('inflate: too long');
                out[written++] = symbol;
                continue;
            }
            if (symbol === 256) break;
            const l = symbol - 257;
            if (l >= 29) throw new Error('inflate: bad length');
            const length = LengthBase[l] + bits.read(LengthExtra[l]);
            const d = decode(bits, distances);
            if (d >= 30) throw new Error('inflate: bad distance');
            const distance = DistBase[d] + bits.read(DistExtra[d]);
            if (distance > written || written + length > size) throw new Error('inflate: bad copy');
            for (let i = 0; i < length; ++i, ++written) out[written] = out[written - distance];
        }
    }
    if (written !== size) throw new Error('inflate: wrong size');
    // The Adler-32 of what was inflated closes the stream.
    let a = 1, b = 0;
    for (let i = 0; i < size; ) {
        const end = Math.min(size, i + 3800);
        for (; i < end; ++i) {
            a += out[i];
            b += a;
        }
        a %= 65521;
        b %= 65521;
    }
    const at = bits.at;
    if (at + 4 > data.length) throw new Error('inflate: no checksum');
    const adler = ((data[at] << 24) | (data[at + 1] << 16) | (data[at + 2] << 8) | data[at + 3]) >>> 0;
    if (adler !== (((b << 16) | a) >>> 0)) throw new Error('inflate: checksum');
    return out;
}
