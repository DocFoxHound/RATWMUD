// Atlas's Economy page (Docs/Design/79-a-staged-economy.md, 12 and 14; tools/studio.py /api/atlas/economy): on Atlas's
// world, the slider and its cause, the rules with the residents' needs and the city's services, the seasons, and (with
// migration 0061) the goods ships never carry, status.json's bands and street, and market.json's staged events and ships.
// Per world (live.economy): Update from live and Import from dev never carry them. A change reaches a game server
// running the world at once (economy.sync), and any server started on it later. The DM's Economy tab embeds the same
// panels on the DM's world.
import {useCallback, useEffect, useState} from 'react';
import {api} from '../lib/api';
import type {Me} from '../dm/api';
import type {EconomyData, EconomyReply} from '../lib/economy';
import {ImportsPanel} from './ImportsPanel';
import {MarketPanel} from './MarketPanel';
import {RulesPanel} from './RulesPanel';
import {SeasonsPanel} from './SeasonsPanel';
import {StatusPanel} from './StatusPanel';
import {SteerPanel} from './SteerPanel';

type WorldInfo = {key: string; name: string; kind?: string; upstream?: string};

export function EconomyControls({me}: {me: Me}) {
    const [data, setData] = useState<EconomyData | null>(null);
    const [problem, setProblem] = useState(''), [said, setSaid] = useState('');
    const [busy, setBusy] = useState(false);
    const canChange = me.role === 'dm' || me.role === 'admin';
    const load = useCallback(async () => {
        try { setData(await api.economy.read()); setProblem(''); } catch (error) { setProblem((error as Error).message); }
    }, []);
    useEffect(() => { void load(); }, [load]);

    const change = async (what: () => Promise<EconomyReply>) => {
        setBusy(true); setProblem(''); setSaid('');
        try {
            const next = await what();
            setData(next as EconomyData);
            setSaid(next.changed === false ? 'Nothing to change.' : `${next.detail || 'Changed.'} ${next.takesEffect}`);
            if (next.warnings?.length) setProblem(next.warnings.join(' '));
        } catch (error) { setProblem((error as Error).message); await load(); }
        finally { setBusy(false); }
    };

    if (!data) return <div className="dm-center"><p className="hint">{problem || 'Reading the economy…'}</p></div>;
    const locked = !canChange || busy;
    const world = data.worldInfo as WorldInfo | null;
    const props = {data, locked};
    return <div className="sync-page economy-page">
        <div className="dm-sheet-bar">
            <strong>Economy</strong>
            <span className="meta hint">{world?.name ?? data.world} ({world?.key ?? data.world}){data.slim ? '' : ' · before migration 0061'}</span>
            <button disabled={busy} onClick={() => void load()}>Read again</button>
        </div>
        <div className="sync-body">
            <p className="hint">The staged economy (Docs/Design/79-a-staged-economy.md): shops set out their own wares each morning,
                goods their market doesn't make come in by wagon, cart or ship, and each household's band says how it lives.
                {' '}{data.note}</p>
            {!data.supported && <p className="publish-error">{data.problem}</p>}
            {!canChange && <p className="hint">Your account ({me.role}) can look; changing these needs a dm or admin account.</p>}
            {said && <p className="publish-done">{said}</p>}
            {problem && <p className="publish-error">{problem}</p>}
            <SteerPanel {...props} setSteer={body => void change(() => api.economy.steer(body))} />
            <RulesPanel {...props} setRules={body => void change(() => api.economy.rules(body))}
                setServices={body => void change(() => api.economy.services(body))}
                setNeeds={body => void change(() => api.economy.needs(body))} />
            <SeasonsPanel {...props} setSeasons={body => void change(() => api.economy.seasons(body))} />
            <ImportsPanel {...props} setImports={body => void change(() => api.economy.imports(body))} />
            <StatusPanel {...props} setStatus={body => void change(() => api.economy.status(body))} />
            <MarketPanel {...props} setMarket={body => void change(() => api.economy.market(body))} />
        </div>
    </div>;
}
