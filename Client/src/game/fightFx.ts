// How a fight looks and sounds as it happens (Docs/Design/33-combat.md, "Visual feedback"; doc 37, phase 3): the W
// lunges at what it attacks and recoils from what hits it; the one hit flashes; the damage rises off them as a figure;
// a heavy blow shakes the view; a fall bursts; fire fills its cone and leaves ash; health drains with a lag so the size
// of a blow shows. All of it is drawn from the fight's own log: the server's positions never move, and nothing here
// decides an outcome. What should be heard is queued as cues (ui/sound.ts plays them).
import {apart, FamilyColours, type BattleLine, type BattleView, type FighterView, type Tile} from './battle.ts';

export interface Mark {
    x: number;                  // Tile coordinates (centres), in the arena.
    y: number;
    glyph: string;
    color: 'hit' | 'graze' | 'miss' | 'fire' | 'smoke' | 'burn' | 'charge' | 'item' | 'ash' | 'gift';
    alpha: number;
    tint?: string;              // A Gift's own colour ("#rrggbb"), for color 'gift'.
    size?: number;              // Relative to a tile (default a tile's glyph).
}

/** Each Gift family's glyphs (doc 43), for what its magic does on the arena. */
export const FamilyGlyphs: Record<string, string> = {fire: '^*~', earth: '▲▪∴', water: '≈~°', wind: '≋~»', sound: ')(·', blinker: '✦·*',
    gravity: '◎•○', seer: '◉✧·', death_walker: '†·.'};

/** Lines a Gift's magic tells (doc 43): drawn with the family's glyphs, not as a blow. */
const GiftKinds = ['gift', 'blink', 'crash', 'pulled', 'stumble', 'riposte', 'overreach', 'lost', 'still', 'break'];

/** A figure rising off a wolf: the damage of a blow, "miss", "down". */
export interface Float {
    x: number;                  // Tile coordinates (centre of the tile it rose from), already risen.
    y: number;
    text: string;
    color: 'hit' | 'heavy' | 'graze' | 'miss' | 'fire' | 'down' | 'heal' | 'gift';
    size: number;               // Relative: 1 a normal blow.
    alpha: number;
    sub?: string;               // Under it, smaller: where the blow landed ("throat").
}

/** A ring bursting out from a tile (a fall). */
export interface Ring {
    x: number;
    y: number;
    radius: number;             // In tiles.
    alpha: number;
}

/** What another wolf just did, in a word or two, said under its token (doc 37: a turn shown, not just run). */
export interface Caption {
    id: string;
    text: string;
    alpha: number;
}

interface Effect {
    kind: string;
    actor: string;
    target: string;
    tiles: Tile[];
    at: number;                 // When it began (the page's clock).
    from: Tile | null;          // Where the actor and target stood.
    to: Tile | null;
    damage: number;             // The figure the line gave ("(12)"), 0 for none.
    zone: string;               // Where it landed ("throat", "flank"), from the line's " on the throat".
    family: string;             // The Gift family of whoever did it (doc 43), for its magic's colour and glyphs.
}

const Strike = 0.25, Mark = 0.5, Flame = 1.0, Ash = 2.5, FloatTime = 1.1, Flash = 0.18, Shake = 0.32, Burst = 0.7;
const Said = 1.6;               // How long a caption stays under the wolf.
const Heavy = 18;               // A blow this hard shakes the view (doc 33: it can knock a sword loose).

/** A bump that goes out and comes back over `length` seconds: 0 → 1 → 0. */
const bump = (t: number, length: number) => (t < 0 || t > length ? 0 : Math.sin(Math.PI * t / length));

/** What a fight line should sound like (ui/sound.ts), if anything. */
const Cues: Record<string, string> = {hit: 'bite', graze: 'graze', slash: 'cut', miss: 'miss', flame: 'fire', charge: 'charge',
    burn: 'burn', down: 'down', death: 'down', rise: 'rise', tend: 'rise', truce: 'truce', yield: 'truce', over: 'over', shove: 'graze',
    gift: 'charge', blink: 'miss', crash: 'bite', riposte: 'cut', overreach: 'burn', pulled: 'graze'};

export class FightEffects {
    private seen = new Map<string, number>();       // The last line seen of each fight.
    private effects: Effect[] = [];
    private shown = new Map<string, {health: number; dropAt: number; at: number}>();     // Health as drawn, draining after a blow.
    private shakeAt = -10;
    private shakeSize = 0;
    /** Sounds to play, oldest first: taken by whoever plays them (`takeCues`). */
    private cues: string[] = [];
    private said: Array<{id: string; text: string; at: number}> = [];
    private walking = new Map<string, boolean>();   // Who was walking at the last look (a walk begun is said).
    selfId = '';

