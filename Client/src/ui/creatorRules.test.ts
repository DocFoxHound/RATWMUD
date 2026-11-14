// The creator's rules (creatorRules.ts; the user, 2026-11-14): the name's limit, the fur zones' colours and their Reset,
// and the Gift tiers greyed by the account's unlocks.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {nameProblem, NameMost, resetZone, setZoneColour, tierCards, zoneChanged, zoneCoat} from './creatorRules.ts';
import type {Fur} from './fur.ts';

test('a name is 2 to 24 characters, as Core and central count them', () => {
    assert.equal(NameMost, 24);
    const core = readFileSync(new URL('../../../Core/RatwIdentity.h', import.meta.url), 'utf8');
    assert.match(core, /NewNameMost = 24;/, 'the same limit as the game\'s creation and central\'s check');
    assert.equal(nameProblem('Ab'), '');
    assert.equal(nameProblem('A'.repeat(24)), '');
    assert.match(nameProblem('A'.repeat(25)), /2 to 24/);
    assert.match(nameProblem('A'), /2 to 24/);
    assert.equal(nameProblem('É' + 'x'.repeat(23)), '', 'a letter beyond ASCII counts one');
});

const timber: Fur = {coat: ['soot', 'ash', 'cream'], eyes: 'amber'};

test('a fur zone wears the coat until one of its colours is changed, and Reset gives it the coat again', () => {
    assert.deepEqual(zoneCoat(timber, 'legs'), ['soot', 'ash', 'cream'], 'every zone starts as the coat');
    assert.equal(zoneChanged(timber, 'legs'), false);
    const changed = setZoneColour(timber, 'legs', 2, 'snow');
    assert.deepEqual(changed.zones, {legs: ['soot', 'ash', 'snow']}, 'only the light fur changed, the rest the coat\'s');
    assert.deepEqual(changed.coat, timber.coat, 'the coat itself is untouched');
    assert.equal(zoneChanged(changed, 'legs'), true);
    const twice = setZoneColour(changed, 'legs', 0, 'black');
    assert.deepEqual(twice.zones?.legs, ['black', 'ash', 'snow'], 'a second change starts from the zone\'s own');
    const both = setZoneColour(twice, 'tail_tip', 1, 'mist');
    assert.deepEqual(Object.keys(both.zones ?? {}).sort(), ['legs', 'tail_tip']);
    const reset = resetZone(both, 'legs');
    assert.deepEqual(reset.zones, {tail_tip: ['soot', 'mist', 'cream']}, 'Reset gives that zone the coat\'s scheme, the others keep theirs');
    assert.deepEqual(zoneCoat(reset, 'legs'), timber.coat);
    assert.equal(resetZone(reset, 'tail_tip').zones, undefined, 'all reset: nothing of its own kept');
    const back = setZoneColour(changed, 'legs', 2, 'cream');
    assert.equal(back.zones, undefined, 'a zone changed back to the coat\'s colours is the coat\'s again');
    // A new coat carries every zone that has none of its own.
    const recoated: Fur = {...changed, coat: ['russet', 'tan', 'cream']};
    assert.deepEqual(zoneCoat(recoated, 'back'), ['russet', 'tan', 'cream']);
});

test('the Gift tiers: Normal, Gifted and Quickened, each greyed with the reason when the account hasn\'t opened it', () => {
    const open = (cards: ReturnType<typeof tierCards>) => cards.map(c => [c.tier, c.open]);
    assert.deepEqual(open(tierCards('world', null)), [['normal', true], ['gifted', true], ['quickened', true]], 'an older server: all open');
    const fresh = {gifted: {open: false, message: "Gifted isn't open to your account yet: Social level 1 of 3.",
        progress: [{label: 'Social level 1 of 3'}, {label: 'Scenes as a Normal wolf: 4 of 10'}]},
        quickened: {open: false, message: "Quickened isn't open to your account yet: 62 of 100 stars.", progress: [{label: 'Stars: 62 of 100'}]}};
    const cards = tierCards('world', fresh);
    assert.deepEqual(open(cards), [['normal', true], ['gifted', false], ['quickened', false]]);
    assert.match(cards[1].reason, /Social level 1 of 3/);
    assert.deepEqual(cards[1].progress, ['Social level 1 of 3', 'Scenes as a Normal wolf: 4 of 10']);
    assert.deepEqual(cards[2].progress, ['Stars: 62 of 100']);
    const giftedOnly = tierCards('world', {gifted: {open: true}, quickened: {open: false, message: 'Not yet: 62 of 100 stars.'}});
    assert.deepEqual(open(giftedOnly), [['normal', true], ['gifted', true], ['quickened', false]]);
    assert.equal(giftedOnly[2].reason, 'Not yet: 62 of 100 stars.');
    // Quickened needs Gifted (Core/RatwGame.cpp, arrivalChoices), whatever its own record says.
    const odd = tierCards('world', {gifted: {open: false, message: 'Gifted first.'}, quickened: {open: true}});
    assert.deepEqual(open(odd), [['normal', true], ['gifted', false], ['quickened', false]]);
    assert.equal(odd[2].reason, 'Gifted first.');
    assert.deepEqual(open(tierCards('world', {gifted: {open: true}, quickened: {open: true}})), [['normal', true], ['gifted', true], ['quickened', true]]);
    // On central: Gifted is open to any wolf with a family; Quickened is the account's, once for every world (doc 49,
    // 9.1): greyed with what it asks until central says the account has it, then open.
    const central = tierCards('central', null, {quickened: {level: 8, stars: 100, givers: 30, stories: 2}});
    assert.deepEqual(open(central), [['normal', true], ['gifted', true], ['quickened', false]]);
    assert.match(central[2].reason, /social level 8, 100 stars from 30 different wolves and 2 closed Stories/);
    assert.match(central[2].reason, /whole account, in every world/);
    const opened = tierCards('central', null, {quickened: {level: 8}}, true);
    assert.deepEqual(open(opened), [['normal', true], ['gifted', true], ['quickened', true]], 'the account has it: open');
    assert.equal(opened[2].reason, '');
});
