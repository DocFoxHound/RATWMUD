// The DM's Economy tab (Docs/Design/79-a-staged-economy.md, 12; doc 78's tab table): the staged economy's levers on the
// DM's world, the same panels as Atlas's Economy page (tools/studio.py's /api/dm/economy routes): the prosperity slider
// and its cause; the residents' needs, the rules and the services; the seasons' prices and shelves; and, on a world at
// migration 0061, the goods ships never carry and the host's own status.json and market.json values. Each change is the
// world's own (live.economy), audited in its DM audit and sent to its running game server (economy.sync), which applies
// it at once; a server started later reads it too. The day-to-day acts (a Times nudge, a harvest called, an event, a
// storm, prices reset, a grant or fine) are the Prosperity tab's. The store room went with the staged economy (doc 79,
// 17): restocking is free.
import {useCallback, useEffect, useState, type ComponentProps, type JSXElementConstructor} from 'react';
import {atLeast, dmApi, type Me, type Target, worldLabel} from './api';
import {actorName, hosted} from '../lib/hosted';
import {syncWords, type EconomyReply} from '../lib/economyContract';
import {awaitSync} from '../lib/acts';
import {SteerPanel} from '../studio/SteerPanel';
import {RulesPanel} from '../studio/RulesPanel';
import {SeasonsPanel} from '../studio/SeasonsPanel';
import {ImportsPanel} from '../studio/ImportsPanel';
import {StatusPanel} from '../studio/StatusPanel';
import {MarketPanel} from '../studio/MarketPanel';

type Setter<P extends JSXElementConstructor<any>, K extends keyof ComponentProps<P>> = ComponentProps<P>[K];
/** The view as the panels read it: the contract's EconomyView (with whatever the studio adds to it). */
type PanelData = ComponentProps<typeof RulesPanel>['data'];

export function EconomyTab({me, target}: {me: Me; target: Target}) {
    const [data, setData] = useState<PanelData | null>(null);
    const [problem, setProblem] = useState(''), [said, setSaid] = useState('');
    const [busy, setBusy] = useState(false);
    const canChange = atLeast(me, 'dm');
    const load = useCallback(async () => {
        try { setData(await dmApi.economy.get(target) as PanelData); setProblem(''); } catch (error) { setProblem((error as Error).message); }
    }, [target]);
    useEffect(() => { void load(); }, [load]);

    const change = async (what: () => Promise<EconomyReply>) => {
        setBusy(true); setProblem(''); setSaid('');
        try {
            const next = await what();
            setData(next as PanelData);
            setSaid(next.changed === false ? 'Nothing to change.'
                : hosted ? `${next.detail || 'Changed.'} Saved: the world uses it within a few seconds.` : `${next.detail || 'Changed.'} ${next.takesEffect}`);
            if (next.warnings?.length) setProblem(next.warnings.join(' '));
            // (What the game server makes of it, read back without holding the page: applied with what it skipped,
            // refused, or never taken while no server runs. Phase 9's last repair.)
            if (next.sync !== null && next.sync !== undefined)
                void awaitSync(next.sync, () => dmApi.economy.get(target)).then(last => { if (last) setData(d => d && {...d, lastSync: last}); });
        } catch (error) { setProblem((error as Error).message); await load(); }
        finally { setBusy(false); }
    };
    // (Each body is the panel's own, sent as it is: the studio checks it.)
    const setSteer: Setter<typeof SteerPanel, 'setSteer'> = body => void change(() => dmApi.economy.steer(target, body));
    const setNeeds: Setter<typeof RulesPanel, 'setNeeds'> = body => void change(() => dmApi.economy.needs(target, body));
    const setRules: Setter<typeof RulesPanel, 'setRules'> = body => void change(() => dmApi.economy.rules(target, body));
    const setServices: Setter<typeof RulesPanel, 'setServices'> = body => void change(() => dmApi.economy.services(target, body));
    const setSeasons: Setter<typeof SeasonsPanel, 'setSeasons'> = body => void change(() => dmApi.economy.seasons(target, body));
    const setImports: Setter<typeof ImportsPanel, 'setImports'> = body => void change(() => dmApi.economy.imports(target, body));
    const setStatus: Setter<typeof StatusPanel, 'setStatus'> = body => void change(() => dmApi.economy.status(target, body));
    const setMarket: Setter<typeof MarketPanel, 'setMarket'> = body => void change(() => dmApi.economy.market(target, body));

    if (!data) return <div className="dm-center"><p className="hint">{problem || 'Reading the economy\'s levers…'}</p></div>;
    const locked = !canChange || busy || !data.supported;
    const info = data.worldInfo as {name?: string; key?: string} | null;
    return <div className="sync-page economy-page dm-economy">
        <div className="dm-sheet-bar">
            <strong>Economy · {hosted ? worldLabel(target) : info?.name ?? target}</strong>
            <span className="meta hint">{hosted ? 'how the world\'s trade and times run' : `the staged economy's levers (${info?.key ?? target})`}</span>
            <button disabled={busy} onClick={() => void load()}>Read again</button>
        </div>
        <div className="sync-body">
            {hosted ? <p className="hint">Each change is saved in the world, noted in its history, and sent to its game server, which
                uses it within a few seconds; a server started later reads it too.</p>
                : <p className="hint">Each change is the world's own (live.economy), audited, and sent to its running game server, which
                applies it at once. {data.takesEffect} {data.note}</p>}
            {!canChange && !hosted && <p className="hint">Your account ({me.role}) can look; changing these needs a dm account or above.</p>}
            {!data.supported && <p className="publish-error">{data.problem}</p>}
            {said && <p className="publish-done">{said}</p>}
            {problem && <p className="publish-error">{problem}</p>}
            {data.lastSync && <p className={data.lastSync.status === 'applied' && !/skipped|held/.test(data.lastSync.result) ? 'hint' : 'publish-error'}>
                {hosted ? syncWords(data.lastSync, by => actorName(by), false) : syncWords(data.lastSync)}</p>}
            <SteerPanel data={data} locked={locked} setSteer={setSteer} />
            <RulesPanel data={data} locked={locked} setRules={setRules} setServices={setServices} setNeeds={setNeeds} />
            <SeasonsPanel data={data} locked={locked} setSeasons={setSeasons} />
            {data.slim ? <>
                <ImportsPanel data={data} locked={locked} setImports={setImports} />
                <StatusPanel data={data} locked={locked} setStatus={setStatus} />
                <MarketPanel data={data} locked={locked} setMarket={setMarket} />
            </> : <p className="hint">{hosted ? 'The goods ships never carry and this world\'s own band and event settings need a newer ' +
                'package: update it (keep world.toml, .env and backups); until then it runs on the defaults.'
                : 'The goods ships never carry and this world\'s own status.json and market.json values need migration 0061 on this ' +
                'world; until then it runs on the shipped ones.'}</p>}
        </div>
    </div>;
}
