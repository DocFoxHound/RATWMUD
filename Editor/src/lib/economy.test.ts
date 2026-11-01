import {test} from 'node:test';
import assert from 'node:assert/strict';
import {basketRows, bulk, choices, comfortOf, comfortsText, filtered, goodsBulk, goodsShown, goodsText, markupOf, percent,
    percentsOf, percentsText, ruleChange, ruleOf, RULES, ruleValue, seasonal, seasonChange, serviceChange, serviceText,
    serviceValue, switchText, wholeOf, wouldChange, type BasketGood, type CountryGood, type RuleControls, type SeasonGroup,
    type StoreBusiness} from './economy.ts';

// The seasons (doc 68, Progress, "Seasons and the DM's rules"): what the DM's and Atlas's panels read and send.
const grain: SeasonGroup = {id: 'grain', name: 'grain and flour', about: '', goods: ['flour', 'bread'], changed: false,
    supply: [85, 115, 140, 75], price: [110, 95, 85, 115], shelf: [100, 100, 100, 100], normal: 150,
    file: {supply: [85, 115, 140, 75], price: [110, 95, 85, 115], shelf: [100, 100, 100, 100], normal: 150}};

test('four percents are typed in any of the usual ways, within their limits', () => {
    assert.deepEqual(percentsOf('50 100 150 40', 0, 1000), [50, 100, 150, 40]);
    assert.deepEqual(percentsOf('50/100/150/40', 0, 1000), [50, 100, 150, 40]);
    assert.deepEqual(percentsOf(' 50%, 100%, 150%, 40% ', 0, 1000), [50, 100, 150, 40]);
    assert.equal(percentsOf('50 100 150', 0, 1000), null, 'four, one a season');
    assert.equal(percentsOf('5 100 100 100', 10, 1000), null, 'a price under 10%');
    assert.equal(percentsOf('50 100 150 4.5', 0, 1000), null);
    assert.equal(percentsText([50, 100, 150, 40]), '50 / 100 / 150 / 40');
});

test('a group\'s factor by day, as the game reckons it (Society::seasonal)', () => {
    const v = [10, 20, 30, 40];
    assert.equal(seasonal(v, 46), .1);
    assert.equal(seasonal(v, 138), .2);
    assert.equal(seasonal(v, 229.5), .3);
    assert.equal(seasonal(v, 320), .4);
    assert.ok(Math.abs(seasonal(v, 92) - .15) < 1e-9, 'halfway between two middles');
    assert.ok(Math.abs(seasonal(v, 0) - (.4 + (.1 - .4) * 45 / 91)) < 1e-9, 'winter runs on to spring across the year\'s turn');
});

test('a change is sent only when it changes something', () => {
    assert.equal(seasonChange(grain, 'supply', [85, 115, 140, 75]), null);
    assert.deepEqual(seasonChange(grain, 'supply', [85, 115, 140, 50]), {set: {grain: {supply: [85, 115, 140, 50]}}});
    assert.equal(seasonChange(grain, 'normal', 150), null);
    assert.deepEqual(seasonChange(grain, 'normal', 0), {set: {grain: {normal: 0}}});
});

