import {test} from 'node:test';
import assert from 'node:assert/strict';
import {causeProblem, clampProsperity, goodsShown, heldRules, kindChange, kindsOn, marketChange, neverChange, neverNow,
    percentsOf, percentsText, perWeekNow, prosperityText, ruleChange, ruleOf, RULE_WORDS, ruleValue, seasonal, seasonChange,
    seasonNow, serviceChange, serviceText, serviceValue, sharesOf, sharesText, shipsNow, shipsOf, statusChange, statusPart,
    steerChange, timesWord, wholeOf, type EconomyData} from './economy.ts';

// The view as tools/studio.py's Economy.read gives it (economyContract.ts's EconomyView), small.
const trim = {id: 'groom_and_trim', name: 'A groom and trim', business: 'barber', price: 1, every: 4};
const view = (over: Partial<EconomyData['economy']> = {}): EconomyData => ({
    supported: true, slim: true, problem: '', note: '', takesEffect: '', worldInfo: null,
    economy: {treasury: 1000, storeHerbs: 100, storeMeals: 50, needsPaused: false, seasons: {}, rules: {playerSellMost: 300},
        services: {}, overrides: {}, ...over},
    prosperity: {value: 0, cause: '', low: -100, high: 100, step: 5},
    ruleList: [{name: 'playerSellMost', low: 0, high: 5000, fallback: 400, switch: false},
        {name: 'ripple', low: 0, high: 70, fallback: 50, switch: false},
        {name: 'neverBroke', low: 0, high: 1, fallback: 0, switch: true},
        {name: 'harvestBias', low: -20, high: 20, fallback: 0, switch: false}],
    serviceList: [trim],
    seasonGroups: [{id: 'grain', name: 'grain and flour', goods: ['flour', 'bread'], price: [110, 95, 85, 115], shelf: [100, 100, 100, 100]}],
    goods: [{id: 'cheese', name: 'Cheese'}, {id: 'salt', name: 'Salt'}, {id: 'iron_bar', name: 'Iron bar'}],
    defaults: {status: {shares: [{at: -100, shares: [18, 45, 32, 5]}, {at: 0, shares: [5, 25, 60, 10]}, {at: 100, shares: [2, 12, 70, 16]}],
        caps: {rewardsADay: 200, theftADay: 20}, leanDays: 3},
        market: {events: {perWeek: 2, list: [{id: 'house_wedding', words: 'the House wedding', needs: ['city']},
            {id: 'good_catch', words: 'a good catch', needs: ['port']}], causes: {}},
            wharf: {ships: ['the Gull', 'the Heron'], never: ['cheese']}}},
});
const rule = (d: EconomyData, name: string) => d.ruleList.find(r => r.name === name)!;

test('the slider: clamped as the host clamps it, its word, and a change sent only when it changes something', () => {
    assert.deepEqual([clampProsperity(9999), clampProsperity(-9999), clampProsperity(37), clampProsperity(38), clampProsperity('abc'),
        clampProsperity(NaN)], [100, -100, 35, 40, 0, 0]);
    assert.deepEqual([timesWord(-100), timesWord(-40), timesWord(0), timesWord(35), timesWord(100)], ['hard', 'lean', 'ordinary', 'good', 'boom']);
    assert.equal(prosperityText(35), '+35 (good times)');
    const d = view();
    assert.equal(steerChange(d, 0), null);
    assert.equal(steerChange(d, 2), null, 'rounds to 0: nothing to change');
    assert.deepEqual(steerChange(d, 37), {prosperity: 35});
    assert.deepEqual(steerChange(d, 0, 'the war levy'), {prosperity: 0, cause: 'the war levy'});
    assert.deepEqual(steerChange(d, 0, undefined, true), {prosperity: 0, now: true}, 'now on its own moves the times at once');
    assert.equal(causeProblem('the Crown\'s war levy'), '');
    assert.notEqual(causeProblem('x'.repeat(81)), '');
    assert.notEqual(causeProblem('the dog days'), '');
    assert.equal(causeProblem('dogged rains'), '');
});

