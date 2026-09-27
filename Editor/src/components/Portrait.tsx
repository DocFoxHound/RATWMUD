import type {CSSProperties} from 'react';
import {COATS} from '../model/model.mjs';
import type {Appearance} from '../model/model.mjs';

// Data/Portraits/<species>.png is a 2×2 grayscale atlas of 768×512 frames:
// young (top left), adolescent (top right), adult (bottom left), old (bottom right).
// The frame is tinted with the coat colour and masked to the wolf's silhouette,
// approximating the game's runtime recolouring.
const FRAMES = ['0% 0%', '100% 0%', '0% 100%', '100% 100%'];
const stage = (age: number) => age <= 12 ? 0 : age <= 17 ? 1 : age <= 64 ? 2 : 3;

export function Portrait({appearance, age, size = 56, className = ''}: {appearance: Appearance; age: number; size?: number; className?: string}) {
    const url = `url(portraits/${appearance.species}.png)`, at = FRAMES[stage(age)];
    const layer: CSSProperties = {backgroundImage: url, backgroundSize: '200% 200%', backgroundPosition: at,
        maskImage: url, maskSize: '200% 200%', maskPosition: at, WebkitMaskImage: url, WebkitMaskSize: '200% 200%', WebkitMaskPosition: at};
    return (
        <span className={`portrait ${className}`} style={{width: size, height: size * 2 / 3}} role="img"
            aria-label={`${appearance.species} wolf, ${COATS[appearance.baseColor][0]} coat`}>
            <span style={{...layer, backgroundColor: COATS[appearance.baseColor][1], backgroundBlendMode: 'multiply'}} />
        </span>
    );
}
