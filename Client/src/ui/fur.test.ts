// The layered wolf dolls' colours (fur.ts; Docs/Design/81-painted-wolves.md): the palette's rule, the 46 doll layers,
// the layered doll compositor, the face ramp, validation matching Core, and the derived look an older wolf is given.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {AllDolls, buildOf, BuildNames, Builds, BuildStature, coatOf, coatShades, coatThree, dollColours, dollOf, DollLayers, EyeColours, FurColours,
    furOf, furRamp, furRgb, FurZones, hexRgb, isEyeColour, isFurColour, isFurLayer, layerColour, nearestEye, nearestFur, olderLook, paintDoll,
    paintFur, RampStops, readFur, ReadyCoats, sexOf, ShadowLuma, zoneRamps, type DollSource, type Fur, type FurSource, type OlderLook, type Rgb} from './fur.ts';
import {readAppearance} from './portrait.ts';

const older: OlderLook = {species: 'timber', sex: 'male', stature: 'average', pattern: 'saddle', baseColor: 3, gradientColor: 1, markingColor: 5,
    gradientAmount: 0.35, patternAmount: 0.55};
const nine = {...older, sex: 'male'};

test('the palette: one file, 56 muted fur colours, 23 eyes (vivid allowed), 46 doll layers and ready coats', () => {
    const file = JSON.parse(readFileSync(new URL('../../../Data/Wolves/palette.json', import.meta.url), 'utf8'));
    assert.deepEqual(FurColours.map(c => [c.id, c.name, c.group, c.hex]), file.fur.map((c: any) => [c.id, c.name, c.group, c.hex]), 'run tools/fur_palette.py');
    assert.deepEqual(EyeColours.map(c => [c.id, c.hex]), file.eyes.map((c: any) => [c.id, c.hex]));
    assert.equal(DollLayers.length, 46);
    assert.equal(DollLayers[0].id, 'eyes');
    // Core's table is of the same file (Core/RatwFurPalette.inc), in the same order.
    const core = readFileSync(new URL('../../../Core/RatwFurPalette.inc', import.meta.url), 'utf8');
    assert.deepEqual([...core.matchAll(/^RATW_FUR\("([a-z_]+)", "[^"]+", 0x([0-9a-f]{6})\)$/gm)].map(m => [m[1], `#${m[2]}`]),
        FurColours.map(c => [c.id, c.hex]), 'run tools/fur_palette.py');
    assert.deepEqual([...core.matchAll(/^RATW_EYE\("([a-z_]+)"/gm)].map(m => m[1]), EyeColours.map(c => c.id));
    assert.deepEqual([...core.matchAll(/^RATW_FUR_ZONE\("([a-z_]+)"/gm)].map(m => m[1]), FurZones.map(z => z.id));
    assert.deepEqual([...core.matchAll(/^RATW_FUR_LAYER\("([a-z0-9_]+)"/gm)].map(m => m[1]), DollLayers.slice(1).map(l => l.id));
    assert.deepEqual(FurZones.map(z => z.id), ['back', 'sides', 'chest', 'legs', 'tail', 'tail_tip', 'head', 'muzzle']);
    assert.ok(FurColours.length >= 50 && FurColours.length <= 60 && EyeColours.length >= 20 && EyeColours.length <= 24);
    assert.equal(new Set(FurColours.map(c => c.id)).size, FurColours.length);
    const spread = (hex: string) => Math.max(...hexRgb(hex)) - Math.min(...hexRgb(hex));
    for (const c of FurColours) assert.ok(spread(c.hex) <= 100, `${c.name} is muted`);
    assert.ok(EyeColours.some(c => spread(c.hex) > 160), 'some eyes are vivid (the user, 2026-11-14)');
    for (const group of ['Blacks and charcoals', 'Greys and silvers', 'Whites and creams', 'Tans and buffs', 'Browns', 'Russets and muted reds',
        'Ochres and golds', 'Slate and dusk blues', 'Green-greys'])
        assert.ok(FurColours.some(c => c.group === group), group);
    assert.ok(ReadyCoats.length >= 10);
    for (const coat of ReadyCoats) {
        assert.equal(coat.coat.length, 5, `${coat.name} has 5 shades`);
        assert.ok(readFur({coat: coat.coat, eyes: coat.eyes, ...(coat.claws ? {claws: coat.claws} : {}),
            ...(coat.layers ? {layers: coat.layers} : {})}), coat.name);
    }
    assert.ok(isFurColour('russet') && !isFurColour('amber') && !isFurColour('#8a4a2c') && isEyeColour('amber') && !isEyeColour('russet'));
    assert.ok(isFurLayer('fore_socks') && isFurLayer('eyebrow') && !isFurLayer('eyes') && !isFurLayer('wings'));
});

test('the layered doll renderer: resolves 5-shade coat, claws, overrides, blends layers, multiplies shadow, composites lineart', () => {
    const fur: Fur = {coat: ['black', 'charcoal', 'iron', 'pewter', 'silver'], eyes: 'blue', claws: 'ivory',
        layers: {fore_socks: 'snow', eyebrow: 'cream'}};
    assert.equal(layerColour(fur, 'eyes'), 'blue');
    assert.equal(layerColour(fur, 'fore_socks'), 'snow');
    assert.equal(layerColour(fur, 'eyebrow'), 'cream');
    assert.equal(layerColour(fur, 'fore_claws'), 'ivory');
    assert.equal(layerColour(fur, 'back_accent'), 'black');
    assert.equal(layerColour(fur, 'belly'), 'silver');

    const lut = dollColours(fur);
    assert.equal(lut.length, 47 * 3);
    // 5 test pixels: (0) pure belly (layer 8), (1) 50/50 belly + back_accent (layers 8 & 12), (2) belly with full shadow,
    // (3) belly with opaque black lineart, (4) empty background.
    const idx = new Uint8ClampedArray([
        8, 0, 0, 255,
        8, 12, 0, 255,
        8, 0, 0, 255,
        8, 0, 0, 255,
        0, 0, 0, 255,
    ]);
    const wt = new Uint8ClampedArray([
        255, 0, 255, 255,
        128, 127, 255, 255,
        255, 0, 255, 255,
        255, 0, 255, 255,
        0, 0, 0, 255,
    ]);
    const ink = new Uint8ClampedArray([
        0, 0, 0, 255,
        0, 0, 0, 255,
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 0, 255,
    ]);
    const source: DollSource = {width: 5, height: 1, idx, wt, ink};
    const out = new Uint8ClampedArray(20);
    paintDoll(source, lut, out);
    const px = (i: number) => Array.from(out.slice(i * 4, i * 4 + 4));
    const silver = furRgb('silver'), black = furRgb('black');
    assert.deepEqual(px(0), [...silver, 255], 'pure layer takes its exact palette colour');
    const blended = px(1);
    for (let c = 0; c < 3; ++c) assert.ok(Math.abs(blended[c] - (silver[c] + black[c]) / 2) <= 1.5, 'blended edge mixes layer colours');
    const shaded = px(2);
    for (let c = 0; c < 3; ++c) assert.ok(Math.abs(shaded[c] - Math.round(silver[c] * ShadowLuma)) <= 1.5, 'shadow multiplies cleanly');
    assert.deepEqual(px(3), [0, 0, 0, 255], 'opaque lineart sits on top');
    assert.deepEqual(px(4), [0, 0, 0, 0], 'transparent background stays empty');
});

test('the face ramp and zone blend (kept for face.png until new faces arrive)', () => {
    const dark: Rgb = [40, 30, 20], mid: Rgb = [140, 100, 60], light: Rgb = [230, 220, 200];
    const ramp = furRamp(dark, mid, light);
    const at = (l: number) => [ramp[l * 3], ramp[l * 3 + 1], ramp[l * 3 + 2]];
    assert.equal(ramp.length, 768);
    assert.deepEqual(RampStops, [0, 38, 118, 212, 255]);
    assert.deepEqual(at(38), dark);
    assert.deepEqual(at(118), mid);
    assert.deepEqual(at(212), light);
    assert.deepEqual(at(0), [18, 14, 9]);
    assert.deepEqual(at(255), [243, 238, 228]);

    const fur: Fur = {coat: ['soot', 'ash', 'cream'], eyes: 'amber', zones: {legs: ['silver', 'mist', 'snow']}};
    const ramps = zoneRamps(fur);
    assert.equal(ramps.length, 8);
    assert.equal(ramps[0], ramps[1]);
    assert.notEqual(ramps[3], ramps[0]);
    const grey = (l: number, alpha = 255) => [l, l, l, alpha];
    const picture = new Uint8ClampedArray([...grey(118), ...grey(118), ...grey(118), 200, 150, 40, 255, 20, 22, 24, 255, ...grey(118, 0)]);
    const map = new Uint8ClampedArray([1, 0, 0, 255, 3, 0, 0, 255, 1 | (3 << 4), 128, 0, 255, 6, 0, 255, 255, 7, 0, 127, 255, 1, 0, 0, 255]);
    const source: FurSource = {width: 6, height: 1, picture, map};
    const out = new Uint8ClampedArray(24);
    paintFur(source, ramps, [217, 164, 65], out);
    assert.deepEqual(Array.from(out.slice(0, 4)), [...furRgb('ash'), 255]);
    assert.deepEqual(Array.from(out.slice(4, 8)), [...furRgb('mist'), 255]);
    assert.deepEqual(Array.from(out.slice(16, 20)), [20, 22, 24, 255]);
    assert.deepEqual(Array.from(out.slice(20, 24)), [0, 0, 0, 0]);
});

test('a wolf\'s fur is checked as the server checks it (3-shade legacy and 5-shade layered)', () => {
    assert.deepEqual(readFur({coat: ['soot', 'ash', 'cream'], eyes: 'amber'}), {coat: ['soot', 'ash', 'cream'], eyes: 'amber'});
    assert.deepEqual(readFur({coat: ['soot', 'flint', 'ash', 'dove', 'cream'], eyes: 'amber', claws: 'straw', layers: {eyebrow: 'snow'}}),
        {coat: ['soot', 'flint', 'ash', 'dove', 'cream'], eyes: 'amber', claws: 'straw', layers: {eyebrow: 'snow'}});
    assert.ok(readFur({coat: ['soot', 'ash', 'cream'], eyes: 'amber', zones: {tail_tip: ['black', 'soot', 'charcoal']}}));
    for (const bad of [null, 'timber', {coat: ['soot', 'ash'], eyes: 'amber'}, {coat: ['soot', 'ash', 'cream', 'snow'], eyes: 'amber'},
        {coat: ['soot', 'ash', 'neon'], eyes: 'amber'}, {coat: ['soot', 'ash', '#ff00ff'], eyes: 'amber'},
        {coat: ['soot', 'ash', 'cream'], eyes: 'purple'}, {coat: ['soot', 'ash', 'cream']},
        {coat: ['soot', 'ash', 'cream'], eyes: 'amber', claws: 'amber'},
        {coat: ['soot', 'ash', 'cream'], eyes: 'amber', zones: {}}, {coat: ['soot', 'ash', 'cream'], eyes: 'amber', zones: {wings: ['soot', 'ash', 'cream']}},
        {coat: ['soot', 'ash', 'cream'], eyes: 'amber', layers: {}}, {coat: ['soot', 'ash', 'cream'], eyes: 'amber', layers: {eyes: 'snow'}},
        {coat: ['soot', 'ash', 'cream'], eyes: 'amber', layers: {wings: 'snow'}}, {coat: ['soot', 'ash', 'cream'], eyes: 'amber', layers: {eyebrow: 'amber'}},
        {coat: ['soot', 'ash', 'cream'], eyes: 'amber', extra: 1}])
        assert.equal(readFur(bad), null, JSON.stringify(bad));
    assert.ok(readAppearance({...nine, build: 'heavy', fur: {coat: ['soot', 'flint', 'ash', 'dove', 'cream'], eyes: 'amber'}})?.fur);
    for (const build of ['small', 'medium', 'large', 'lean', 'average', 'heavy']) assert.ok(readAppearance({...nine, build}), `${build} is a build`);
    assert.equal(readAppearance({...nine, build: 'huge'}), null);
    assert.equal(readAppearance(nine)?.fur, undefined);
});

test('an older wolf or NPC derives a 5-shade layered look and sex/size doll deterministically', () => {
    const fur = furOf(older);
    assert.ok(readFur(fur), 'the derived fur is valid');
    assert.equal(fur.coat.length, 5, 'derived coat has 5 shades');
    assert.deepEqual(furOf(older), fur, 'deterministic');
    assert.equal(fur.coat[2], nearestFur('#8e8271'), 'the coat colour is the mid shade');
    assert.equal(fur.eyes, 'amber');
    assert.ok(fur.zones?.back, 'a saddle pattern sets the back zone');
    assert.ok(fur.zones?.legs && fur.zones.chest);
    assert.deepEqual(furOf({...older, pattern: 'solid', gradientAmount: 0}).zones, undefined);
    // Expanding 3-shade to 5-shade and reducing 5-shade to 3-shade
    assert.equal(coatShades(['soot', 'ash', 'cream']).length, 5);
    assert.deepEqual(coatThree(['soot', 'flint', 'ash', 'dove', 'cream']), ['soot', 'ash', 'cream']);
    // Sex and build pick the exact doll
    assert.deepEqual(Builds.map(b => [b, BuildNames[b], BuildStature[b]]), [['small', 'Small', 'short'], ['medium', 'Average', 'average'], ['large', 'Tall', 'tall']]);
    assert.equal(sexOf({...older, sex: 'female'}), 'female');
    assert.equal(dollOf({...older, sex: 'female', stature: 'tall'}), 'female-large');
    assert.equal(dollOf({...older, sex: 'male', stature: 'short'}), 'male-small');
    assert.deepEqual(['lean', 'heavy', 'average'].map(build => buildOf({...older, stature: 'short', build})), ['large', 'medium', 'small']);
    // olderLook summarizes layered look cleanly
    const chosen: Fur = {coat: ['black', 'charcoal', 'iron', 'pewter', 'silver'], eyes: 'gold', claws: 'ivory',
        layers: {fore_socks: 'snow', tail_tip: 'snow'}};
    const o = olderLook(chosen);
    assert.match(o.coat, /^#[0-9a-f]{6}$/);
    assert.equal(o.gradientTint, '#adb1b0');
    assert.deepEqual(o.markings.map(m => m.mask), ['socks', 'tail_tip']);
    assert.ok(readAppearance({...nine, pattern: 'solid', build: 'large', stature: 'tall', fur: chosen, coat: o.coat, gradientTint: o.gradientTint, eyes: o.eyes, markings: o.markings}));
    assert.equal(nearestEye('#d9a441'), 'amber');
    assert.deepEqual(coatOf(furRgb('charcoal')).map(isFurColour), [true, true, true]);
});

test('the doll assets: six sex/size dolls in one shared ground-aligned frame, plus the face painting', () => {
    const size = (name: string) => {
        const bytes = readFileSync(new URL(`../assets/wolves/${name}.png`, import.meta.url));
        assert.equal(bytes.toString('latin1', 1, 4), 'PNG', name);
        return [bytes.readUInt32BE(16), bytes.readUInt32BE(20)];
    };
    const frame = size('male-large-idx');
    assert.ok(frame[0] > frame[1], 'the shared doll frame is wider than tall');
    for (const doll of AllDolls) {
        assert.deepEqual(size(`${doll}-idx`), frame, `${doll}-idx matches shared frame`);
        assert.deepEqual(size(`${doll}-wt`), frame, `${doll}-wt matches shared frame`);
        assert.deepEqual(size(`${doll}-ink`), frame, `${doll}-ink matches shared frame`);
    }
    const face = size('face');
    assert.deepEqual(size('face-map'), face);
    assert.ok(face[1] > face[0], 'the face is taller than wide');
});
