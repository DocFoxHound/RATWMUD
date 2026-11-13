// The render smoke (doc 79, Phase 9's first repair): every economy panel of the studio and the DM tool, and the
// Prosperity tab, rendered whole with react-dom/server from views shaped as the tools give them, so a panel that
// throws while rendering (a missing field, a bad map over a list) fails a test, not a host's page. Built for Node by
// scripts/render-smoke.mjs (vite, server side) and run by `npm run smoke`; no browser, no server.
import {renderToString} from 'react-dom/server';
import type {EconomyData} from './lib/economy';
import type {Me, ProsperityView} from './dm/api';
import {SteerPanel} from './studio/SteerPanel';
import {RulesPanel} from './studio/RulesPanel';
import {SeasonsPanel} from './studio/SeasonsPanel';
import {ImportsPanel} from './studio/ImportsPanel';
import {StatusPanel} from './studio/StatusPanel';
import {MarketPanel} from './studio/MarketPanel';
import {ProsperityTab} from './dm/ProsperityTab';
import {MyWorldTab} from './dm/MyWorldTab';
import type {MyWorld} from './dm/api';

const none = () => {};
const view = (slim: boolean): EconomyData => ({
    supported: true, slim, problem: '', note: 'The economy\'s settings.', takesEffect: 'At once.', worldInfo: null,
    economy: {treasury: 1000, storeHerbs: 100, storeMeals: 50, needsPaused: false,
        seasons: {grain: {price: [120, 100, 80, 130], shelf: [100, 100, 100, 100]}}, rules: {playerSellMost: 300, ripple: 40},
        services: {groom_and_trim: {on: false, price: 2, every: 10}},
        overrides: slim ? {prosperityCause: 'the long frost', neverImported: [], status: {leanDays: 4, caps: {theftADay: 10}},
            market: {events: {perWeek: 1}, wharf: {ships: ['the Gull']}}} : {}},
    prosperity: {value: -45, cause: 'the long frost', low: -100, high: 100, step: 5},
    ruleList: [{name: 'playerSellMost', low: 0, high: 5000, fallback: 400, switch: false},
        {name: 'ripple', low: 0, high: 70, fallback: 50, switch: false},
        {name: 'neverBroke', low: 0, high: 1, fallback: 0, switch: true},
        {name: 'caravanRate', low: 0, high: 300, fallback: 100, switch: false},
        {name: 'harvestBias', low: -20, high: 20, fallback: 0, switch: false}],
    serviceList: [{id: 'groom_and_trim', name: 'A groom and trim', business: 'barber', price: 1, every: 4}],
    seasonGroups: [{id: 'grain', name: 'grain and flour', goods: ['flour', 'bread'], price: [110, 95, 85, 115], shelf: [100, 100, 100, 100]}],
    goods: [{id: 'cheese', name: 'Cheese'}, {id: 'salt', name: 'Salt'}],
    defaults: {status: {shares: [{at: -100, shares: [18, 45, 32, 5]}, {at: 0, shares: [5, 25, 60, 10]}, {at: 100, shares: [2, 12, 70, 16]}],
        caps: {rewardsADay: 200, theftADay: 20, welcomeDays: 7}, leanDays: 3, pay: {keeper: 14}, street: {}},
        market: {events: {perWeek: 2, list: [{id: 'house_wedding', words: 'the House wedding', needs: ['city']},
            {id: 'good_catch', words: 'a good catch', needs: []}], causes: {}},
            wharf: {ships: ['the Gull', 'the Heron'], never: ['cheese']}}},
} as unknown as EconomyData);

const prosperity: ProsperityView = {
    target: 'dev', writtenAt: new Date().toISOString(), gameDay: 40.4, stale: false,
    report: {v: 1, day: 40, at: 40.4,
        prosperity: {slider: -10, target: -20, cause: 'the long frost', by: 'dm', neverHungry: false, neverBroke: false},
        markets: [{id: 'grey', name: 'Greyfen', times: {word: 'lean', score: -0.4, cause: 'the long frost'},
            households: {destitute: 1, poor: 3, comfortable: 6, rich: 1}, overridden: 1, hungry: 2, short: 1, thieves: 0.6,
            beggars: 1, dole: 2, nudge: {points: -30, until: 45, words: 'the frost'}, resetting: {until: 41}}],
        pressures: [{market: 'grey', good: 'bread', P: 0.5, cause: 'the long frost'}],
        areas: [{id: 'orchards', name: 'The orchards', harvest: 'poor', override: {word: 'poor', until: 60, words: 'blight'}},
            {id: 'fields', name: 'The fields', harvest: 'fair', override: null}],
        eventKinds: [{id: 'house_wedding', words: 'the House wedding', needs: []}], weekDays: [40, 41, 42, 43, 44, 45, 46, 47, 48],
        nextWeek: 42, bandOverrides: [{household: 'h:cottage', band: 'poor', until: 52, members: ['npc_x']}]},
    towns: [{community: 'greyfen', treasury: 900}], treasury: 5, goodNames: {bread: 'Trencher bread'},
    week: {wagons: [{id: 'c9', from: 'grey', to: 'port', day: 40, state: 'travelling', goods: ['salt']}],
        ships: [{id: 's3', port: 'grey', name: 'the Gull', day: 41, state: 'due', goods: ['salt']}],
        events: [{id: 'e4', market: 'grey', kind: 'house_wedding', day: 42, hour: 11.5, state: 'due', by: 'dm', words: 'the wedding'}]},
    players: {days: [{day: 40, made: 25, sunk: 4, goodsMade: 1, goodsSunk: 0}],
        holders: [{holder: 'acct-ada', name: 'Ada', made: 25, sunk: 4, byReason: {'dm grant: for the bell': [25, 0, 0, 0], 'provisions to start with': [0, 0, 12, 0]}}]},
    thefts: 1, word: [{kind: 'slider', text: 'the long frost'}],
    log: [{at: new Date().toISOString(), kind: 'character.grant', state: 'refused', text: 'dm-user: character.grant Ada: refused by the game (Over the cap)'},
        {at: 'game day 39', kind: 'ripple', text: 'bread at Greyfen dearer by 0.35'}],
};

