// Inflating and parsing the server's messages away from the page's own thread, so a large snapshot never costs a
// frame. Messages come back in the order they went in; the connection hands them on in that order.
import {decodeMessage, Kind} from './wire.ts';

export type Decoded =
    | {seq: number; kind: typeof Kind.Motion; raw: Uint8Array}
    | {seq: number; kind: typeof Kind.Event | typeof Kind.Snapshot; value: Record<string, unknown>; wireBytes: number}
    | {seq: number; kind: 0};               // Malformed: dropped, as the old client dropped it.

const decoder = new TextDecoder();

/** One message, decoded: the same on this worker and, where workers are missing, on the page. */
export function decode(seq: number, data: ArrayBuffer): Decoded {
    const arrival = decodeMessage(data);
    if (!arrival) return {seq, kind: 0};
    if (arrival.kind === Kind.Motion) return {seq, kind: Kind.Motion, raw: arrival.raw};
    let value: unknown;
    try {
        value = JSON.parse(decoder.decode(arrival.raw));
    } catch {
        return {seq, kind: 0};
    }
    if (typeof value !== 'object' || value === null || Array.isArray(value)) return {seq, kind: 0};
    return {seq, kind: arrival.kind, value: value as Record<string, unknown>, wireBytes: arrival.wireBytes};
}

interface WorkerScope {
    onmessage: ((e: MessageEvent<{seq: number; data: ArrayBuffer}>) => void) | null;
    postMessage(message: Decoded, transfer?: Transferable[]): void;
}
// Only when loaded as a worker (no document), never when imported by the page or the tests.
const scope = globalThis as unknown as WorkerScope & {document?: unknown; WorkerGlobalScope?: unknown};
if (typeof scope.WorkerGlobalScope !== 'undefined' && typeof scope.document === 'undefined')
    scope.onmessage = e => {
        const out = decode(e.data.seq, e.data.data);
        scope.postMessage(out, out.kind === Kind.Motion ? [out.raw.buffer as ArrayBuffer] : []);
    };
