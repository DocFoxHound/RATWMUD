// The game's wire over a WebSocket (Docs/Design/27-browser-client.md; the server's side is Core/RatwWeb.h and
// RatwLink.h). Each message is a kind byte, then its payload:
//
//   to the server:   Command (a command's JSON), Ack (f64 revision, u8 missing: see sections.ts), Ping (8 bytes)
//   from the server: Event, Snapshot (u32 raw length, then JSON, zlib-compressed), Motion (the same around a binary
//                    frame: see motion.ts), Pong (the Ping's 8 bytes back, not compressed: answered by the server
//                    itself, for the latency overlay of Docs/Design/31-responsiveness.md)
import {inflate} from './inflate.ts';

export const Kind = {Command: 1, Ack: 2, Ping: 3, Pose: 4, Event: 10, Snapshot: 11, Motion: 12, Pong: 13} as const;
export type ServerKind = typeof Kind.Event | typeof Kind.Snapshot | typeof Kind.Motion;
export const MaxCommand = 65536;
export const MaxRaw = 16 << 20;
/** The raw length's top bit: the payload is the raw bytes themselves, not compressed (small messages: doc 31, Phase 4). */
export const Stored = 0x80000000;

const encoder = new TextEncoder();

/** A command message, or null if its JSON is too large to send. */
export function commandMessage(json: string): Uint8Array | null {
    const text = encoder.encode(json);
    if (text.length > MaxCommand) return null;
    const out = new Uint8Array(text.length + 1);
    out[0] = Kind.Command;
    out.set(text, 1);
    return out;
}

export function ackMessage(revision: number, missing: boolean): Uint8Array {
    const out = new Uint8Array(10);
    out[0] = Kind.Ack;
    new DataView(out.buffer).setFloat64(1, revision, true);
    out[9] = missing ? 1 : 0;
    return out;
}

/** Where the page's own wolf is (doc 31, Phase 3/4): u32 seq, f32 x, f32 y, f32 facing, i8 heading x, i8 heading y. */
export function poseMessage(seq: number, x: number, y: number, facing: number, ix: number, iy: number): Uint8Array {
    const out = new Uint8Array(19);
    const view = new DataView(out.buffer);
    out[0] = Kind.Pose;
    view.setUint32(1, seq >>> 0, true);
    view.setFloat32(5, x, true);
    view.setFloat32(9, y, true);
    view.setFloat32(13, facing, true);
    view.setInt8(17, Math.sign(ix));
    view.setInt8(18, Math.sign(iy));
    return out;
}

/** A ping carrying a time (ms, the page's clock); the server sends it straight back as a Pong. */
export function pingMessage(at: number): Uint8Array {
    const out = new Uint8Array(9);
    out[0] = Kind.Ping;
    new DataView(out.buffer).setFloat64(1, at, true);
    return out;
}

/** The time a Pong carries back, or null if this message isn't one. */
export function pongTime(data: ArrayBuffer | Uint8Array): number | null {
    const bytes = data instanceof Uint8Array ? data : new Uint8Array(data);
    if (bytes.length !== 9 || bytes[0] !== Kind.Pong) return null;
    return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength).getFloat64(1, true);
}

export interface Arrival {
    kind: ServerKind;
    raw: Uint8Array;         // JSON text (UTF-8) for events and snapshots; the binary frame for motion.
    wireBytes: number;       // What it took on the wire.
}

/** A message from the server, inflated; null for anything malformed (it is then ignored, as the old client did). */
export function decodeMessage(data: ArrayBuffer | Uint8Array): Arrival | null {
    const bytes = data instanceof Uint8Array ? data : new Uint8Array(data);
    if (bytes.length < 6) return null;
    const kind = bytes[0];
    if (kind !== Kind.Event && kind !== Kind.Snapshot && kind !== Kind.Motion) return null;
    const length = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength).getUint32(1, true);
    if (length >= Stored) {
        const raw = bytes.subarray(5);
        return raw.length === length - Stored && raw.length > 0 ? {kind: kind as ServerKind, raw, wireBytes: bytes.length} : null;
    }
    const rawLength = length;
    if (rawLength === 0 || rawLength > MaxRaw) return null;
    try {
        return {kind: kind as ServerKind, raw: inflate(bytes.subarray(5), rawLength), wireBytes: bytes.length};
    } catch {
        return null;
    }
}

/** A server message as the server makes it: for tests and the headless client's own checks. */
export async function encodeMessage(kind: ServerKind, raw: Uint8Array): Promise<Uint8Array> {
    const stream = new Blob([raw as BlobPart]).stream().pipeThrough(new CompressionStream('deflate'));
    const packed = new Uint8Array(await new Response(stream).arrayBuffer());
    const out = new Uint8Array(5 + packed.length);
    out[0] = kind;
    new DataView(out.buffer).setUint32(1, raw.length, true);
    out.set(packed, 5);
    return out;
}
