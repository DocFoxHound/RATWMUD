// How a fight looks as it happens (Docs/Design/33-combat.md, "Visual feedback"): the W nudges toward what it attacks
// and recoils from what hits it, and brief glyphs mark contact, a miss, fire. All of it is a display offset drawn from
// the fight's own log: the server's positions never move, and nothing here decides an outcome.
import type {BattleLine, BattleView, Tile} from './battle.ts';

export interface Mark {
    x: number;                  // Tile coordinates (centres), in the arena.
    y: number;
    glyph: string;
    color: 'hit' | 'graze' | 'miss' | 'fire' | 'smoke' | 'burn' | 'charge' | 'item';
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
}

const Strike = 0.25, Mark = 0.5, Flame = 1.0;

/** A bump that goes out and comes back over `length` seconds: 0 → 1 → 0. */
const bump = (t: number, length: number) => (t < 0 || t > length ? 0 : Math.sin(Math.PI * t / length));

export class FightEffects {
    private seen = new Map<string, number>();       // The last line seen of each fight.
    private effects: Effect[] = [];

    /** Takes in a fight's new lines (by their sequence); the first sight of a fight starts nothing (no replay). */
    update(b: BattleView | null, clock: number) {
        this.effects = this.effects.filter(e => clock - e.at < Flame + 0.2);
        if (!b) return;
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
        if (['hit', 'graze', 'slash', 'miss', 'burnt', 'flame'].includes(line.kind))
            this.effects.push({kind: line.kind, actor: line.actor, target: line.target, tiles: line.tiles, at: clock, from: at(line.actor),
                to: at(line.target)});
    }

    /** How far a fighter is drawn from its tile now (tiles): a lunge, a recoil, a sidestep. Nothing in reduced motion. */
    offset(id: string, clock: number, reduced: boolean): [number, number] {
        if (reduced) return [0, 0];
        let dx = 0, dy = 0;
        for (const e of this.effects) {
            if (!e.from || !e.to) continue;
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

    /** The marks to draw now: contact, misses and fire from what just happened; burning, gathering fire, smoke and
     * dropped things from how the fight stands. */
    marks(b: BattleView, clock: number, reduced: boolean): Mark[] {
        const out: Mark[] = [];
        for (const e of this.effects) {
            const t = clock - e.at;
            const fade = Math.max(0, 1 - t / Mark);
            if (e.kind === 'flame' || e.kind === 'burnt') {
                if (e.kind === 'flame' && t < Flame)
                    e.tiles.forEach(([x, y], i) => {
                        const flicker = reduced ? 0 : Math.floor(clock * 12 + i * 3) % 3;
                        out.push({x, y, glyph: '^~*'[flicker], color: 'fire', alpha: Math.max(0.2, 1 - t / Flame)});
                    });
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

    /** A gathering caster trembles where it stands. */
    tremble(id: string, b: BattleView, clock: number, reduced: boolean): [number, number] {
        if (reduced || !b.fighters.find(f => f.id === id)?.casting) return [0, 0];
        return [Math.sin(clock * 40) * 0.04, Math.cos(clock * 33) * 0.03];
    }
}
