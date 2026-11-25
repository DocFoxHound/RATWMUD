import {useEffect, useRef} from 'react';
import {COATS} from '../model/model.mjs';
import type {Appearance} from '../model/model.mjs';
import {drawPortrait, Portraits} from '../../../Client/src/ui/portrait';

// Renders any NPC or character using the layered wolf dolls (Client/src/assets/wolves, shared 1033x821 frame),
// matching the exact sex, size, and coat/fur shown in the game client.
let sharedPortraits: Portraits | undefined;

export function Portrait({appearance, age: _age, size = 56, className = ''}: {appearance: Appearance; age: number; size?: number; className?: string}) {
    const ref = useRef<HTMLCanvasElement>(null);
    const height = Math.max(1, Math.round(size * 821 / 1033));
    const pixelW = Math.max(2, Math.round(size * 2));
    const pixelH = Math.max(2, Math.round(height * 2));
    const lookKey = JSON.stringify(appearance);

    useEffect(() => {
        const canvas = ref.current;
        const ctx = canvas?.getContext('2d');
        if (!canvas || !ctx) return;
        const images = sharedPortraits ??= new Portraits();
        const fullLook = {gradientAmount: 0.35, patternAmount: 0.55, ...appearance};
        const paint = () => {
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            drawPortrait(ctx, images, fullLook, 0, 0, canvas.width, canvas.height, undefined, {view: 'profile', pad: 0.03});
        };
        paint();
        return images.listen(paint);
    }, [lookKey, pixelW, pixelH]);

    const coatName = COATS[appearance.baseColor]?.[0] ?? 'wolf';
    return (
        <span className={`portrait ${className}`} style={{width: size, height}} role="img"
            aria-label={`${appearance.sex} ${appearance.stature} wolf, ${coatName} coat`}>
            <canvas ref={ref} width={pixelW} height={pixelH} style={{width: '100%', height: '100%', display: 'block'}} />
        </span>
    );
}
