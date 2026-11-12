// Prosperity (Docs/Design/79-a-staged-economy.md, 12: the Money tab's successor, the one read model): the staged economy
// as the game server last saved it. Bands by market, the hungry and the short, would-be thieves, beggars and the dole;
// Times and their causes; the price board (the largest pressures, with causes); harvests; the week's wagons, ships and
// events; player coin made against sunk; the word on the street; the log; the towns' treasuries. And the DM's acts on it:
// a Times nudge on a market, prices reset (a market's or all), a harvest called per area, an event added or cancelled, a
// due ship stormed, a player granted or fined. Each act is queued for the game server, which applies and journals it; the
// view catches up at its next checkpoint.
import {Fragment, useCallback, useEffect, useState} from 'react';
import {dmApi, type EconomyActKind, type Me, type ProsperityView, type Target} from './api';
import {BANDS, HARVEST_WORDS, GRANT_MOST, CAUSE_MOST} from '../lib/economyContract';
import {clock, dayLabel} from '../lib/economy';
import {awaitOutcome} from '../lib/acts';

const p = (n: number | null | undefined) => n === null || n === undefined ? '—' : `${Math.round(n).toLocaleString()}p`;
const signed = (n: number) => `${n > 0 ? '+' : ''}${n}`;
const ago = (iso: string | null) => {
    if (!iso) return 'never saved';
    const s = Math.max(0, Math.round((Date.now() - new Date(iso).getTime()) / 1000));
    return s < 90 ? `${s}s ago` : s < 5400 ? `${Math.round(s / 60)} min ago` : `${Math.round(s / 3600)} h ago`;
};
const WORD_ICON: Record<string, string> = {slider: '⚖', times: '☼', harvest: '❦', ship: '⚓', event: '✦', cause: '¤'};


/** What the DM is about to do: a form's fields as typed. */
interface Draft { points: string; days: string; words: string; word: string; kind: string; day: string; hour: string; coins: string; reason: string; who: string }
const EMPTY: Draft = {points: '20', days: '7', words: '', word: 'poor', kind: '', day: '', hour: '9', coins: '10', reason: '', who: ''};