test('a rule is the world\'s own or the default; negative lows; prosperity is the slider\'s, not listed', () => {
    const d = view();
    assert.equal(ruleValue(d, rule(d, 'playerSellMost')), 300);
    assert.equal(ruleValue(d, rule(d, 'ripple')), 50);
    assert.equal(ruleChange(d, rule(d, 'playerSellMost'), 300), null);
    assert.deepEqual(ruleChange(d, rule(d, 'playerSellMost'), 250), {set: {playerSellMost: 250}});
    assert.deepEqual(ruleChange(d, rule(d, 'playerSellMost'), null), {set: {playerSellMost: null}}, 'back to the default');
    assert.equal(ruleChange(d, rule(d, 'ripple'), null), null, 'already the default');
    assert.equal(ruleChange(d, rule(d, 'ripple'), 50), null, 'the default typed in: nothing to change');
    assert.equal(ruleOf('', 1000), null);
    assert.equal(ruleOf('15', 1000), 15);
    assert.equal(ruleOf('1001', 1000), undefined);
    assert.equal(ruleOf('-15', 20, -20), -15, 'the harvest bias runs below 0');
    assert.equal(ruleOf('-21', 20, -20), undefined);
    assert.equal(wholeOf('-5', 20, -20), -5);
    assert.equal(wholeOf('-5', 20), null);
    assert.equal(wholeOf('25%', 90), 25);
    assert.equal(wholeOf('2.5', 90), null);
    assert.ok(!('prosperity' in RULE_WORDS), 'the slider has its own panel');
    for (const name of ['caravanRate', 'shipsPerWeek', 'eventRate', 'overrideNotices', 'wagonsOnRoad', 'harvestBias'])
        assert.ok(RULE_WORDS[name]?.unit, name);
    assert.match(RULE_WORDS.shipsPerWeek.unit, /0 means no ships even with a port/);
    assert.match(RULE_WORDS.wagonsOnRoad.unit, /wagonsMost/);
    assert.deepEqual(heldRules(view({rules: {steerLean: 150, ripple: 40, prosperity: 20}})), ['steerLean'], 'kept, not shown');
});

