import {test} from 'node:test';
import assert from 'node:assert/strict';
import {basketRows, bulk, choices, filtered, goodsBulk, goodsShown, goodsText, markupOf, percent, switchText, wholeOf, wouldChange,
    type BasketGood, type CountryGood, type StoreBusiness} from './economy.ts';

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