export function ProsperityTab({me, target, initial}: {me: Me; target: Target; initial?: ProsperityView}) {
    const [data, setData] = useState<ProsperityView | null>(initial ?? null);   // (`initial`: the render smoke's.)
    const [players, setPlayers] = useState<{id: string; name: string}[]>([]);
    const [problem, setProblem] = useState(''), [said, setSaid] = useState('');
    const [busy, setBusy] = useState(false);
    const [draft, setDraft] = useState<Draft>(EMPTY);
    const [open, setOpen] = useState<string>('');          // Which act's form is open ("nudge:grey", "harvest:orchards", ...).
    const canAct = me.role === 'dm' || me.role === 'admin';
    const load = useCallback(() => dmApi.prosperity(target).then(d => { setData(d); setProblem(''); })
        .catch(e => setProblem((e as Error).message)), [target]);
    useEffect(() => { void load(); const t = setInterval(load, 30000); return () => clearInterval(t); }, [load]);
    useEffect(() => {
        dmApi.players(target).then(d => setPlayers(d.characters.filter(c => !c.dead).map(c => ({id: c.id, name: c.name})))).catch(() => setPlayers([]));
    }, [target]);
    const set = (fields: Partial<Draft>) => setDraft(d => ({...d, ...fields}));
    const toggle = (key: string, fields: Partial<Draft> = {}) => { setOpen(o => (o === key ? '' : key)); setDraft({...EMPTY, ...fields}); };

    /** Queues an act, says what will be applied (as clamped), and waits a little for the game server's outcome. */
    const act = async (kind: EconomyActKind, id: string, payload: Record<string, unknown>, what: string) => {
        setBusy(true); setProblem(''); setSaid('');
        try {
            const queued = await dmApi.liveAct(target, kind, id, payload);
            setSaid(`${what}: queued (${JSON.stringify(queued.payload)}). Waiting for the game server…`);
            setOpen('');
            const done = await awaitOutcome(() => dmApi.action(target, queued.id));
            if (done.status === 'queued') setSaid(`${what}: still queued; no game server has picked it up yet.`);
            else (done.status === 'applied' ? setSaid : setProblem)(`${what}: ${done.status}. ${done.result ?? ''}`);
            await load();
        } catch (e) { setProblem((e as Error).message); }
        finally { setBusy(false); }
    };
    const whole = (s: string) => Number.isFinite(Number(s)) && s.trim() !== '' ? Math.round(Number(s)) : NaN;

    const report = data?.report ?? null;
    const markets = report?.markets ?? [];
    const marketName = (id: string) => markets.find(m => m.id === id)?.name ?? id;
    const kinds = report?.eventKinds ?? [];
    const today = data?.gameDay === null || data?.gameDay === undefined ? null : Math.floor(data.gameDay);
    const nextTwoWeeks = today === null ? [] : Array.from({length: 14}, (_, i) => today - (today % 7) + i).filter(d => d >= today);
    const locked = !canAct || busy;
    const words = (
        <label className="field"><span>Words (what people will say)</span>
            <input value={draft.words} maxLength={CAUSE_MOST} placeholder="blight at the orchards" onChange={e => set({words: e.target.value})} /></label>);

    return <div className="dm-health dm-prosperity">
        <div className="dm-sheet-bar">
            <strong>Prosperity · {target.toUpperCase()}</strong>
            <span className="meta hint">{data ? `game day ${today ?? '—'} · saved ${ago(data.writtenAt)}` : 'Loading…'}</span>
            <button onClick={() => void load()}>Refresh</button>
        </div>
        {problem && <p className="error">{problem}</p>}
        {said && <p className="publish-done">{said}</p>}
        {data?.stale && <p className="hint dm-note">This is the last save{data.writtenAt ? ` (${ago(data.writtenAt)})` : ''}: no game server has saved this world
            lately, so what's shown may be behind, and acts wait for a server to start.</p>}
        {data && !report && <p className="hint dm-note">No prosperity report in the last save yet (a server from before the staged economy, or
            one that hasn't saved since it started). The week and the players' money below are read from the save itself.</p>}
        {!canAct && <p className="hint dm-note">Your account ({me.role}) can look; acting needs a dm or admin account.</p>}
        {data && <div className="dm-health-tables">
            <div>
                {report && <p className="meta">Prosperity {signed(report.prosperity.slider)}{report.prosperity.target !== report.prosperity.slider
                    ? `, heading to ${signed(report.prosperity.target)}` : ''}{report.prosperity.cause ? ` · ${report.prosperity.cause}` : ''}
                    {report.prosperity.by ? ` (${report.prosperity.by === 'dm' ? 'the DM\'s words' : 'the seeded words'})` : ''}
                    {report.prosperity.neverHungry ? ' · never hungry' : ''}{report.prosperity.neverBroke ? ' · never broke' : ''}
                    <span className="hint"> (the slider is on the Economy tab)</span></p>}
                <h3>Markets <button disabled={locked} onClick={() => window.confirm('Reset prices in every market? They ease to ordinary over a day.') &&
                    void act('prices.reset', 'all', {}, 'Prices reset everywhere')}>Reset all prices</button></h3>
                <table className="dm-table"><thead><tr><th>Market</th><th>Times</th><th title="Households: destitute / poor / comfortable / rich (the DM's bands among them)">Bands</th>
                    <th title="Hunger 60 or more">Hungry</th><th title="Short of food today">Short</th><th title="Thefts expected today">Thieves</th>
                    <th>Beggars</th><th>Dole</th><th></th></tr></thead>
                    <tbody>{markets.map(m => <Fragment key={m.id}>
                        <tr key={m.id}><td>{m.name}</td>
                            <td title={m.times.cause}>{m.times.word}{m.times.cause ? ` · ${m.times.cause}` : ''}
                                {m.nudge && <em> · nudged {signed(m.nudge.points)} until day {Math.floor(m.nudge.until)}{m.nudge.words ? ` (${m.nudge.words})` : ''}</em>}
                                {m.resetting && <em> · prices resetting</em>}</td>
                            <td className="num">{BANDS.map(b => m.households[b] ?? 0).join(' / ')}{m.overridden ? ` (${m.overridden} set)` : ''}</td>
                            <td className="num">{m.hungry}</td><td className="num">{m.short}</td><td className="num">{m.thieves.toFixed(1)}</td>
                            <td className="num">{m.beggars}</td><td className="num">{m.dole}</td>
                            <td><button disabled={locked} onClick={() => toggle(`nudge:${m.id}`)}>Nudge</button>
                                <button disabled={locked} onClick={() => window.confirm(`Reset prices at ${m.name}?`) && void act('prices.reset', m.id, {}, `Prices reset at ${m.name}`)}>Reset prices</button>
                                <button disabled={locked} onClick={() => toggle(`event:${m.id}`, {kind: kinds[0]?.id ?? '', day: String(today ?? '')})}>Add event</button></td></tr>
                        {open === `nudge:${m.id}` && <tr key={`${m.id}-nudge`}><td colSpan={9}><div className="dm-act-form">
                            <label className="field"><span>Points (-100 hard to +100 boom; 0 ends a nudge)</span>
                                <input type="number" min={-100} max={100} value={draft.points} onChange={e => set({points: e.target.value})} /></label>
                            <label className="field"><span>Days (0 ends it)</span><input type="number" min={0} max={366} value={draft.days} onChange={e => set({days: e.target.value})} /></label>
                            {words}
                            <button className="primary" disabled={locked || Number.isNaN(whole(draft.points))} onClick={() =>
                                void act('times.nudge', m.id, {points: whole(draft.points), days: whole(draft.days) || 0, words: draft.words}, `Times nudged at ${m.name}`)}>Nudge</button>
                        </div></td></tr>}
                        {open === `event:${m.id}` && <tr key={`${m.id}-event`}><td colSpan={9}><div className="dm-act-form">
                            <label className="field"><span>Event</span><select value={draft.kind} onChange={e => set({kind: e.target.value})}>
                                {kinds.map(k => <option key={k.id} value={k.id}>{k.words || k.id}</option>)}</select></label>
                            <label className="field"><span>Day (this week or next)</span><select value={draft.day} onChange={e => set({day: e.target.value})}>
                                {nextTwoWeeks.map(d => <option key={d} value={d}>{dayLabel(d, today, report?.nextWeek ?? (today === null ? null : today - (today % 7) + 7))}</option>)}</select></label>
                            <label className="field"><span>Hour</span><input type="number" min={0} max={23} value={draft.hour} onChange={e => set({hour: e.target.value})} /></label>
                            <button className="primary" disabled={locked || !draft.kind} onClick={() =>
                                void act('event.add', m.id, {kind: draft.kind, hour: whole(draft.hour) || 0, ...(draft.day ? {day: whole(draft.day)} : {})},
                                         `An event added at ${m.name}`)}>Add the event</button>
                        </div></td></tr>}
                    </Fragment>)}</tbody></table>
                <p className="meta">Thefts today: {data.thefts}.</p>

                <h3>The price board (the largest pressures)</h3>
                <table className="dm-table"><thead><tr><th>Market</th><th>Good</th><th>Pressure</th><th>Why</th></tr></thead>
                    <tbody>{(report?.pressures ?? []).map(r => <tr key={`${r.market}:${r.good}`}><td>{marketName(r.market)}</td><td>{data.goodNames?.[r.good] ?? r.good}</td>
                        <td className="num">{r.P > 0 ? '▲' : '▼'} {Math.abs(r.P).toFixed(2)}</td><td>{r.cause}</td></tr>)}</tbody></table>

                <h3>Harvests</h3>
                <table className="dm-table"><thead><tr><th>Area</th><th>Harvest</th><th>Called</th><th></th></tr></thead>
                    <tbody>{(report?.areas ?? []).map(a => <Fragment key={a.id}>
                        <tr key={a.id}><td>{a.name}</td><td>{a.harvest}</td>
                            <td>{a.override ? `${a.override.word} until day ${Math.floor(a.override.until)}${a.override.words ? ` (${a.override.words})` : ''}` : '—'}</td>
                            <td><button disabled={locked} onClick={() => toggle(`harvest:${a.id}`, {days: ''})}>Call the harvest</button>
                                {a.override && <button disabled={locked} onClick={() => void act('harvest.override', a.id, {word: 'clear'}, `${a.name}'s harvest call cleared`)}>Clear</button>}</td></tr>
                        {open === `harvest:${a.id}` && <tr key={`${a.id}-call`}><td colSpan={4}><div className="dm-act-form">
                            <label className="field"><span>Harvest</span><select value={draft.word} onChange={e => set({word: e.target.value})}>
                                {HARVEST_WORDS.map(w => <option key={w} value={w}>{w}</option>)}</select></label>
                            <label className="field"><span>Days (empty: the rest of the season)</span>
                                <input type="number" min={1} max={366} value={draft.days} onChange={e => set({days: e.target.value})} /></label>
                            {words}
                            <button className="primary" disabled={locked} onClick={() =>
                                void act('harvest.override', a.id, {word: draft.word, words: draft.words, ...(draft.days ? {days: whole(draft.days)} : {})},
                                         `${a.name}'s harvest called ${draft.word}`)}>Call it</button>
                        </div></td></tr>}
                    </Fragment>)}</tbody></table>

                <h3>This week: wagons and carts</h3>
                <table className="dm-table"><thead><tr><th>Day</th><th>From</th><th>To</th><th>State</th><th>Goods</th></tr></thead>
                    <tbody>{data.week.wagons.map(w => <tr key={w.id}><td className="num">{w.day}</td><td>{marketName(w.from)}</td><td>{marketName(w.to)}</td>
                        <td>{w.state}</td><td>{w.goods.join(', ')}</td></tr>)}</tbody></table>
                <h3>This week: ships</h3>
                <table className="dm-table"><thead><tr><th>Day</th><th>Ship</th><th>Port</th><th>State</th><th>Brings</th><th></th></tr></thead>
                    <tbody>{data.week.ships.map(s => <tr key={s.id || s.name}><td className="num">{s.day}</td><td>{s.name}</td><td>{marketName(s.port)}</td>
                        <td>{s.state}</td><td>{s.goods.join(', ')}</td>
                        <td>{s.state === 'due' && s.id && <button disabled={locked} onClick={() => window.confirm(`Keep ${s.name} out with a storm?`) &&
                            void act('ship.storm', s.id, {}, `${s.name} stormbound`)}>Storm</button>}</td></tr>)}</tbody></table>
                <h3>This week: events</h3>
                <table className="dm-table"><thead><tr><th>Day</th><th>Market</th><th>Event</th><th>State</th><th>By</th><th></th></tr></thead>
                    <tbody>{data.week.events.map(e => <tr key={e.id}><td className="num">{e.day} {clock(e.hour)}</td>
                        <td>{marketName(e.market)}</td><td title={e.kind}>{e.words || e.kind}</td><td>{e.state}</td><td>{e.by}</td>
                        <td>{(e.state === 'due' || e.state === 'on') && <button disabled={locked} onClick={() => window.confirm(`Cancel ${e.words || e.kind}?`) &&
                            void act('event.cancel', e.id, {}, 'Event cancelled')}>Cancel</button>}</td></tr>)}</tbody></table>
            </div>
            <div>
                <h3>The word on the street</h3>
                <ul className="dm-word">{data.word.map((w, i) => <li key={i}>{WORD_ICON[w.kind] ?? '·'} {w.text}</li>)}
                    {!data.word.length && <li className="hint">Nothing anyone is talking of.</li>}</ul>

                <h3>Players' coin: made and sunk</h3>
                <table className="dm-table"><thead><tr><th>Day</th><th>Made</th><th>Sunk</th><th title="Goods at the catalogue's price">Goods in / out</th></tr></thead>
                    <tbody>{data.players.days.map(d => <tr key={d.day}><td className="num">{d.day}</td><td className="num">{p(d.made)}</td>
                        <td className="num">{p(d.sunk)}</td><td className="num">{p(d.goodsMade)} / {p(d.goodsSunk)}</td></tr>)}</tbody></table>
                <table className="dm-table"><thead><tr><th>Player</th><th>Made</th><th>Sunk</th></tr></thead>
                    <tbody>{data.players.holders.map(h => <tr key={h.holder} title={Object.entries(h.byReason).map(([r, [m, s]]) => `${r}: +${m} -${s}`).join('\n')}>
                        <td>{h.name}</td><td className="num">{p(h.made)}</td><td className="num">{p(h.sunk)}</td></tr>)}</tbody></table>
                <div className="dm-act-form">
                    <label className="field"><span>Grant or fine a player</span><select value={draft.who} disabled={locked}
                        onChange={e => setDraft(d => ({...d, who: e.target.value}))}>
                        <option value="">Choose a player…</option>{players.map(c => <option key={c.id} value={c.id}>{c.name}</option>)}</select></label>
                    <label className="field"><span>Coins (a fine below 0; at most {GRANT_MOST} either way)</span>
                        <input type="number" min={-GRANT_MOST} max={GRANT_MOST} value={draft.coins} disabled={locked} onChange={e => set({coins: e.target.value})} /></label>
                    <label className="field"><span>Reason</span><input value={draft.reason} maxLength={CAUSE_MOST} disabled={locked}
                        placeholder="for ringing the bell" onChange={e => set({reason: e.target.value})} /></label>
                    <button className="primary" disabled={locked || !draft.who || !whole(draft.coins) || !draft.reason.trim()} onClick={() => {
                        const coins = whole(draft.coins), name = players.find(c => c.id === draft.who)?.name ?? draft.who;
                        if (window.confirm(`${coins > 0 ? `Grant ${name} ${coins}p` : `Fine ${name} ${-coins}p`}: ${draft.reason}?`))
                            void act('character.grant', draft.who, {coins, reason: draft.reason}, coins > 0 ? `${name} granted` : `${name} fined`);
                    }}>{whole(draft.coins) < 0 ? 'Fine' : 'Grant'}</button>
                    <p className="hint">A grant is new money, capped a player a day by the world; a fine is never more than they carry.</p>
                </div>

                <h3>Treasuries</h3>
                {data.treasury !== null && data.treasury !== undefined && <p>The realm's treasury: {p(data.treasury)}.</p>}
                {data.towns.length ? <table className="dm-table"><tbody>{data.towns.map(t => <tr key={t.community}><td>{t.community}</td><td className="num">{p(t.treasury)}</td></tr>)}</tbody></table>
                    : <p className="hint">No town keeps a purse of its own in this world{data.treasury !== null && data.treasury !== undefined ? ': the realm\'s treasury is the only one' : ''}.</p>}

                <h3>The log (28 days)</h3>
                <ol className="dm-log">{data.log.map((e, i) => <li key={i} className={e.state ? `dm-log-${e.state}` : undefined}><small>{e.at.startsWith('game day') ? e.at : new Date(e.at).toLocaleString()}</small> {e.text}</li>)}</ol>
            </div>
        </div>}
    </div>;
}
