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

    /**
     * The pose at `time`. Past the newest sample it holds still, unless `ahead` allows carrying on at the last
     * measured velocity for that many seconds at most (only for the player's own wolf, while it is being moved).
     */
    at(time: number, ahead = 0): TimedPose {
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
        // Other wolves are never extrapolated past an obstacle or out of visibility.
        const last = s[s.length - 1];
        if (ahead <= 0 || s.length < 2) return last;
        const before = s[s.length - 2], span = last.time - before.time;
        if (span <= 0 || span > 0.25) return last;
        const t = Math.min(time - last.time, ahead) / span;
        return {time, x: last.x + (last.x - before.x) * t, y: last.y + (last.y - before.y) * t, facing: last.facing};
    }
}