test('a service: market.json\'s until changed, one of its three at a time', () => {
    const d = view();
    assert.deepEqual(serviceValue(d, trim), {on: true, price: 1, every: 4});
    assert.equal(serviceChange(d, trim, 'price', 1), null);
    assert.deepEqual(serviceChange(d, trim, 'on', false), {set: {groom_and_trim: {on: false}}});
    assert.match(serviceText(d, trim), /barber, 1p, about once every 4 days .*\(market\.json's\)/);
    const changed = view({services: {groom_and_trim: {on: false, price: 2, every: 1}}});
    assert.deepEqual(serviceValue(changed, trim), {on: false, price: 2, every: 1});
    assert.match(serviceText(changed, trim), /2p, about once every 1 day a .* \(off\)$/);
});

test('the seasons: price and shelf, the world\'s or market.json\'s; four percents typed in any usual way', () => {
    assert.deepEqual(percentsOf('50 100 150 40', 0, 1000), [50, 100, 150, 40]);
    assert.deepEqual(percentsOf(' 50%, 100%, 150%, 40% ', 0, 1000), [50, 100, 150, 40]);
    assert.equal(percentsOf('50 100 150', 0, 1000), null, 'four, one a season');
    assert.equal(percentsOf('5 100 100 100', 10, 1000), null, 'a price under 10%');
    assert.equal(percentsText([50, 100, 150, 40]), '50 / 100 / 150 / 40');
    const d = view(), g = d.seasonGroups[0];
    assert.deepEqual(seasonNow(d, g), {price: [110, 95, 85, 115], shelf: [100, 100, 100, 100], changed: false});
    assert.equal(seasonChange(d, g, 'price', [110, 95, 85, 115]), null);
    assert.deepEqual(seasonChange(d, g, 'shelf', [100, 100, 140, 100]), {set: {grain: {shelf: [100, 100, 140, 100]}}});
    // A world before 0061 keeps four parts; the panel reads its price and shelf.
    const old = view({seasons: {grain: {price: [100, 90, 80, 120], shelf: [100, 100, 100, 100], supply: [1, 2, 3, 4], normal: 9} as never}});
    assert.deepEqual(seasonNow(old, g).price, [100, 90, 80, 120]);
    assert.ok(seasonNow(old, g).changed);
    const v = [10, 20, 30, 40];
    assert.equal(seasonal(v, 46), .1);
    assert.ok(Math.abs(seasonal(v, 92) - .15) < 1e-9, 'halfway between two middles');
});

test('the goods ships never carry: the world\'s list, else market.json\'s', () => {
    const d = view();
    assert.deepEqual(neverNow(d), ['cheese']);
    assert.equal(neverChange(d, 'cheese', true), null);
    assert.deepEqual(neverChange(d, 'salt', true), {neverImported: ['cheese', 'salt']});
    assert.deepEqual(neverChange(d, 'cheese', false), {neverImported: []}, 'none: ships carry everything');
    assert.deepEqual(neverNow(view({overrides: {neverImported: []}})), []);
    assert.deepEqual(goodsShown(d.goods, 'IRON').map(g => g.id), ['iron_bar']);
});

test('status.json\'s parts: the world\'s merged over the file\'s, one part set or back to the file\'s', () => {
    const d = view({overrides: {status: {caps: {theftADay: 40}, leanDays: 5}}});
    assert.deepEqual(statusPart(d, 'caps'), {rewardsADay: 200, theftADay: 40});
    assert.equal(statusPart(d, 'leanDays'), 5);
    assert.deepEqual(statusChange(d, 'leanDays', null), {status: {caps: {theftADay: 40}}});
    assert.deepEqual(statusChange(view({overrides: {status: {leanDays: 5}}}), 'leanDays', null), {status: null}, 'nothing left: the file\'s');
    assert.deepEqual(statusChange(view(), 'leanDays', 4), {status: {leanDays: 4}});
    const rows = view().defaults.status as {shares: {at: number; shares: number[]}[]};
    assert.equal(sharesText(rows.shares), '-100: 18 45 32 5\n0: 5 25 60 10\n100: 2 12 70 16');
    assert.deepEqual(sharesOf(sharesText(rows.shares)), rows.shares);
    assert.match(sharesOf('-100: 18 45 32 5\n100: 40 40 40 20') as string, /sum to 140/);
    assert.match(sharesOf('-100: 18 45 32 5\n100: 1 13 70 16') as string, /at least 2%/);
    assert.match(sharesOf('-100: 5 45 32 18\n100: 18 12 60 10') as string, /never rises/);
    assert.match(sharesOf('0: 5 25 60 10\n100: 2 12 70 16') as string, /-100 to 100/);
});

test('market.json\'s parts: the staged events on or off, the events a week, the ships\' names', () => {
    const d = view();
    assert.deepEqual(kindsOn(d), ['house_wedding', 'good_catch']);
    assert.equal(kindChange(d, 'good_catch', true), null);
    const off = kindChange(d, 'good_catch', false)!;
    assert.deepEqual(off, {market: {events: {list: ['house_wedding']}}}, 'by id: the host stores the file\'s kind whole');
    const less = view({overrides: {market: {events: {list: ['house_wedding'], perWeek: 1}}}});
    assert.deepEqual(kindsOn(less), ['house_wedding']);
    assert.deepEqual(kindChange(less, 'good_catch', true), {market: {events: {perWeek: 1}}}, 'all of the file\'s on: its list');
    const own = {id: 'harvest_fair', words: 'the harvest fair', goods: {cider: .2}};
    const mine = view({overrides: {market: {events: {list: [{...own}, {id: 'house_wedding', words: 'w'}]}}}});
    assert.deepEqual(kindsOn(mine), ['harvest_fair', 'house_wedding']);
    assert.deepEqual(kindChange(mine, 'good_catch', true), {market: {events: {list: ['house_wedding', 'good_catch', own]}}},
        'a host\'s own kind kept as the world has it');
    assert.equal(perWeekNow(d), 2);
    assert.equal(perWeekNow(less), 1);
    assert.deepEqual(marketChange(less, 'events', 'perWeek', null), {market: {events: {list: ['house_wedding']}}});
    assert.deepEqual(marketChange(view(), 'wharf', 'ships', ['the Gull']), {market: {wharf: {ships: ['the Gull']}}});
    assert.deepEqual(shipsNow(d), ['the Gull', 'the Heron']);
    assert.deepEqual(shipsOf('the Gull\n\n the Kestrel \n'), ['the Gull', 'the Kestrel']);
    assert.match(shipsOf('the Seadog') as string, /letters/);
    assert.match(shipsOf('the Gull\nthe Gull') as string, /once/);
    assert.match(shipsOf('') as string, /One to 64/);
});
