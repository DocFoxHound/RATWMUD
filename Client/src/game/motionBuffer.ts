// A short history of authoritative poses, not a client-side simulation (UI/RatwMotionBuffer.h). Drawing between
// samples keeps their velocity instead of repeatedly easing toward a target.

export interface TimedPose {
    time: number;
    x: number;
    y: number;
    facing: number;
}

export class MotionBuffer {
    samples: TimedPose[] = [];

    add(time: number, x: number, y: number, facing: number): boolean {
        if (![time, x, y, facing].every(Number.isFinite)) return false;
        const last = this.samples.at(-1);
        if (last && time <= last.time) return false;
        // A discontinuity must never animate through intervening walls or rooms.
        if (last && (time - last.time > 0.5 || Math.hypot(x - last.x, y - last.y) > 8)) this.samples = [];
        this.samples.push({time, x, y, facing});
        if (this.samples.length > 32) this.samples.splice(0, this.samples.length - 32);
        return true;
    }

    at(time: number): TimedPose {
        const s = this.samples;
        if (!s.length) return {time: 0, x: 0, y: 0, facing: 0};
        if (time <= s[0].time) return s[0];
        for (let i = 1; i < s.length; ++i)
            if (time < s[i].time) {
                const a = s[i - 1], b = s[i];
                const alpha = (time - a.time) / (b.time - a.time);
                const arc = Math.atan2(Math.sin(b.facing - a.facing), Math.cos(b.facing - a.facing));
                return {time, x: a.x + (b.x - a.x) * alpha, y: a.y + (b.y - a.y) * alpha, facing: a.facing + arc * alpha};
            }
        // No speculative extrapolation past an obstacle or out of visibility.
        return s[s.length - 1];
    }
}
