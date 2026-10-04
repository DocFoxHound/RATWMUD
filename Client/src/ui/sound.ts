// The fight's sounds (Docs/Design/37-combat-feel.md, phase 3), the game's first: short and soft, made in the browser
// from noise and tones as they are needed, so there are no sound files to fetch. A bite is a snap and a thump; a sword's
// cut a swish; a miss a whiff; fire a roar that swells and fades; a fall a low thud; one's own turn a two-note chime.
// Volume 0 is silent (Settings: Combat sound). Browsers start sound only after the player has done something on the
// page; until then nothing plays, and nothing fails.

type Ctx = AudioContext;

export class Sound {
    private ctx: Ctx | null = null;
    private noise: AudioBuffer | null = null;
    private out: GainNode | null = null;
    private last = new Map<string, number>();           // When each cue last played: a burst of the same is played once.

    /** Plays a cue (a fight line's kind: see fightFx.ts Cues) at a volume 0..1. */
    play(cue: string, volume: number) {
        if (volume <= 0) return;
        const ctx = this.context();
        if (!ctx || !this.out) return;
        const now = ctx.currentTime;
        if (now - (this.last.get(cue) ?? -1) < 0.06) return;
        this.last.set(cue, now);
        this.out.gain.setValueAtTime(volume * 0.8, now);
        const recipe = Recipes[cue];
        if (recipe) recipe(this, ctx, now);
    }

    private context(): Ctx | null {
        if (this.ctx) {
            if (this.ctx.state === 'suspended') void this.ctx.resume().catch(() => undefined);
            return this.ctx;
        }
        const Make = (globalThis as {AudioContext?: typeof AudioContext; webkitAudioContext?: typeof AudioContext}).AudioContext ??
            (globalThis as {webkitAudioContext?: typeof AudioContext}).webkitAudioContext;
        if (!Make) return null;
        try {
            this.ctx = new Make();
        } catch {
            return null;
        }
        this.out = this.ctx.createGain();
        this.out.connect(this.ctx.destination);
        // A second of white noise, shared by everything that hisses.
        const length = this.ctx.sampleRate;
        this.noise = this.ctx.createBuffer(1, length, this.ctx.sampleRate);
        const data = this.noise.getChannelData(0);
        for (let i = 0; i < length; ++i) data[i] = Math.random() * 2 - 1;
        return this.ctx;
    }

    /** Noise through a filter, shaped by an envelope: `freq` may sweep from its first value to its second. */
    hiss(ctx: Ctx, at: number, length: number, peak: number, type: BiquadFilterType, freq: [number, number], q = 1, attack = 0.005) {
        if (!this.noise || !this.out) return;
        const src = ctx.createBufferSource();
        src.buffer = this.noise;
        const filter = ctx.createBiquadFilter();
        filter.type = type;
        filter.Q.value = q;
        filter.frequency.setValueAtTime(freq[0], at);
        filter.frequency.exponentialRampToValueAtTime(Math.max(20, freq[1]), at + length);
        const gain = ctx.createGain();
        gain.gain.setValueAtTime(0.0001, at);
        gain.gain.exponentialRampToValueAtTime(peak, at + attack);
        gain.gain.exponentialRampToValueAtTime(0.0001, at + length);
        src.connect(filter).connect(gain).connect(this.out);
        src.start(at, Math.random() * 0.5);
        src.stop(at + length + 0.05);
    }

    /** A tone, its pitch sliding from `from` to `to`, shaped by an envelope. */
    tone(ctx: Ctx, at: number, length: number, peak: number, from: number, to: number, type: OscillatorType = 'sine', attack = 0.005) {
        if (!this.out) return;
        const osc = ctx.createOscillator();
        osc.type = type;
        osc.frequency.setValueAtTime(from, at);
        osc.frequency.exponentialRampToValueAtTime(Math.max(20, to), at + length);
        const gain = ctx.createGain();
        gain.gain.setValueAtTime(0.0001, at);
        gain.gain.exponentialRampToValueAtTime(peak, at + attack);
        gain.gain.exponentialRampToValueAtTime(0.0001, at + length);
        osc.connect(gain).connect(this.out);
        osc.start(at);
        osc.stop(at + length + 0.05);
    }
}

const Recipes: Record<string, (s: Sound, ctx: Ctx, at: number) => void> = {
    // A snap of teeth and a thump of the body.
    bite: (s, ctx, at) => {
        s.hiss(ctx, at + 0.05, 0.07, 0.5, 'bandpass', [2400, 1400], 1.4);
        s.tone(ctx, at + 0.06, 0.12, 0.45, 150, 70);
    },
    graze: (s, ctx, at) => s.hiss(ctx, at + 0.05, 0.08, 0.28, 'bandpass', [2800, 1800], 1.2),
    // A blade through the air, and the cut.
    cut: (s, ctx, at) => {
        s.hiss(ctx, at, 0.16, 0.35, 'bandpass', [5200, 1600], 2.5, 0.03);
        s.hiss(ctx, at + 0.09, 0.08, 0.4, 'highpass', [2600, 2000], 0.8);
        s.tone(ctx, at + 0.1, 0.1, 0.25, 180, 90);
    },
    // A whiff: air, nothing struck.
    miss: (s, ctx, at) => s.hiss(ctx, at, 0.16, 0.18, 'bandpass', [800, 2600], 1.6, 0.04),
    // Fire: a roar that swells and fades, with crackle.
    fire: (s, ctx, at) => {
        s.hiss(ctx, at, 1.0, 0.45, 'lowpass', [500, 2200], 0.7, 0.12);
        s.hiss(ctx, at + 0.15, 0.85, 0.3, 'bandpass', [1400, 600], 1.0, 0.1);
        for (let i = 0; i < 6; ++i) s.hiss(ctx, at + 0.1 + i * 0.13 + Math.random() * 0.05, 0.03, 0.25, 'highpass', [3000, 3000], 0.7, 0.002);
    },
    // Heat gathering: a low rising hum.
    charge: (s, ctx, at) => s.tone(ctx, at, 0.7, 0.12, 110, 260, 'triangle', 0.2),
    burn: (s, ctx, at) => s.hiss(ctx, at, 0.14, 0.18, 'lowpass', [1600, 700], 0.8, 0.01),
    // A fall: a heavy thud and a breath out.
    down: (s, ctx, at) => {
        s.tone(ctx, at, 0.5, 0.6, 140, 45);
        s.hiss(ctx, at, 0.35, 0.3, 'lowpass', [420, 160], 0.7, 0.01);
    },
    // Up again: two notes rising.
    rise: (s, ctx, at) => {
        s.tone(ctx, at, 0.18, 0.14, 392, 392);
        s.tone(ctx, at + 0.12, 0.25, 0.14, 523, 523);
    },
    truce: (s, ctx, at) => s.tone(ctx, at, 0.4, 0.1, 440, 440, 'triangle', 0.05),
    over: (s, ctx, at) => {
        s.tone(ctx, at, 0.5, 0.12, 330, 330, 'triangle', 0.02);
        s.tone(ctx, at + 0.18, 0.6, 0.12, 494, 494, 'triangle', 0.02);
    },
    // One's own turn: a soft two-note chime.
    turn: (s, ctx, at) => {
        s.tone(ctx, at, 0.35, 0.12, 784, 784, 'sine', 0.01);
        s.tone(ctx, at + 0.09, 0.45, 0.1, 1175, 1175, 'sine', 0.01);
    },
};

export const SoundCues = Object.keys(Recipes);
