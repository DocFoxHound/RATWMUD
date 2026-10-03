// Packed snapshots (Docs/Design/31-responsiveness.md, Phase 4.8; the server's side is Core/RatwPack.h): the same tree
// of values as a snapshot's JSON, in a compact binary form, read here into the very same objects.
//
//   tag 0 null, 1 false, 2 true, 3 whole number (zigzag varint), 4 float32, 5 float64, 6 string,
//       7 array (varint count, values), 8 object (varint count, then key and value for each field)
//   string: varint n; even: n/2 bytes of UTF-8, then the next number in the message's table; odd: table entry (n-1)/2

const decoder = new TextDecoder();

/** The value, or undefined for anything malformed. */
export function unpackValue(bytes: Uint8Array): unknown {
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    const table: string[] = [];
    let at = 0;
    const fail = (): never => { throw new Error('malformed'); };
    const varint = (): number => {
        let n = 0, scale = 1;
        for (let i = 0; i < 8; ++i) {
            if (at >= bytes.length) fail();
            const b = bytes[at++];
            n += (b & 0x7f) * scale;
            if (!(b & 0x80)) return n;
            scale *= 128;
        }
        return fail();
    };
    const text = (): string => {
        const n = varint();
        if (n % 2 === 1) {
            const s = table[(n - 1) / 2];
            return s === undefined ? fail() : s;
        }
        const length = n / 2;
        if (at + length > bytes.length) fail();
        const s = decoder.decode(bytes.subarray(at, at + length));
        at += length;
        table.push(s);
        return s;
    };
    const value = (depth: number): unknown => {
        if (depth > 256 || at >= bytes.length) fail();
        switch (bytes[at++]) {
        case 0: return null;
        case 1: return false;
        case 2: return true;
        case 3: {
            const z = varint();
            return z % 2 === 0 ? z / 2 : -(z + 1) / 2;
        }
        case 4: {
            if (at + 4 > bytes.length) fail();
            const f = view.getFloat32(at, true);
            at += 4;
            return f;
        }
        case 5: {
            if (at + 8 > bytes.length) fail();
            const d = view.getFloat64(at, true);
            at += 8;
            return d;
        }
        case 6: return text();
        case 7: {
            const n = varint();
            if (n > bytes.length - at) fail();
            const out: unknown[] = new Array(n);
            for (let i = 0; i < n; ++i) out[i] = value(depth + 1);
            return out;
        }
        case 8: {
            const n = varint();
            if (n > bytes.length - at) fail();
            const out: Record<string, unknown> = {};
            for (let i = 0; i < n; ++i) {
                const key = text();
                out[key] = value(depth + 1);
            }
            return out;
        }
        default: return fail();
        }
    };
    try {
        const out = value(0);
        return at === bytes.length ? out : undefined;
    } catch {
        return undefined;
    }
}