test('the comforts: the world\'s own, or country.json\'s', () => {
    const file = {comfortsAbove: null, comfortsShare: null, fileAbove: 60, fileShare: 10};
    assert.match(comfortsText(file), /more than 60p a head spends 10% .* \(country\.json's\)/);
    assert.match(comfortsText({...file, comfortsAbove: 40}), /more than 40p a head spends 10%/);
    assert.equal(comfortOf('', 100), null, 'empty: back to country.json\'s');
    assert.equal(comfortOf('25', 100), 25);
    assert.equal(comfortOf('101', 100), undefined);
});

// The rules and the services (doc 68, Progress, "First fixes"; migration 0058).
const trim = {id: 'groom_and_trim', name: 'A groom and trim', business: 'barber', price: 1, every: 4};
const rules: RuleControls = {rulesSupported: true, rulesProblem: '', rules: {almsADay: 12}, services: {},
    ruleDefaults: {comfortsMost: 200, almsADay: 10, treasuryCap: 2000, wageSupport: 100, workBankHours: 0, fairFood: 1, services: 1,
        servicesCityShare: 30, suppliers: 1, patrons: 1, armourEvery: 120, buyAheadDays: 1, floatDrawDays: 7, floatWagesDays: 7,
        subsidyToNeed: 1}, serviceList: [trim]};

test('a rule is the world\'s own or country.json\'s, and a change is sent only when it changes something', () => {
    assert.deepEqual(RULES.map(r => r.key).sort(), Object.keys(rules.ruleDefaults).sort(), 'every rule of country.json listed');
    assert.equal(ruleValue(rules, 'almsADay'), 12);
    assert.equal(ruleValue(rules, 'treasuryCap'), 2000);
    assert.equal(ruleChange(rules, 'almsADay', 12), null);
    assert.deepEqual(ruleChange(rules, 'almsADay', 7), {set: {almsADay: 7}});
    assert.deepEqual(ruleChange(rules, 'almsADay', null), {set: {almsADay: null}}, 'back to country.json\'s');
    assert.equal(ruleChange(rules, 'treasuryCap', null), null, 'already country.json\'s');
    assert.equal(ruleChange(rules, 'treasuryCap', 2000), null, 'country.json\'s value typed in: nothing to change');
    assert.deepEqual(ruleChange(rules, 'fairFood', 0), {set: {fairFood: 0}});
    assert.equal(ruleOf('', 1000), null);
    assert.equal(ruleOf('15', 1000), 15);
    assert.equal(ruleOf('1001', 1000), undefined);
});

test('a service: country.json\'s until changed, one of its three at a time', () => {
    assert.deepEqual(serviceValue(rules, trim), {on: true, price: 1, every: 4});
    assert.equal(serviceChange(rules, trim, 'price', 1), null);
    assert.deepEqual(serviceChange(rules, trim, 'on', false), {set: {groom_and_trim: {on: false}}});
    assert.match(serviceText(rules, trim), /barber, 1p, about once every 4 days .*\(country\.json's\)/);
    const changed = {...rules, services: {groom_and_trim: {on: false, price: 2, every: 1}}};
    assert.deepEqual(serviceValue(changed, trim), {on: false, price: 2, every: 1});
    assert.match(serviceText(changed, trim), /2p, about once every 1 day a .* \(off\)$/);
    assert.equal(serviceChange(changed, trim, 'every', 1), null);
});

const shop = (npc: string, business: string | null, district: string, over: Partial<StoreBusiness> = {}): StoreBusiness => ({
    npc, name: npc, label: `${business} keeper`, business, district, interior: 'A shop', making: [], resale: [], internalStore: true,
    effective: 'on', why: '', ...over});
const smith = shop('rm_smith', 'smithy', 'works', {making: [{item: 'iron_bar', price: 9, cost: 9}, {item: 'charcoal', price: 2, cost: 2}]});
const stall = shop('rm_stall', 'stall', 'commons', {resale: [{item: 'flour', price: 1, cost: 1}], internalStore: false,
    effective: 'off (this business)'});
const odd = shop('rm_odd', null, 'commons', {resale: [{item: 'meal', price: 6, cost: 5}]});
const all = [smith, stall, odd];

test('the table filters by business and district', () => {
    assert.deepEqual(filtered(all, {business: '', district: ''}), all);
    assert.deepEqual(filtered(all, {business: 'smithy', district: ''}), [smith]);
    assert.deepEqual(filtered(all, {business: '', district: 'commons'}), [stall, odd]);
    assert.deepEqual(filtered(all, {business: 'none', district: 'commons'}), [odd]);
    assert.deepEqual(choices(all), {businesses: [['none', 1], ['smithy', 1], ['stall', 1]], districts: [['commons', 2], ['works', 1]]});
});

test('"All on" and "All off" send the filter the host applies, or the rows themselves for no business', () => {
    assert.deepEqual(bulk(false, {business: '', district: ''}, all), {all: false});
    assert.deepEqual(bulk(true, {business: 'smithy', district: 'works'}, [smith]), {all: true, business: 'smithy', district: 'works'});
    assert.deepEqual(bulk(false, {business: 'none', district: ''}, [odd]), {set: {rm_odd: false}});
    assert.equal(wouldChange(all, true), 1);
    assert.equal(wouldChange(all, false), 2);
});

test('goods, switches and markups as the page says them', () => {
    assert.equal(goodsText(smith, {internalStore: true, storeResale: true}), 'making: iron_bar 9p, charcoal 2p');
    assert.equal(goodsText(smith, {internalStore: false, storeResale: true}), 'making (off): iron_bar 9p, charcoal 2p');
    assert.equal(goodsText(shop('x', 'shipwright', 'wharf'), {internalStore: true, storeResale: true}), 'draws nothing');
    assert.equal(switchText('making', true), 'Making from the store room: on for this world');
    assert.equal(switchText('resale', false), 'Vendors restock from the store room: off');
    assert.match(switchText('rule', false), /the city's loop needs it/);
    assert.equal(percent(75), '75%');
    assert.equal(markupOf('120%'), 120);
    assert.equal(markupOf(' 0 '), 0);
    assert.equal(markupOf('1001'), null);
    assert.equal(markupOf('7.5'), null);
    assert.equal(markupOf('abc'), null);
    assert.equal(markupOf(''), null);
});

test('the Country: its basket by share, its goods searched and switched, the loop\'s numbers typed in', () => {
    const basket: BasketGood[] = [{item: 'nails', name: 'Nails', share: 10, percent: 25, price: 3, makers: ['smithy']},
        {item: 'cider', name: 'Cider', share: 30, percent: 75, price: 3, makers: []}];
    assert.deepEqual(basketRows(basket).rows.map(b => b.item), ['cider', 'nails']);
    assert.equal(basketRows(basket).unmade, 1);
    const goods: CountryGood[] = [{item: 'iron_bar', name: 'Iron bar', businesses: 3, supplied: true},
        {item: 'fish', name: 'Fish', businesses: 4, supplied: false}];
    assert.deepEqual(goodsShown(goods, 'IRON').map(g => g.item), ['iron_bar']);
    assert.deepEqual(goodsShown(goods, ''), goods);
    assert.deepEqual(goodsBulk(true, goods, goods), {all: true});
    assert.deepEqual(goodsBulk(false, [goods[0]], goods), {set: {iron_bar: false}});
    assert.equal(wholeOf('25%', 90), 25);
    assert.equal(wholeOf('14p', 1000), 14);
    assert.equal(wholeOf('91', 90), null);
    assert.equal(wholeOf('2.5', 90), null);
});