    /** Takes in a fight's new lines (by their sequence); the first sight of a fight starts nothing (no replay). */
    update(b: BattleView | null, clock: number) {
        this.effects = this.effects.filter(e => clock - e.at < Math.max(Flame + Ash, FloatTime) + 0.2);
        this.said = this.said.filter(c => clock - c.at < Said);
        if (!b) {
            this.shown.clear();
            this.walking.clear();
            return;
        }
        // Another wolf setting off: where to, in a word (closing in, falling back).
        for (const f of b.fighters) {
            const walking = f.walk.length > 0;
            if (walking && !this.walking.get(f.id) && f.id !== this.selfId) this.say(f.id, moveWord(b, f), clock);
            this.walking.set(f.id, walking);
        }
        const last = this.seen.get(b.id);
        const top = b.log.reduce((m, l) => Math.max(m, l.seq), 0);
        if (last === undefined) {
            this.seen.set(b.id, top);
            return;
        }
        for (const line of b.log) {
            if (line.seq <= last) continue;
            this.begin(b, line, clock);
        }
        this.seen.set(b.id, Math.max(last, top));
    }

    private begin(b: BattleView, line: BattleLine, clock: number) {
        const at = (id: string): Tile | null => {
            const f = b.fighters.find(o => o.id === id);
            return f ? [f.x, f.y] : null;
        };
        const damage = Number(/\((\d+)\)\.?$/.exec(line.text)?.[1] ?? 0);
        const zone = / on (?:the|a) ([a-z]+)[,( ]/.exec(line.text)?.[1] ?? '';
        // Whom it befell: a line's target ("bites Bo", "tends Bo", "Bo is caught in the fire"), but its actor for what
        // happens to oneself ("Bo burns", "Bo goes down", "Bo dies", "Bo struggles back to their feet").
        const hurt = ['burn', 'down', 'death', 'rise'].includes(line.kind) ? line.actor : line.target;
        const family = b.fighters.find(f => f.id === line.actor)?.gift ?? '';
        const effect: Effect = {kind: line.kind, actor: line.actor, target: hurt, tiles: line.tiles, at: clock, from: at(line.actor),
            to: at(hurt), damage, zone, family};
        if (['hit', 'graze', 'slash', 'miss', 'burnt', 'burn', 'flame', 'down', 'death', 'tend', 'rise', 'shove', ...GiftKinds].includes(line.kind))
            this.effects.push(effect);
        // A heavy blow, fire, or a fall shakes the view: more when it is oneself.
        const heavy = damage >= Heavy || line.kind === 'flame' || line.kind === 'down' || line.kind === 'death';
        if (heavy || (damage > 0 && hurt === this.selfId)) {
            const size = (heavy ? 1 : 0.5) * (hurt === this.selfId ? 1.6 : 1);
            if (clock - this.shakeAt > Shake || size > this.shakeSize) {
                this.shakeAt = clock;
                this.shakeSize = size;
            }
        }
        const cue = Cues[line.kind];
        if (cue) this.cues.push(cue);
        const word = captionOf(line);
        if (word && line.actor && line.actor !== this.selfId) this.say(line.actor, word, clock);
    }

    private say(id: string, text: string, clock: number) {
        this.said = this.said.filter(c => c.id !== id);         // (One at a time under a wolf: the latest.)
        this.said.push({id, text, at: clock});
    }

    /** What other wolves just did, a word or two each: "steps in", then "bites". */
    captions(clock: number): Caption[] {
        return this.said.map(c => {
            const t = clock - c.at;
            return {id: c.id, text: c.text, alpha: t < Said * 0.7 ? 1 : Math.max(0, 1 - (t - Said * 0.7) / (Said * 0.3))};
        }).filter(c => c.alpha > 0);
    }

    /** The sounds since last asked. */
    takeCues(): string[] {
        return this.cues.splice(0);
    }

    /** Queues a sound the log doesn't carry (one's own turn coming). */
    cue(name: string) {
        this.cues.push(name);
    }

    /** How far a fighter is drawn from its tile now (tiles): a lunge, a recoil, a sidestep. Nothing in reduced motion. */
    offset(id: string, clock: number, reduced: boolean): [number, number] {
        if (reduced) return [0, 0];
        let dx = 0, dy = 0;
        for (const e of this.effects) {
            if (!e.from || !e.to || e.kind === 'burn' || e.kind === 'down' || e.kind === 'death' || e.kind === 'tend' || e.kind === 'rise' ||
                (GiftKinds.includes(e.kind) && e.kind !== 'crash' && e.kind !== 'riposte')) continue;
            const t = clock - e.at;
            const len = Math.hypot(e.to[0] - e.from[0], e.to[1] - e.from[1]) || 1;
            const ux = (e.to[0] - e.from[0]) / len, uy = (e.to[1] - e.from[1]) / len;
            if (e.actor === id && e.kind !== 'burnt') {
                const reach = e.kind === 'slash' ? 0.4 : 0.3;
                dx += ux * reach * bump(t, Strike);
                dy += uy * reach * bump(t, Strike);
            }
            if (e.target === id) {
                if (e.kind === 'miss') {
                    // The one it missed sidesteps.
                    dx += -uy * 0.2 * bump(t, Strike);
                    dy += ux * 0.2 * bump(t, Strike);
                } else {
                    const back = e.kind === 'graze' ? 0.1 : 0.25;
                    dx += ux * back * bump(t, Strike);
                    dy += uy * back * bump(t, Strike);
                }
            }
        }
        return [dx, dy];
    }

    /** How brightly a fighter flashes now (0..1): struck, burnt, or tended. */
    flash(id: string, clock: number): number {
        let most = 0;
        for (const e of this.effects) {
            if (e.target !== id || e.kind === 'miss') continue;
            const t = clock - e.at - (e.kind === 'burnt' ? 0 : Strike * 0.4);     // At contact, not at the wind-up.
            if (t >= 0 && t < Flash) most = Math.max(most, 1 - t / Flash);
        }
        return most;
    }

    /** The view's shake now, in pixels: a heavy blow, fire, a fall. None in reduced motion. */
    shake(clock: number, reduced: boolean): [number, number] {
        const t = clock - this.shakeAt;
        if (reduced || t < 0 || t > Shake) return [0, 0];
        const amp = 5 * this.shakeSize * (1 - t / Shake);
        return [Math.sin(t * 90) * amp, Math.cos(t * 73) * amp * 0.7];
    }

    /** The figures rising now: damage dealt, misses, falls, wounds tended. */
    floats(clock: number, reduced: boolean): Float[] {
        const out: Float[] = [];
        for (const e of this.effects) {
            if (!e.to) continue;
            const t = clock - e.at - Strike * 0.4;
            if (t < 0 || t > FloatTime) continue;
            const rise = reduced ? 0.95 : 0.95 + t * 0.6;           // (Starting above the name over the token.)
            const alpha = t < FloatTime * 0.6 ? 1 : 1 - (t - FloatTime * 0.6) / (FloatTime * 0.4);
            const [x, y] = e.to;
            if (e.kind === 'miss') out.push({x, y: y - rise, text: 'miss', color: 'miss', size: 0.8, alpha});
            else if (e.kind === 'down' || e.kind === 'death')
                out.push({x, y: y - rise * 0.6, text: e.kind === 'death' ? 'DEAD' : 'DOWN', color: 'down', size: 1.2, alpha});
            else if (e.kind === 'tend' || e.kind === 'rise') out.push({x, y: y - rise, text: 'up', color: 'heal', size: 0.9, alpha});
            else if (e.kind === 'overreach') out.push({x, y: y - rise, text: 'OVERREACH', color: 'down', size: 0.8, alpha});
            else if (e.kind === 'lost') out.push({x, y: y - rise, text: '…', color: 'gift', size: 1.1, alpha});
            else if (e.kind === 'riposte') out.push({x, y: y - rise, text: 'riposte', color: 'gift', size: 0.85, alpha});
            else if (e.damage > 0)
                out.push({x, y: y - rise, text: String(e.damage), sub: e.zone || undefined,
                    color: e.kind === 'burnt' || e.kind === 'burn' ? 'fire' : e.kind === 'graze' ? 'graze' : e.damage >= Heavy ? 'heavy' : 'hit',
                    size: e.kind === 'graze' ? 0.8 : e.damage >= Heavy ? 1.35 : 1, alpha});
        }
        return out;
    }

    /** Rings bursting from a fall. */
    rings(clock: number, reduced: boolean): Ring[] {
        const out: Ring[] = [];
        if (reduced) return out;
        for (const e of this.effects) {
            if ((e.kind !== 'down' && e.kind !== 'death') || !e.to) continue;
            const t = clock - e.at;
            if (t >= 0 && t < Burst) out.push({x: e.to[0], y: e.to[1], radius: 0.4 + t / Burst * 1.4, alpha: 1 - t / Burst});
        }
        return out;
    }

    /** The tiles fire fills now, and how fiercely (0..1): for a glow under the flames. */
    fire(clock: number): Array<{x: number; y: number; heat: number}> {
        const out: Array<{x: number; y: number; heat: number}> = [];
        for (const e of this.effects) {
            if (e.kind !== 'flame') continue;
            const t = clock - e.at;
            if (t >= 0 && t < Flame) {
                const heat = t < 0.15 ? t / 0.15 : 1 - (t - 0.15) / (Flame - 0.15) * 0.6;
                for (const [x, y] of e.tiles) out.push({x, y, heat});
            }
        }
        return out;
    }

    /**
     * A fighter's health as drawn: after a blow it holds a moment, then drains to the true figure, so the size of the
     * blow shows; it rises at once.
     */
    lagHealth(id: string, health: number, clock: number): number {
        const s = this.shown.get(id);
        if (!s || health >= s.health) {
            this.shown.set(id, {health, dropAt: -1, at: clock});
            return health;
        }
        if (s.dropAt < 0) s.dropAt = clock;
        if (clock - s.dropAt > 0.35) s.health = Math.max(health, s.health - 60 * Math.max(0, clock - s.at));
        s.at = clock;
        if (s.health <= health) s.dropAt = -1;
        return s.health;
    }

    /** The marks to draw now: contact, misses and fire from what just happened; burning, gathering fire, smoke and
     * dropped things from how the fight stands. */
    marks(b: BattleView, clock: number, reduced: boolean): Mark[] {
        const out: Mark[] = [];
        for (const e of this.effects) {
            const t = clock - e.at;
            const fade = Math.max(0, 1 - t / Mark);
            if (e.kind === 'flame') {
                if (t < Flame)
                    e.tiles.forEach(([x, y], i) => {
                        // Two tongues of flame a tile, flickering; then ash where it went.
                        const flicker = reduced ? 0 : Math.floor(clock * 14 + i * 3) % 3;
                        const alpha = Math.max(0.25, 1 - t / Flame);
                        out.push({x: x - 0.15, y: y + 0.1, glyph: '^~*'[flicker], color: 'fire', alpha});
                        out.push({x: x + 0.18, y: y - 0.15, glyph: '~*^'[flicker], color: 'fire', alpha: alpha * 0.8});
                    });
                else if (t < Flame + Ash)
                    for (const [x, y] of e.tiles) out.push({x, y, glyph: '·', color: 'ash', alpha: 0.6 * (1 - (t - Flame) / Ash)});
                continue;
            }
            if (GiftKinds.includes(e.kind)) {
                this.giftMarks(e, t, clock, reduced, out);
                continue;
            }
            if (!e.to || fade <= 0) continue;
            const [x, y] = e.to;
            if (e.kind === 'hit' || e.kind === 'slash') out.push({x, y, glyph: e.kind === 'slash' ? ')' : '*', color: 'hit', alpha: fade});
            else if (e.kind === 'graze') out.push({x, y, glyph: "'", color: 'graze', alpha: fade});
            else if (e.kind === 'miss') out.push({x, y, glyph: '(', color: 'miss', alpha: fade * 0.6});
        }
        for (const [x, y] of b.smoke) out.push({x, y, glyph: '░', color: 'smoke', alpha: 0.35});
        for (const d of b.drops) out.push({x: d.x, y: d.y, glyph: '†', color: 'item', alpha: 1});
        for (const f of b.fighters) {
            if (f.burning > 0) out.push({x: f.x, y: f.y - 0.6, glyph: '~', color: 'burn', alpha: reduced ? 1 : 0.6 + 0.4 * Math.abs(Math.sin(clock * 6))});
            if (f.casting) {
                // The tell: heat gathering at the muzzle.
                const step = reduced ? 2 : Math.floor(clock * 3) % 3;
                out.push({x: f.x + 0.45, y: f.y - 0.1, glyph: '.:*'[step], color: 'charge', alpha: 1});
            }
        }
        return out;
    }

    /**
     * What a Gift does, drawn on the arena (doc 43): its tiles flicker with the family's glyphs and fade; a blink leaves a
     * spark where it went from and where it came to, joined by a trail; a crash bursts.
     */
    private giftMarks(e: Effect, t: number, clock: number, reduced: boolean, out: Mark[]) {
        const tint = FamilyColours[e.family] ?? '#c9a640';
        const glyphs = FamilyGlyphs[e.family] ?? '*·.';
        const life = e.kind === 'gift' ? 1.1 : 0.8;
        if (t < 0 || t > life) return;
        const alpha = t < 0.15 ? t / 0.15 : 1 - (t - 0.15) / (life - 0.15);
        if (e.kind === 'blink' && e.tiles.length >= 2) {
            const [[ax, ay], [bx, by]] = e.tiles;
            const n = Math.max(1, Math.round(Math.hypot(bx - ax, by - ay) * 2));
            for (let i = 1; i < n; ++i) {
                const k = i / n;
                if (!reduced && k > t / life * 1.5) break;
                out.push({x: ax + (bx - ax) * k, y: ay + (by - ay) * k, glyph: '·', color: 'gift', tint, alpha: alpha * 0.7});
            }
            out.push({x: ax, y: ay, glyph: '✦', color: 'gift', tint, alpha: alpha * 0.6, size: 0.6});
            out.push({x: bx, y: by, glyph: '✦', color: 'gift', tint, alpha, size: 0.9});
            return;
        }
        if (e.kind === 'crash' && e.to) {
            out.push({x: e.to[0], y: e.to[1], glyph: '✸', color: 'gift', tint: '#f3e3c3', alpha, size: 0.9});
            return;
        }
        if (e.kind === 'stumble' && e.from) {
            out.push({x: e.from[0], y: e.from[1] - 0.5, glyph: '?', color: 'gift', tint: '#d9c08c', alpha});
            return;
        }
        if (e.kind === 'break' && e.from) {
            out.push({x: e.from[0], y: e.from[1] - 0.6, glyph: '×', color: 'gift', tint, alpha});
            return;
        }
        const tiles = e.tiles.length ? e.tiles : e.to ? [e.to] : e.from ? [e.from] : [];
        tiles.forEach(([x, y], i) => {
            const step = reduced ? 0 : Math.floor(clock * 10 + i * 2) % glyphs.length;
            out.push({x, y, glyph: glyphs[step], color: 'gift', tint, alpha, size: 0.75});
        });
    }

    /** A gathering caster trembles where it stands. */
    tremble(id: string, b: BattleView, clock: number, reduced: boolean): [number, number] {
        if (reduced || !b.fighters.find(f => f.id === id)?.casting) return [0, 0];
        return [Math.sin(clock * 40) * 0.04, Math.cos(clock * 33) * 0.03];
    }
}

/** A fight line's doing, in a word or two, for the caption under whoever did it ("" for none). */
export function captionOf(line: BattleLine): string {
    const t = line.text;
    switch (line.kind) {
        case 'hit': return 'bites';
        case 'graze': return t.includes(' nicks ') ? 'cuts' : 'bites';
        case 'slash': return 'cuts';
        case 'miss': return t.includes(' swings ') ? 'swings' : 'snaps';
        case 'charge': return t.includes('heat shimmers') ? 'gathers fire' : 'gathers';
        case 'gift': return 'Gift';
        case 'blink': return 'blinks';
        case 'riposte': return 'ripostes';
        case 'overreach': return 'overreaches';
        case 'flame': return 'FIRE';
        case 'tend': return 'tends';
        case 'roll': return 'rolls';
        case 'rest': return 'rests';
        case 'guard': return 'on guard';
        case 'ambush': return t.includes('strikes from hiding') ? 'from hiding' : '';
        case 'suspect': return '?';
        case 'notice': return '!';
        case 'shove': return 'shoves';
        case 'hold': return 'takes a sword';
        case 'stow': return 'stows the sword';
        case 'pickup': return 'grabs the sword';
        case 'struggle': return 'struggles';
        case 'flee': return t.includes('cut off') ? 'tries to flee' : 'flees';
        case 'wait': return t.endsWith(' waits.') ? 'waits' : '';
        case 'yield': return t.includes('offers to yield') ? 'yields' : '';
        case 'truce': return t.includes('offers a truce') ? 'offers a truce' : '';
        default: return '';
    }
}

/** A walk begun, in a word: closing on a foe, falling back from them, or only moving. */
export function moveWord(b: BattleView, f: FighterView): string {
    const foes = b.fighters.filter(o => o.side !== f.side && o.status === 'fighting');
    const end = f.walk[f.walk.length - 1];
    if (!end || !foes.length) return 'moves';
    const nearest = (x: number, y: number) => Math.min(...foes.map(o => apart(x, y, o.x, o.y)));
    const reach = f.mouth === 'sword' ? 2 : 1;
    if (nearest(end[0], end[1]) <= reach) return 'steps in';
    return nearest(end[0], end[1]) > nearest(f.x, f.y) ? 'falls back' : 'moves';
}
