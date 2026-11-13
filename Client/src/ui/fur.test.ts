// The painted wolves' colours (fur.ts; Docs/Design/81-painted-wolves.md): the palette's rule, the ramp, the blend
// between fur zones, a wolf's fur as it is checked, and the painted look an older wolf is given.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {buildOf, BuildNames, Builds, BuildStature, coatOf, EyeColours, FurColours, furOf, furRamp, furRgb, FurZones, hexRgb, isEyeColour, isFurColour, nearestEye, nearestFur,
    olderLook, paintFur, RampStops, readFur, ReadyCoats, zoneRamps, type Fur, type FurSource, type OlderLook, type Rgb} from './fur.ts';
import {readAppearance} from './portrait.ts';

const older: OlderLook = {species: 'timber', stature: 'average', pattern: 'saddle', baseColor: 3, gradientColor: 1, markingColor: 5,
    gradientAmount: 0.35, patternAmount: 0.55};
const nine = {...older, sex: 'male'};

test('the palette: one file, about thirty muted fur colours and ten natural eyes, every ready coat made of them', () => {
    const file = JSON.parse(readFileSync(new URL('../../../Data/Wolves/palette.json', import.meta.url), 'utf8'));
    assert.deepEqual(FurColours.map(c => [c.id, c.name, c.group, c.hex]), file.fur.map((c: any) => [c.id, c.name, c.group, c.hex]), 'run tools/fur_palette.py');
    assert.deepEqual(EyeColours.map(c => [c.id, c.hex]), file.eyes.map((c: any) => [c.id, c.hex]));
    // Core's table is of the same file (Core/RatwFurPalette.inc), in the same order.
    const core = readFileSync(new URL('../../../Core/RatwFurPalette.inc', import.meta.url), 'utf8');
    assert.deepEqual([...core.matchAll(/^RATW_FUR\("([a-z_]+)", "[^"]+", 0x([0-9a-f]{6})\)$/gm)].map(m => [m[1], `#${m[2]}`]),
        FurColours.map(c => [c.id, c.hex]), 'run tools/fur_palette.py');
    assert.deepEqual([...core.matchAll(/^RATW_EYE\("([a-z_]+)"/gm)].map(m => m[1]), EyeColours.map(c => c.id));
    assert.deepEqual([...core.matchAll(/^RATW_FUR_ZONE\("([a-z_]+)"/gm)].map(m => m[1]), FurZones.map(z => z.id));
    assert.deepEqual(FurZones.map(z => z.id), ['back', 'sides', 'chest', 'legs', 'tail', 'tail_tip', 'head', 'muzzle']);
    assert.ok(FurColours.length >= 28 && FurColours.length <= 36 && EyeColours.length >= 9 && EyeColours.length <= 12);
    assert.equal(new Set(FurColours.map(c => c.id)).size, FurColours.length);
    const spread = (hex: string) => Math.max(...hexRgb(hex)) - Math.min(...hexRgb(hex));
    for (const c of FurColours) assert.ok(spread(c.hex) <= 100, `${c.name} is muted`);
    for (const c of EyeColours) assert.ok(spread(c.hex) <= 160, `${c.name} eyes are natural`);
    for (const group of ['Blacks and charcoals', 'Greys and silvers', 'Whites and creams', 'Tans and buffs', 'Browns', 'Russets and muted reds',
        'Ochres and golds', 'Slate and dusk blues', 'Green-greys'])
        assert.ok(FurColours.some(c => c.group === group), group);
    assert.ok(ReadyCoats.length >= 5);
    for (const coat of ReadyCoats) assert.ok(readFur({coat: coat.coat, eyes: coat.eyes, ...(coat.zones ? {zones: coat.zones} : {})}), coat.name);
    assert.ok(isFurColour('russet') && !isFurColour('amber') && !isFurColour('#8a4a2c') && isEyeColour('amber') && !isEyeColour('russet'));
});

test('the ramp: the painting\'s dark, mid and light fur become the three colours, and its brushwork stays between them', () => {
    const dark: Rgb = [40, 30, 20], mid: Rgb = [140, 100, 60], light: Rgb = [230, 220, 200];
    const ramp = furRamp(dark, mid, light);
    const at = (l: number) => [ramp[l * 3], ramp[l * 3 + 1], ramp[l * 3 + 2]];
    assert.equal(ramp.length, 768);
    assert.deepEqual(RampStops, [0, 38, 118, 212, 255]);
    assert.deepEqual(at(38), dark);
    assert.deepEqual(at(118), mid);
    assert.deepEqual(at(212), light);
    assert.deepEqual(at(0), [18, 14, 9], 'the deepest shadow is the dark fur, darker');
    assert.deepEqual(at(255), [243, 238, 228], 'the brightest light is the light fur, paler, never white');
    assert.deepEqual(at(78), [90, 65, 40], 'half way from dark to mid');
    for (let l = 1; l < 256; ++l) for (let c = 0; c < 3; ++c) assert.ok(ramp[l * 3 + c] >= ramp[(l - 1) * 3 + c], 'lighter paint is never darker fur');
});

test('the zone blend: each pixel its zone\'s ramp, two zones mixed where the map feathers them, eyes and ink apart', () => {
    const fur: Fur = {coat: ['soot', 'ash', 'cream'], eyes: 'amber', zones: {legs: ['silver', 'mist', 'snow']}};
    const ramps = zoneRamps(fur);
    assert.equal(ramps.length, 8);
    assert.equal(ramps[0], ramps[1], 'zones with no colours of their own share the coat\'s ramp');
    assert.notEqual(ramps[3], ramps[0]);
    const grey = (l: number, alpha = 255) => [l, l, l, alpha];
    // Six pixels of mid fur (luminance 118): sides; legs; half sides and half legs; an iris; ink; and one off the wolf.
    const picture = new Uint8ClampedArray([...grey(118), ...grey(118), ...grey(118), 200, 150, 40, 255, 20, 22, 24, 255, ...grey(118, 0)]);
    const map = new Uint8ClampedArray([1, 0, 0, 255, 3, 0, 0, 255, 1 | (3 << 4), 128, 0, 255, 6, 0, 255, 255, 7, 0, 127, 255, 1, 0, 0, 255]);
    const source: FurSource = {width: 6, height: 1, picture, map};
    const out = new Uint8ClampedArray(24);
    paintFur(source, ramps, [217, 164, 65], out);
    const pixel = (i: number) => Array.from(out.slice(i * 4, i * 4 + 4));
    const ash = furRgb('ash'), mist = furRgb('mist');
    assert.deepEqual(pixel(0), [...ash, 255], 'mid fur on the sides is the coat\'s mid colour');
    assert.deepEqual(pixel(1), [...mist, 255], 'mid fur on the legs is the legs\' own');
    const half = pixel(2);
    for (let c = 0; c < 3; ++c) assert.ok(Math.abs(half[c] - (ash[c] + mist[c]) / 2) <= 1.5, 'a feathered pixel is between the two');
    assert.ok(pixel(3)[0] > pixel(3)[2] + 80, 'the iris takes the eye colour, not the fur\'s');
    assert.deepEqual(pixel(4), [20, 22, 24, 255], 'ink (the nose, the eye\'s rim) stays as painted');
    assert.deepEqual(pixel(5), [0, 0, 0, 0], 'nothing off the wolf');
    // The same look painted again is the same picture.
    const again = new Uint8ClampedArray(24);
    paintFur(source, zoneRamps(fur), [217, 164, 65], again);
    assert.deepEqual(again, out);
});

test('a wolf\'s fur is checked as the server checks it', () => {
    assert.deepEqual(readFur({coat: ['soot', 'ash', 'cream'], eyes: 'amber'}), {coat: ['soot', 'ash', 'cream'], eyes: 'amber'});
    assert.ok(readFur({coat: ['soot', 'ash', 'cream'], eyes: 'amber', zones: {tail_tip: ['black', 'soot', 'charcoal']}}));
    for (const bad of [null, 'timber', {coat: ['soot', 'ash'], eyes: 'amber'}, {coat: ['soot', 'ash', 'neon'], eyes: 'amber'},
        {coat: ['soot', 'ash', '#ff00ff'], eyes: 'amber'}, {coat: ['soot', 'ash', 'cream'], eyes: 'purple'}, {coat: ['soot', 'ash', 'cream']},
        {coat: ['soot', 'ash', 'cream'], eyes: 'amber', zones: {}}, {coat: ['soot', 'ash', 'cream'], eyes: 'amber', zones: {wings: ['soot', 'ash', 'cream']}},
        {coat: ['soot', 'ash', 'cream'], eyes: 'amber', zones: {legs: ['soot', 'ash']}}, {coat: ['soot', 'ash', 'cream'], eyes: 'amber', extra: 1}])
        assert.equal(readFur(bad), null, JSON.stringify(bad));
    assert.ok(readAppearance({...nine, build: 'heavy', fur: {coat: ['soot', 'ash', 'cream'], eyes: 'amber'}})?.fur);
    for (const build of ['small', 'medium', 'large', 'lean', 'average', 'heavy']) assert.ok(readAppearance({...nine, build}), `${build} is a build`);
    assert.equal(readAppearance({...nine, build: 'huge'}), null, 'an unknown build is no look');
    assert.equal(readAppearance({...nine, fur: {coat: ['soot', 'ash', 'lime'], eyes: 'amber'}}), null, 'a look off the palette is no look');
    assert.equal(readAppearance(nine)?.fur, undefined, 'an older look has none');
});

test('an older wolf is painted from what it had: nearest palette colours, markings as zones, nothing saved', () => {
    const fur = furOf(older);
    assert.ok(readFur(fur), 'the fur it is given is a fur it could have chosen');
    assert.deepEqual(furOf(older), fur, 'the same older look is always the same fur');
    assert.equal(fur.coat[1], nearestFur('#8e8271'), 'the coat colour is the mid fur');
    assert.equal(fur.eyes, 'amber');
    assert.ok(fur.zones?.back, 'a saddle pattern is the back');
    assert.ok(fur.zones?.legs && fur.zones.chest, 'the belly-and-legs colour is the chest and the legs');
    assert.deepEqual(furOf({...older, pattern: 'solid', gradientAmount: 0}).zones, undefined, 'a plain coat has no zones of its own');
    // Markings, where their names are a zone's.
    const marked = furOf({...older, coat: '#303534', gradientAmount: 0, eyes: '#5f9fd0', markings: [{mask: 'socks', color: '#f4efe6', opacity: 1},
        {mask: 'tail_tip', color: '#f4efe6', opacity: 1}, {mask: 'mask', color: '#a26843', opacity: 0.9}, {mask: 'bib', color: '#f4efe6', opacity: 1},
        {mask: 'cape', color: '#1a1612', opacity: 1}, {mask: 'freckles', color: '#ffffff', opacity: 1}]});
    assert.deepEqual(Object.keys(marked.zones ?? {}).sort(), ['back', 'chest', 'head', 'legs', 'tail_tip']);
    assert.equal(marked.zones?.legs[1], nearestFur('#f4efe6'));
    assert.equal(marked.eyes, 'blue', 'the nearest eye colour');
    assert.equal(nearestEye('#d9a441'), 'amber');
    assert.deepEqual(coatOf(furRgb('charcoal')).map(isFurColour), [true, true, true]);
    assert.equal(coatOf(furRgb('charcoal'))[1], 'charcoal');
    // The painting: the size its build names; an older build's name keeps the painting that took its place; with no
    // build, its stature.
    assert.deepEqual(Builds.map(b => [b, BuildNames[b], BuildStature[b]]), [['small', 'Small', 'short'], ['medium', 'Medium', 'average'], ['large', 'Large', 'tall']]);
    for (const size of Builds) assert.equal(buildOf({...older, stature: 'average', build: size}), size);
    assert.deepEqual(['lean', 'heavy', 'average'].map(build => buildOf({...older, stature: 'short', build})), ['large', 'medium', 'small'],
        'a wolf saved with one of the first paintings keeps it, whatever its stature');
    assert.equal(buildOf(older), 'medium');
    assert.equal(buildOf({...older, stature: 'tall'}), 'large');
    assert.equal(buildOf({...older, stature: 'short', fur: {coat: ['soot', 'ash', 'cream'], eyes: 'amber'}}), 'small', 'a painted wolf with no build, too');
    // A chosen fur is kept as chosen, and the older fields are written to match it.
    const chosen: Fur = {coat: ['black', 'charcoal', 'silver'], eyes: 'gold', zones: {legs: ['silver', 'mist', 'snow'], tail: ['rust', 'sand', 'ivory']}};
    assert.equal(furOf({...older, fur: chosen}), chosen);
    const o = olderLook(chosen);
    assert.match(o.coat, /^#[0-9a-f]{6}$/);
    assert.equal(o.gradientTint, '#adb1b0');
    assert.deepEqual(o.markings.map(m => m.mask), ['stockings'], 'a zone an older marking can stand for is one');
    assert.ok(readAppearance({...nine, pattern: 'solid', build: 'large', stature: 'tall', fur: chosen, coat: o.coat, gradientTint: o.gradientTint, eyes: o.eyes, markings: o.markings}),
        'and with them the look is one the server takes');
});

test('the paintings: one face, and a profile for each size in one shared frame, each with its map', () => {
    // A PNG's width and height are the two big-endian numbers after its signature and the IHDR chunk's head.
    const size = (name: string) => {
        const bytes = readFileSync(new URL(`../assets/wolves/${name}.png`, import.meta.url));
        assert.equal(bytes.toString('latin1', 1, 4), 'PNG', name);
        return [bytes.readUInt32BE(16), bytes.readUInt32BE(20)];
    };
    const frame = size('large-profile');
    for (const build of Builds) {
        assert.deepEqual(size(`${build}-profile`), frame, `the ${build} profile is in the shared frame (tools/avatar_prep.py), so the sizes keep their scale`);
        assert.deepEqual(size(`${build}-profile-map`), frame);
    }
    assert.ok(frame[0] > frame[1], 'a profile is wider than tall');
    const face = size('face');
    assert.deepEqual(size('face-map'), face);
    assert.ok(face[1] > face[0], 'the face is taller than wide');
});
