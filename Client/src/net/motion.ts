// Motion frames: the twenty-a-second poses, binary on the wire (Core/RatwMotionCore.h). Little-endian:
//
//   u32 magic "RMT1", string session, string observer, string cell, i32 cell generation, f64 revision, f64 time,
//   i32 count, then per pose: string id, f32 x, f32 y, f32 facing, u8 moving; then the observer's own movement
//   (Docs/Design/31-responsiveness.md, Phase 3): u8 mode (0 free, 1 held, 2 fighting), u32 the last held input
//   applied, u32 the last pose of its own accepted, u8 partial (far wolves left out: Phase 4.3)
//
// A string is an i32 length, then: positive, that many bytes with a final 0 (Latin-1); negative, that many UTF-16
// units with a final 0; zero, the empty string.

export interface Pose {
    id: string;
    x: number;
    y: number;
    facing: number;
    moving: boolean;
}
export interface MotionFrame {
    motionSession: string;
    observer: string;
    cellId: string;
    cellGeneration: number;
    revision: number;
    time: number;
    entities: Pose[];
    // The observer's own movement; absent in frames made by tests, read as free with nothing acknowledged.
    mode?: number;
    inputAck?: number;
    poseAck?: number;
    // Wolves farther than FarAway were left out (they come in every fourth frame): keep the ones not in it.
    partial?: boolean;
}

/** As the server's motion::FarAway (tiles). */
export const FarAway = 24;

const Magic = 0x31544d52;
const MaxPoses = 4096;
const MaxString = 4096;

class Reader {
    at = 0;
    bad = false;
    private view: DataView;
    private bytes: Uint8Array;
    constructor(bytes: Uint8Array) {
        this.bytes = bytes;
        this.view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    }
    private need(n: number): boolean {
        if (this.bad || this.at + n > this.bytes.length) {
            this.bad = true;
            return false;
        }
        return true;
    }
    u8() { if (!this.need(1)) return 0; return this.view.getUint8(this.at++); }
    u32() { if (!this.need(4)) return 0; const v = this.view.getUint32(this.at, true); this.at += 4; return v; }
    i32() { if (!this.need(4)) return 0; const v = this.view.getInt32(this.at, true); this.at += 4; return v; }
    f32() { if (!this.need(4)) return 0; const v = this.view.getFloat32(this.at, true); this.at += 4; return v; }
    f64() { if (!this.need(8)) return 0; const v = this.view.getFloat64(this.at, true); this.at += 8; return v; }
    string(): string {
        const n = this.i32();
        if (this.bad || n === 0) return '';
        if (n > 0) {
            if (n > MaxString || !this.need(n)) { this.bad = true; return ''; }
            let s = '';
            for (let i = 0; i < n - 1; ++i) s += String.fromCharCode(this.bytes[this.at + i]);
            this.at += n;
            return s;
        }
        const units = -n;
        if (units > MaxString || !this.need(units * 2)) { this.bad = true; return ''; }
        let s = '';
        for (let i = 0; i < units - 1; ++i) s += String.fromCharCode(this.view.getUint16(this.at + i * 2, true));
        this.at += units * 2;
        return s;
    }
}

/** Null for anything that isn't a whole, well-formed frame. */
export function unpack(bytes: Uint8Array): MotionFrame | null {
    const r = new Reader(bytes);
    if (r.u32() !== Magic) return null;
    const frame: MotionFrame = {
        motionSession: r.string(), observer: r.string(), cellId: r.string(), cellGeneration: r.i32(),
        revision: r.f64(), time: r.f64(), entities: [], mode: 0, inputAck: 0, poseAck: 0,
    };
    const count = r.i32();
    if (r.bad || count < 0 || count > MaxPoses) return null;
    for (let i = 0; i < count; ++i) {
        const pose = {id: r.string(), x: r.f32(), y: r.f32(), facing: r.f32(), moving: r.u8() !== 0};
        if (r.bad) return null;
        frame.entities.push(pose);
    }
    frame.mode = r.u8();
    frame.inputAck = r.u32();
    frame.poseAck = r.u32();
    frame.partial = r.u8() !== 0;
    return r.bad || r.at !== bytes.length ? null : frame;
}

/** As the server packs a frame: for tests. */
export function pack(frame: MotionFrame): Uint8Array {
    const parts: number[] = [];
    const view = new DataView(new ArrayBuffer(8));
    const push = (n: number) => { for (let i = 0; i < n; ++i) parts.push(view.getUint8(i)); };
    const u32 = (v: number) => { view.setUint32(0, v, true); push(4); };
    const i32 = (v: number) => { view.setInt32(0, v, true); push(4); };
    const f32 = (v: number) => { view.setFloat32(0, v, true); push(4); };
    const f64 = (v: number) => { view.setFloat64(0, v, true); push(8); };
    const string = (s: string) => {
        if (!s) return i32(0);
        if ([...s].every(c => c.charCodeAt(0) < 0x80)) {
            i32(s.length + 1);
            for (const c of s) parts.push(c.charCodeAt(0));
            parts.push(0);
        } else {
            i32(-(s.length + 1));
            for (let i = 0; i < s.length; ++i) { view.setUint16(0, s.charCodeAt(i), true); push(2); }
            parts.push(0, 0);
        }
    };
    u32(Magic);
    string(frame.motionSession); string(frame.observer); string(frame.cellId);
    i32(frame.cellGeneration); f64(frame.revision); f64(frame.time); i32(frame.entities.length);
    for (const p of frame.entities) { string(p.id); f32(p.x); f32(p.y); f32(p.facing); parts.push(p.moving ? 1 : 0); }
    parts.push(frame.mode ?? 0); u32(frame.inputAck ?? 0); u32(frame.poseAck ?? 0); parts.push(frame.partial ? 1 : 0);
    return Uint8Array.from(parts);
}
