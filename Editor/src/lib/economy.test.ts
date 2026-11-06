import {test} from 'node:test';
import assert from 'node:assert/strict';
import {areaOnFrom, areaSwitch, basketRows, bulk, choices, comfortOf, comfortsText, driftLines, farmChange, farmNow, filtered,
    goodsBulk, goodsShown, goodsText, markupOf, percent, percentsOf, percentsText, ruleChange, ruleOf, RULES, ruleValue, scheduleText,
    seasonal, seasonChange, serviceChange, serviceText, serviceValue, switchText, wholeOf, wouldChange, yieldText, type BasketGood,
    type WharfGood, type FarmAreaDefault, type FarmControls, type RuleControls, type SeasonGroup, type StoreBusiness} from './economy.ts';

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

test('the comforts: the world\'s own, or city_loop.json\'s', () => {
    const file = {comfortsAbove: null, comfortsShare: null, fileAbove: 60, fileShare: 10};
    assert.match(comfortsText(file), /more than 60p a head spends 10% .* \(city_loop\.json's\)/);
    assert.match(comfortsText({...file, comfortsAbove: 40}), /more than 40p a head spends 10%/);
    assert.equal(comfortOf('', 100), null, 'empty: back to city_loop.json\'s');
    assert.equal(comfortOf('25', 100), 25);
    assert.equal(comfortOf('101', 100), undefined);
});

// The rules and the services (doc 68, Progress, "First fixes"; migration 0058).
const trim = {id: 'groom_and_trim', name: 'A groom and trim', business: 'barber', price: 1, every: 4};
const rules: RuleControls = {rulesSupported: true, rulesProblem: '', rules: {almsADay: 12}, services: {},
    ruleDefaults: {comfortsMost: 200, almsADay: 10, almsNoEarner: 150, treasuryCap: 2000, wageSupport: 100, workBankHours: 0, fairFood: 1, services: 1,
        servicesCityShare: 30, suppliers: 1, patrons: 1, armourEvery: 120, buyAheadDays: 1, floatDrawDays: 7, floatWagesDays: 7, workshopWagesDays: 2, workshopWageMost: 2,
        subsidyToNeed: 1, estatePostsDays: 2, estates: 1, landRent: 16, landRentFish: 9, landRentFruit: 4, landRentGrain: 28, landRentSalt: 8, landRentDairy: 32,
        landRentThin: 0, farmSurplus: 1, shopRot: 1, luxuryShare: 10, luxuryMost: 1, luxuryAbove: 200, luxuryHouses: 40, luxuryHousesMost: 24, luxuryPriceBand: 700, luxuryPriceStep: 50,
        surplus: 1, surplusKeep: 4000, surplusWorks: 20, surplusAlms: 80, surplusAlmsTo: 40,
        stockPrices: 1, stockPriceBand: 25, stockPriceStep: 10, shelfFollow: 1, shelfDays: 7, shelfCover: 125, shelfGrow: 25,
        wageDrift: 1, wageDriftDays: 28, wageDriftSpare: 50, wageDriftMost: 12, wageDriftHandMost: 12, wageDriftFloor: 7, treasuryDrift: 1,
        treasuryTarget: 5500, treasuryBand: 15, treasuryDays: 182, treasuryJudgeDays: 28, tollStep: 5, tollDriftMost: 1, almsDriftMost: 2,
        wharfValve: 1, wharfFee: 10, wharfKeepDays: 28, wharfKeepLeast: 20, wharfImportScale: 100, wharfTakeScale: 100, wharfExportMost: 300,
        wharfCreditMost: 2000, wharfInjectADay: 0, wharfTakeADay: 0, wharfActionMost: 2000},
    serviceList: [trim]};

test('a rule is the world\'s own or city_loop.json\'s, and a change is sent only when it changes something', () => {
    assert.deepEqual(RULES.map(r => r.key).sort(), Object.keys(rules.ruleDefaults).sort(), 'every rule of city_loop.json listed');
    assert.equal(ruleValue(rules, 'almsADay'), 12);
    assert.equal(ruleValue(rules, 'treasuryCap'), 2000);
    assert.equal(ruleChange(rules, 'almsADay', 12), null);
    assert.deepEqual(ruleChange(rules, 'almsADay', 7), {set: {almsADay: 7}});
    assert.deepEqual(ruleChange(rules, 'almsADay', null), {set: {almsADay: null}}, 'back to city_loop.json\'s');
    assert.equal(ruleChange(rules, 'treasuryCap', null), null, 'already city_loop.json\'s');
    assert.equal(ruleChange(rules, 'treasuryCap', 2000), null, 'city_loop.json\'s value typed in: nothing to change');
    assert.deepEqual(ruleChange(rules, 'fairFood', 0), {set: {fairFood: 0}});
    assert.equal(ruleOf('', 1000), null);
    assert.equal(ruleOf('15', 1000), 15);
    assert.equal(ruleOf('1001', 1000), undefined);
});

test('where the self-correcting rules stand: a line each, nothing before the world has a save', () => {
    assert.deepEqual(driftLines(undefined), []);
    assert.deepEqual(driftLines({saved: null, problem: 'The world has no save yet: nothing has drifted.', tollBase: 30, almsBase: 10}), []);
    const lines = driftLines({saved: '2026-11-01 10:00', problem: '', tollBase: 30, almsBase: 10, toll: 28.5, alms: 11, level: 5612,
        target: 5000, prices: {on: true, moved: 120, mean: 1.02, low: 0.8, high: 1.25}, shelves: {on: true, wares: 400, mean: 12.5},
        wages: {on: false, tills: 110, up: 30, down: 5, mean: 0.9, moved: []}});
    assert.equal(lines.length, 4);
    assert.match(lines[0], /toll 28\.5% \(the world's 30%\), the alms 11p a head \(10p\): the treasury's level 5612p against its target 5000p/);
    assert.match(lines[1], /120 wares .* mean 1\.02, from 0\.8 to 1\.25\.$/);
    assert.match(lines[3], /30 tills up, 5 down of 110, mean 0\.9p a post \(the rule is off\)\.$/);
});

test('a service: city_loop.json\'s until changed, one of its three at a time', () => {
    assert.deepEqual(serviceValue(rules, trim), {on: true, price: 1, every: 4});
    assert.equal(serviceChange(rules, trim, 'price', 1), null);
    assert.deepEqual(serviceChange(rules, trim, 'on', false), {set: {groom_and_trim: {on: false}}});
    assert.match(serviceText(rules, trim), /barber, 1p, about once every 4 days .*\(city_loop\.json's\)/);
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
    const goods: WharfGood[] = [{item: 'iron_bar', name: 'Iron bar', businesses: 3, supplied: true},
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

// The Country's food farms (doc 71, 4; migration 0060): applied as Core's Society::buildFarms applies them.
const grainArea: FarmAreaDefault = {id: 'grain', name: 'grain and flour', goods: ['flour', 'oats'], on: null, yieldScale: 100, crewScale: 100,
    keeper: null, hand: null, price: 'year', priceFactor: 101, revenueADay: [209, 198, 225, 202],
    farms: [{id: 'grain_farm_1', kind: 'grain farm', region: '', keepers: 1, hands: 4, keeper: null, hand: null, reserve: 112, purse: 295,
        basketADay: 48.3, yield: {flour: [52.9, 60.4, 61.7, 46.8]}},
        {id: 'grain_farm_2', kind: 'grain farm', region: '', keepers: 1, hands: 4, keeper: null, hand: null, reserve: 112, purse: 295,
        basketADay: 48.3, yield: {flour: [52.9, 60.4, 61.7, 46.8]}}]};
const farmsOf = (farms: FarmControls['farms']): FarmControls => ({farmsSupported: true, farmsProblem: '', farms,
    farmDefaults: {start: -1, every: 28, order: ['grain', 'fruit', 'meat'], keeper: 16, hand: 10, floatDays: 2, areas: [grainArea]}});

test('a farm\'s crew, wages and yields fall through the farm, its area and the defaults, as the game reckons them', () => {
    const c = farmsOf({keeper: 15, areas: {grain: {keeper: 18, crewScale: 150, yieldScale: 120}},
        farms: {grain_farm_1: {hand: 12, yieldScale: 90}, grain_farm_2: {crewScale: 50}}});
    const one = farmNow(c, grainArea, grainArea.farms[0]), two = farmNow(c, grainArea, grainArea.farms[1]);
    assert.deepEqual([one.keeper, one.hand, one.hands, one.wages], [18, 12, 6, 18 + 6 * 12], 'the farm\'s hand, the area\'s keeper, 4 hands at 150%');
    assert.deepEqual([two.keeper, two.hand, two.hands], [18, 10, 3], '150% of 50%: 3 hands (rounded half up), the defaults\' hand');
    assert.ok(Math.abs(one.yieldPercent - 108) < 1e-9, '120% of 90%');
    assert.equal(one.reserve, 2 * one.wages, 'its float: two days of its wages');
    assert.equal(one.purse, Math.round(295 * 7 / 5), 'its crew\'s purse by its size');
    assert.equal(yieldText(grainArea.farms[0], 50), 'flour 26.4/30.2/30.9/23.4');
    const plain = farmNow(farmsOf({}), grainArea, grainArea.farms[0]);
    assert.deepEqual([plain.keeper, plain.hand, plain.hands, plain.wages, plain.reserve], [16, 10, 4, 56, 112], 'country_farms.json\'s');
});

test('the switch: the schedule\'s day for an area, an area\'s own switch over it', () => {
    assert.equal(areaOnFrom(farmsOf({}), 'grain'), null, 'no schedule by default');
    assert.match(scheduleText(farmsOf({})), /No schedule/);
    const c = farmsOf({start: 10, every: 30});
    assert.equal(areaOnFrom(c, 'grain'), 10);
    assert.equal(areaOnFrom(c, 'meat'), 70, 'the third area, two steps of 30 days on');
    assert.equal(areaOnFrom(c, 'salt'), null, 'not in the order: never by the schedule');
    assert.match(scheduleText(c), /day 10 .* every 30 days: grain, fruit, meat/);
    assert.equal(areaSwitch(c, grainArea), 'schedule');
    assert.equal(areaSwitch(farmsOf({areas: {grain: {on: false}}}), grainArea), 'off');
});

test('a farms change is sent only when it changes something', () => {
    const c = farmsOf({start: 0, areas: {grain: {yieldScale: 120}}});
    assert.equal(farmChange(c, 'all', '', 'start', 0), null);
    assert.deepEqual(farmChange(c, 'all', '', 'start', 30), {set: {start: 30}});
    assert.deepEqual(farmChange(c, 'all', '', 'every', null), null, 'already the file\'s');
    assert.equal(farmChange(c, 'areas', 'grain', 'yieldScale', 120), null);
    assert.deepEqual(farmChange(c, 'areas', 'grain', 'yieldScale', null), {set: {areas: {grain: {yieldScale: null}}}});
    assert.deepEqual(farmChange(c, 'farms', 'grain_farm_1', 'hand', 11), {set: {farms: {grain_farm_1: {hand: 11}}}});
    assert.deepEqual(farmChange(c, 'all', '', 'order', ['salt', 'grain']), {set: {order: ['salt', 'grain']}});
});