const now = Math.floor(Date.now() / 1000);
const myWorld = (owner: boolean): MyWorld => ({
    world: 'world', name: 'Ashfall', package: '0.4.2', owner, fingerprint: '3F2A 9C10 5B7E 0D41 A6C3 22F9 8E05 77D1', wolves: 7,
    status: {key: 'world', name: 'Ashfall', kind: 'live', world: null, server: {state: 'running', by: '', at: '', note: ''}, serverRunning: true,
        release: 0, live: 'world', warning: '', hosted: true, hostServer: {up: true, reason: 'running', code: null, failures: 0, retryIn: null}},
    settings: {player_limit: 40, workers: null, cheats: 'off', editor: {enabled: true}},
    ai: {available: false, reason: 'Needs an AI key in world.toml.', today: {story: 0, roster: 0}, caps: {story: 100, roster: 20}, month: null, costPerStory: null, model: ''},
    history: [{at: now - 60, action: 'server.backup', target: 'world', detail: 'Asked for a backup.', who: 'host-k7q2#3f2a1c'}],
    signIns: [{at: now - 120, account: 'host-k7q2', detail: owner ? 'Signed in from this computer.' : 'Signed in.'}],
    backups: {at: now, last: {name: 'ratw-20261112T140300Z-by-hand', kind: 'by-hand', at: now - 3 * 3600, size: 820000, package: '0.4.2', migration: '0064', pinnedUntil: null},
        backups: [{name: 'ratw-20261112T140300Z-by-hand', kind: 'by-hand', at: now - 3 * 3600, size: 820000, package: '0.4.2', migration: '0064', pinnedUntil: null},
            {name: 'ratw-20261111T090000Z-daily', kind: 'daily', at: now - 30 * 3600, size: 810000, package: '0.4.2', migration: '0064', pinnedUntil: now + 80 * 86400}],
        pins: 1, pinCap: 5, working: null, problem: {code: 'disk', at: now, free: 1024 ** 3}, disk: {free: 1024 ** 3, used: 90 * 1024 ** 2, minFree: 2 * 1024 ** 3}},
    share: owner ? {state: 'ready', at: now - 120, started: now - 200, file: 'ashfall-2026-11-12.ratwpkg', size: 46 * 1024 ** 2, problem: null,
        files: [{name: 'ashfall-2026-11-12.ratwpkg', size: 46 * 1024 ** 2, at: now - 120}], kept: 3, thisHour: 1, perHour: 3, waiting: false, snapshot: true}
        : {state: 'working', at: now, started: now, file: null, size: null, problem: null, files: [], kept: 3, thisHour: 1, perHour: 3, waiting: false, snapshot: false},
    ...(owner ? {folder: 'C:\\Users\\Ann\\OneDrive\\ratw-world', exits: [{at: now - 600, code: 139, lines: ['the world server stopped']}]} : {}),
} as unknown as MyWorld);

export function renderAll(): Record<string, string> {
    const me = {username: 'dm-user', role: 'dm'} as unknown as Me;
    const out: Record<string, string> = {};
    for (const slim of [true, false]) {
        const d = view(slim), at = slim ? '' : ' (before 0061)';
        out['Steer' + at] = renderToString(<SteerPanel data={d} locked={false} setSteer={none} />);
        out['Rules' + at] = renderToString(<RulesPanel data={d} locked={false} setRules={none} setServices={none} setNeeds={none} />);
        out['Seasons' + at] = renderToString(<SeasonsPanel data={d} locked={false} setSeasons={none} />);
        out['Imports' + at] = renderToString(<ImportsPanel data={d} locked={false} setImports={none} />);
        out['Status' + at] = renderToString(<StatusPanel data={d} locked={false} setStatus={none} />);
        out['Market' + at] = renderToString(<MarketPanel data={d} locked={false} setMarket={none} />);
    }
    out['Prosperity'] = renderToString(<ProsperityTab me={me} target="dev" initial={prosperity} />);
    // (A world with no town purses of its own, as Ridgemere 2.0: the realm's treasury, and a word why the table is empty.)
    out['Prosperity (no town purses)'] = renderToString(<ProsperityTab me={me} target="dev" initial={{...prosperity, towns: [], treasury: 1200}} />);
    out['My world (owner)'] = renderToString(<MyWorldTab me={{username: 'host-k7q2#3f2a1c', role: 'owner', hosted: true} as Me} initial={myWorld(true)} />);
    out['My world (dm)'] = renderToString(<MyWorldTab me={{username: 'pell#3f2a1c', role: 'dm', hosted: true} as Me} initial={myWorld(false)} />);
    // (Doc 78, M9: Share my world refused on a nearly full disk.)
    out['My world (share failed)'] = renderToString(<MyWorldTab me={{username: 'host-k7q2#3f2a1c', role: 'owner', hosted: true} as Me}
        initial={{...myWorld(true), share: {state: 'failed', at: now, started: now, file: null, size: null, kept: 3, thisHour: 1, perHour: 3,
            waiting: false, snapshot: false, files: [], problem: {code: 'disk', free: 300 * 1024 ** 2, need: 1280 * 1024 ** 2, where: 'folder'}}}} />);
    return out;
}
